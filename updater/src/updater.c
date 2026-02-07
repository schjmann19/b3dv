#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define GITHUB_RAW "https://raw.githubusercontent.com/schjmann19/b3dv/main"
#define BUFFER_SIZE 4096

#ifdef _WIN32
#define BINARY_NAME "b3dv.exe"
#else
#define BINARY_NAME "b3dv"
#endif

/* Simple helper to check if command exists */
int command_exists(const char *cmd) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "command -v %s >/dev/null 2>&1", cmd);
    return system(buffer) == 0;
}

/* Calculate SHA256 hash of a file */
int calculate_sha256(const char *filename, char *hash_output) {
    char cmd[512];
    FILE *fp;
    char buffer[256];

    printf("Calculating SHA256 hash...\n");

    if (command_exists("sha256sum")) {
        snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", filename);
    } else if (command_exists("shasum")) {
        snprintf(cmd, sizeof(cmd), "shasum -a 256 '%s' 2>/dev/null", filename);
    } else {
        fprintf(stderr, "Error: sha256sum or shasum not found\n");
        return -1;
    }

    fp = popen(cmd, "r");
    if (!fp) {
        perror("popen");
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fprintf(stderr, "Error: Failed to calculate hash\n");
        pclose(fp);
        return -1;
    }
    pclose(fp);

    if (strlen(buffer) < 64) {
        fprintf(stderr, "Error: Invalid hash output\n");
        return -1;
    }

    strncpy(hash_output, buffer, 64);
    hash_output[64] = '\0';

    return 0;
}

/* Download a file from URL */
int download_file(const char *url, const char *filename) {
    char cmd[2048];

    if (command_exists("curl")) {
        snprintf(cmd, sizeof(cmd), "curl -s -L -o '%s' '%s'", filename, url);
    } else if (command_exists("wget")) {
        snprintf(cmd, sizeof(cmd), "wget -q -O '%s' '%s'", filename, url);
    } else {
        fprintf(stderr, "Error: Neither curl nor wget found\n");
        return -1;
    }

    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to download from %s\n", url);
        return -1;
    }

    return 0;
}

/* Parse SHA256SUMS file to find hash for our binary */
int parse_checksum_file(const char *filename, const char *binary_name, char *hash_output) {
    FILE *fp = fopen(filename, "r");
    char line[512];

    if (!fp) {
        perror("fopen");
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *space = strchr(line, ' ');
        if (space) {
            while (*space == ' ') space++;

            char *newline = strchr(line, '\n');
            if (newline) *newline = '\0';

            if (strcmp(space, binary_name) == 0) {
                strncpy(hash_output, line, 64);
                hash_output[64] = '\0';
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);

    fprintf(stderr, "Error: %s not found in checksum file\n", binary_name);
    return -1;
}

/* Make file executable (Unix-like systems) */
int make_executable(const char *filename) {
#ifndef _WIN32
    if (chmod(filename, 0755) != 0) {
        perror("chmod");
        return -1;
    }
#endif
    return 0;
}

/* Backup old binary and replace with new one */
int install_update(const char *new_binary) {
    char backup_dir[512];
    char backup_file[512];
    char timestamp[32];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    /* Check if new binary exists and is readable */
    if (access(new_binary, R_OK) != 0) {
        fprintf(stderr, "Error: New binary file not accessible\n");
        return -1;
    }

    /* Create backups directory if it doesn't exist */
    snprintf(backup_dir, sizeof(backup_dir), "backups");
#ifndef _WIN32
    mkdir(backup_dir, 0755);
#else
    mkdir(backup_dir);
#endif

    /* Create backup filename with timestamp */
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
    snprintf(backup_file, sizeof(backup_file), "backups/%s.bak.%s", BINARY_NAME, timestamp);

    printf("Creating backup: %s\n", backup_file);

    /* Backup existing binary if it exists */
    if (access(BINARY_NAME, F_OK) == 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", BINARY_NAME, backup_file);
        if (system(cmd) != 0) {
            fprintf(stderr, "Error: Failed to create backup\n");
            return -1;
        }
    }

    /* Replace with new binary */
    printf("Installing update...\n");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mv '%s' '%s'", new_binary, BINARY_NAME);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to install update\n");
        return -1;
    }

    /* Make executable */
    if (make_executable(BINARY_NAME) != 0) {
        fprintf(stderr, "Warning: Could not set executable bit\n");
    }

    return 0;
}

int main(void) {
    char binary_url[2048];
    char checksum_url[2048];
    char temp_binary[512];
    char temp_checksum[512];
    char computed_hash[65];
    char expected_hash[65];

    printf("B3DV Updater v1\n");
    printf("================\n");
    printf("Repository: schjmann19/b3dv (main branch)\n\n");

    /* Build URLs */
    snprintf(binary_url, sizeof(binary_url),
             "%s/%s", GITHUB_RAW, BINARY_NAME);
    snprintf(checksum_url, sizeof(checksum_url),
             "%s/SHA256SUMS", GITHUB_RAW);

    printf("Downloading latest binary...\n");

    /* Download binary */
    snprintf(temp_binary, sizeof(temp_binary), "%s.tmp", BINARY_NAME);

    if (download_file(binary_url, temp_binary) != 0) {
        fprintf(stderr, "Fatal: Failed to download binary\n");
        unlink(temp_binary);
        return 1;
    }

    printf("Binary downloaded successfully.\n");

    /* Download SHA256SUMS file */
    snprintf(temp_checksum, sizeof(temp_checksum), "/tmp/SHA256SUMS.tmp");

    printf("Downloading checksums for verification...\n");
    if (download_file(checksum_url, temp_checksum) != 0) {
        fprintf(stderr, "Warning: Could not download SHA256SUMS file\n");
        fprintf(stderr, "Proceeding without checksum verification (not recommended)\n");
    } else {
        /* Parse checksum file to get expected hash */
        if (parse_checksum_file(temp_checksum, BINARY_NAME, expected_hash) != 0) {
            fprintf(stderr, "Warning: Could not find checksum for %s\n", BINARY_NAME);
            fprintf(stderr, "Proceeding without checksum verification (not recommended)\n");
        } else {
            /* Calculate hash of downloaded binary */
            if (calculate_sha256(temp_binary, computed_hash) != 0) {
                fprintf(stderr, "Fatal: Failed to calculate checksum\n");
                unlink(temp_binary);
                unlink(temp_checksum);
                return 1;
            }

            printf("Expected hash: %s\n", expected_hash);
            printf("Got hash:      %s\n", computed_hash);

            /* Compare hashes */
            if (strcmp(expected_hash, computed_hash) != 0) {
                fprintf(stderr, "Fatal: Checksum mismatch! Downloaded file may be corrupted or tampered with.\n");
                unlink(temp_binary);
                unlink(temp_checksum);
                return 1;
            }

            printf("Checksum verified!\n\n");
        }
    }

    unlink(temp_checksum);

    /* Install update */
    if (install_update(temp_binary) != 0) {
        fprintf(stderr, "Fatal: Failed to install update\n");
        unlink(temp_binary);
        return 1;
    }

    printf("Update successful!\n");
    printf("New binary: %s\n", BINARY_NAME);

    return 0;
}
