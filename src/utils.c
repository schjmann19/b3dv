#include "utils.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #define PATH_SEPARATOR "\\"
#else
    #ifdef __unix__
        #include <unistd.h>
    #endif
    #define PATH_SEPARATOR "/"
#endif

// Get current process memory usage in MB
int get_process_memory_mb(void)
{
#ifdef _WIN32
    // Windows implementation using PSAPI
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE process = GetCurrentProcess();

    if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
        return (int)(pmc.WorkingSetSize / (1024 * 1024));  // Convert bytes to MB
    }
    return 0;
#elif defined(__unix__) && defined(__linux__)
    // Linux implementation using /proc/self/status
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return 0;

    int memory_mb = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "VmRSS: %d", &memory_mb) == 1) {
            fclose(f);
            return memory_mb / 1024;  // Convert KB to MB
        }
    }
    fclose(f);
    return 0;
#elif defined(__APPLE__)
    // macOS implementation (basic fallback)
    return 0;  // Would need mach/mach.h for proper implementation
#else
    // Generic fallback for other systems
    return 0;
#endif
}

// Generate timestamped screenshot filename
void get_screenshot_filename(char* buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    strftime(buffer, size, "screenshot_%Y-%m-%d_%H-%M-%S.png", timeinfo);
}
