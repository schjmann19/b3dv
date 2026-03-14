#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#endif
#include "world.h"

// Worker thread main function - processes chunks for lighting and meshing
static void* worker_thread_main(void* arg)
{
    World* world = (World*)arg;
    WorkerQueue* queue = &world->worker_queue;

    while (true) {
        WorkerJob job = {0};
        bool has_job = false;

        // Wait for work or shutdown signal
        pthread_mutex_lock(&queue->mutex);
        while (queue->count == 0 && !queue->shutdown) {
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }

        if (queue->shutdown && queue->count == 0) {
            pthread_mutex_unlock(&queue->mutex);
            break;  // Exit thread
        }

        // Get first job from queue
        if (queue->count > 0) {
            job = queue->queue[0];
            has_job = true;
            // Shift queue
            for (int i = 0; i < queue->count - 1; i++) {
                queue->queue[i] = queue->queue[i + 1];
            }
            queue->count--;
        }
        pthread_mutex_unlock(&queue->mutex);

        if (!has_job) continue;

        // Mark job as in-progress
        pthread_mutex_lock(&queue->mutex);
        queue->jobs_in_progress++;
        pthread_mutex_unlock(&queue->mutex);

        // CRITICAL: Lock cache_mutex BEFORE looking up chunk to prevent it being unloaded
        // This prevents the chunk from being removed from the cache while we process it
        pthread_mutex_lock(&world->cache_mutex);
        Chunk* chunk = world_get_chunk(world, job.chunk_x, job.chunk_y, job.chunk_z);
        if (!chunk) {
            // Chunk was unloaded, mark job complete
            pthread_mutex_unlock(&world->cache_mutex);
            pthread_mutex_lock(&queue->mutex);
            queue->jobs_in_progress--;
            pthread_mutex_unlock(&queue->mutex);
            continue;
        }

        // Mark this chunk as in-use so it isn't unloaded while we're processing it
        __atomic_add_fetch(&chunk->in_use_count, 1, __ATOMIC_ACQ_REL);

        // Lock chunk for processing - protects this chunk's data
        pthread_mutex_lock(&chunk->mutex);

        // We can release cache_mutex now that we have chunk->mutex
        pthread_mutex_unlock(&world->cache_mutex);

        // RE-VALIDATE chunk after locking - it might have been unloaded and replaced
        // Check that coordinates still match what we queued
        if (chunk->chunk_x != job.chunk_x || chunk->chunk_y != job.chunk_y || chunk->chunk_z != job.chunk_z) {
            pthread_mutex_unlock(&chunk->mutex);
            pthread_mutex_lock(&queue->mutex);
            queue->jobs_in_progress--;
            pthread_mutex_unlock(&queue->mutex);
            continue;  // Different chunk now at this address, skip
        }

        // Validate chunk is still relevant (wasn't unloaded)
        // For save jobs, we allow saving even if the chunk is marked unloaded.
        if (!chunk->generated || (job.type != WORKER_JOB_SAVE_CHUNK && !chunk->loaded)) {
            pthread_mutex_unlock(&chunk->mutex);
            pthread_mutex_lock(&queue->mutex);
            queue->jobs_in_progress--;
            pthread_mutex_unlock(&queue->mutex);
            continue;
        }

        // Handle save job separately (async chunk saving)
        if (job.type == WORKER_JOB_SAVE_CHUNK) {
            // Save chunk to disk (same format as world_save_chunk)
            if (chunk->modified) {
                pthread_mutex_unlock(&chunk->mutex);  // release while saving to avoid long lock hold
                world_save_chunk(chunk, world->world_name);
                pthread_mutex_lock(&chunk->mutex);
                chunk->modified = false;
            }
            chunk->pending_save = false;

            // If this chunk was scheduled for unload, allow it to be removed next update
            // (world_update_chunks checks pending_unload)

            pthread_mutex_unlock(&chunk->mutex);
            pthread_mutex_lock(&queue->mutex);
            queue->jobs_in_progress--;
            pthread_mutex_unlock(&queue->mutex);
            continue;
        }

        // Skip if already processed and no updates required
        if (!chunk->needs_relighting && chunk->meshed) {
            pthread_mutex_unlock(&chunk->mutex);
            pthread_mutex_lock(&queue->mutex);
            queue->jobs_in_progress--;
            pthread_mutex_unlock(&queue->mutex);
            continue;
        }

        // NOTE: Do NOT invalidate visible_count before rebuilding!
        // Zeroing it causes a flicker where the chunk disappears for 1+ frames.
        // Instead, we render the old mesh while building the new one in chunk_cache_visible_blocks.
        // The atomic swap at the end of chunk_cache_visible_blocks ensures thread safety.

        // Save relighting requirement and release mutex BEFORE expensive calculations
        bool needs_relighting = chunk->needs_relighting && chunk->generated && chunk->loaded;
        bool needs_meshing = !chunk->meshed && chunk->generated && chunk->loaded;

        if (needs_relighting) {
            chunk->needs_relighting = false;  // Mark as being processed
        }

        pthread_mutex_unlock(&chunk->mutex);

        // Calculate lighting WITHOUT holding chunk->mutex to avoid deadlock
        // chunk_cache_visible_blocks and lighting functions call world_get_block which needs cache_mutex
        if (needs_relighting) {
             fprintf(stderr, "[worker] Computing lighting for chunk (%d,%d,%d)\n", chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);
            fflush(stderr);

            // Compute lighting into inactive buffer (so render can read the previous buffer safely)
            int active = __atomic_load_n(&chunk->active_light_buffer, __ATOMIC_ACQUIRE);
            int inactive = 1 - active;

            calculate_chunk_skylight(chunk, world, inactive);
            calculate_chunk_blocklight(chunk, world, inactive);

            // Swap the active buffer once (both skylight+blocklight now updated)
            pthread_mutex_lock(&chunk->light_swap_mutex);
            __atomic_store_n(&chunk->active_light_buffer, inactive, __ATOMIC_RELEASE);
            pthread_mutex_unlock(&chunk->light_swap_mutex);
        }

        // Cache visible blocks (mesh) - NO locks held here, safer for neighbor lookups
        if (needs_meshing) {
            fprintf(stderr, "[worker] Caching visible blocks for chunk (%d,%d,%d)\n", chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);
            fflush(stderr);

            chunk_cache_visible_blocks(chunk, world);

            fprintf(stderr, "[worker] Cached %d visible blocks for chunk (%d,%d,%d)\n", chunk->visible_count, chunk->chunk_x, chunk->chunk_y, chunk->chunk_z);
            fflush(stderr);

            // Re-acquire chunk->mutex to update meshed flag atomically
            pthread_mutex_lock(&chunk->mutex);
            chunk->meshed = true;
            pthread_mutex_unlock(&chunk->mutex);
        } else if (!chunk->meshed && chunk->generated && chunk->loaded) {
            // Chunk not ready yet, just mark it (avoid infinite loops)
            pthread_mutex_lock(&chunk->mutex);
            chunk->meshed = true;
            pthread_mutex_unlock(&chunk->mutex);
        }

        // Mark job as complete
        pthread_mutex_lock(&queue->mutex);
        queue->jobs_in_progress--;
        pthread_mutex_unlock(&queue->mutex);

        // Decrement in-use counter for this chunk
        __atomic_sub_fetch(&chunk->in_use_count, 1, __ATOMIC_ACQ_REL);
    }

    return NULL;
}

// Initialize worker thread system
void worker_init(World* world)
{
    if (!world) return;

    WorkerQueue* queue = &world->worker_queue;
    queue->capacity = 256;
    queue->count = 0;
    queue->jobs_in_progress = 0;  // No jobs in progress initially
    queue->queue = (WorkerJob*)malloc(sizeof(WorkerJob) * queue->capacity);
    queue->shutdown = false;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);

    // Start worker thread with CPU affinity optimization
    world->worker_running = true;

    // OPTIMIZATION: Set thread attributes for better performance
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);

    // Try to pin worker thread to second core (core 1) for better cache locality
    // Falls back gracefully if CPU affinity not supported
    #ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    // Pin to core 1 (reserve core 0 for main thread)
    CPU_SET(1, &cpuset);
    pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset);
    printf("[worker] Worker thread CPU affinity set to core 1\n");
    #endif

    pthread_create(&world->worker_thread, &thread_attr, worker_thread_main, (void*)world);
    pthread_attr_destroy(&thread_attr);
    printf("[worker] Worker thread started\n");
}

// Internal helper to queue a job (with deduplication)
static void worker_queue_job(World* world, WorkerJob job)
{
    if (!world) return;

    WorkerQueue* queue = &world->worker_queue;

    pthread_mutex_lock(&queue->mutex);

    // Check if job already in queue by coordinates+type
    for (int i = 0; i < queue->count; i++) {
        if (queue->queue[i].chunk_x == job.chunk_x &&
            queue->queue[i].chunk_y == job.chunk_y &&
            queue->queue[i].chunk_z == job.chunk_z &&
            queue->queue[i].type == job.type) {
            pthread_mutex_unlock(&queue->mutex);
            return;  // Already queued
        }
    }

    // Add to queue
    if (queue->count >= queue->capacity) {
        queue->capacity *= 2;
        queue->queue = (WorkerJob*)realloc(queue->queue, sizeof(WorkerJob) * queue->capacity);
    }

    queue->queue[queue->count++] = job;
    const char* type_name = (job.type == WORKER_JOB_SAVE_CHUNK) ? "save" : "light/mesh";
    printf("[worker] Queued %s job for chunk (%d,%d,%d)\n", type_name, job.chunk_x, job.chunk_y, job.chunk_z);
    pthread_cond_signal(&queue->cond);  // Wake up worker thread

    pthread_mutex_unlock(&queue->mutex);
}

// Queue a chunk for processing (lighting + meshing)
void worker_queue_chunk(World* world, Chunk* chunk)
{
    if (!world || !chunk) return;

    WorkerJob job = { .chunk_x = chunk->chunk_x,
                      .chunk_y = chunk->chunk_y,
                      .chunk_z = chunk->chunk_z,
                      .type = WORKER_JOB_LIGHTING_AND_MESH };
    worker_queue_job(world, job);
}

// Queue a chunk for asynchronous saving
void worker_queue_chunk_save(World* world, Chunk* chunk)
{
    if (!world || !chunk) return;

    // Avoid requeuing if already pending save
    pthread_mutex_lock(&chunk->mutex);
    if (chunk->pending_save) {
        pthread_mutex_unlock(&chunk->mutex);
        return;
    }
    chunk->pending_save = true;
    pthread_mutex_unlock(&chunk->mutex);

    WorkerJob job = { .chunk_x = chunk->chunk_x,
                      .chunk_y = chunk->chunk_y,
                      .chunk_z = chunk->chunk_z,
                      .type = WORKER_JOB_SAVE_CHUNK };
    worker_queue_job(world, job);
}

// Flush the worker queue - wait for all pending jobs to complete
// This is important before world_load to avoid race conditions
void worker_flush_queue(World* world)
{
    if (!world) return;

    WorkerQueue* queue = &world->worker_queue;

    // Wait for queue to be empty AND no jobs in progress
    int wait_count = 0;
    while (true) {
        pthread_mutex_lock(&queue->mutex);
        bool queue_empty = (queue->count == 0 && queue->jobs_in_progress == 0);
        pthread_mutex_unlock(&queue->mutex);

        if (queue_empty) {
            printf("[worker] Queue flushed after %d checks\n", wait_count);
            break;
        }

        if (wait_count % 10 == 0) {
            printf("[worker] Waiting for queue... (count=%d, in_progress=%d)\n", queue->count, queue->jobs_in_progress);
        }
        wait_count++;

        struct timespec ts = {0, 10000000};  // 10ms in nanoseconds
        nanosleep(&ts, NULL);

        // Timeout after 5 seconds to prevent infinite hang
        if (wait_count > 500) {
            printf("[worker] WARNING: Queue flush timeout! (count=%d, in_progress=%d)\n", queue->count, queue->jobs_in_progress);
            break;
        }
    }
}

// Shut down worker thread cleanly
void worker_shutdown(World* world)
{
    if (!world) return;

    WorkerQueue* queue = &world->worker_queue;

    // Signal shutdown
    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = true;
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);

    // Wait for thread to exit
    printf("[worker] Waiting for worker thread to exit...\n");
    pthread_join(world->worker_thread, NULL);
    printf("[worker] Worker thread exited\n");

    // Clean up queue
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue->queue);

    world->worker_running = false;
    printf("[worker] Worker thread shut down\n");
}
