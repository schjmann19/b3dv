#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <winreg.h>
    #define PATH_SEPARATOR "\\"
#else
    #ifdef __unix__
        #include <unistd.h>
    #endif
    #define PATH_SEPARATOR "/"
#endif

#include "utils.h"

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

// Get CPU model name
void get_cpu_model(char* buffer, size_t size)
{
#ifdef __linux__
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    colon++;
                    // Skip leading whitespace
                    while (*colon == ' ') colon++;
                    // Remove trailing newline
                    char* newline = strchr(colon, '\n');
                    if (newline) *newline = '\0';
                    snprintf(buffer, size, "CPU: %s", colon);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }
    snprintf(buffer, size, "CPU: Unknown");
#elif defined(_WIN32)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char cpu_name[128];
        DWORD size_read = sizeof(cpu_name);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpu_name, &size_read) == ERROR_SUCCESS) {
            snprintf(buffer, size, "CPU: %s", cpu_name);
        } else {
            snprintf(buffer, size, "CPU: Unknown");
        }
        RegCloseKey(hKey);
    } else {
        snprintf(buffer, size, "CPU: Unknown");
    }
#else
    snprintf(buffer, size, "CPU: Unknown");
#endif
}

// Get GPU model name
void get_gpu_model(char* buffer, size_t size)
{
#ifdef __linux__
    FILE* f = popen("glxinfo -B 2>/dev/null | grep -i 'OpenGL renderer' | head -1", "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f)) {
            // Extract GPU name after "OpenGL renderer string: "
            char* gpu_name = strstr(line, ":");
            if (gpu_name) {
                gpu_name++;
                while (*gpu_name == ' ') gpu_name++;
                char* newline = strchr(gpu_name, '\n');
                if (newline) *newline = '\0';
                snprintf(buffer, size, "GPU: %s", gpu_name);
                pclose(f);
                return;
            }
        }
        pclose(f);
    }
    snprintf(buffer, size, "GPU: Unknown");
#elif defined(_WIN32)
    // Try multiple registry paths for better compatibility
    const char* paths[] = {
        "HARDWARE\\DEVICEMAP\\VIDEO\\Device0",
        "SYSTEM\\ControlSet001\\Services\\nvlddmkm\\Device0",
        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, paths[i], 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char gpu_name[256];
            DWORD size_read = sizeof(gpu_name);

            // Try different value names
            const char* values[] = {"Device Description", "DriverDesc", NULL};

            for (int j = 0; values[j] != NULL; j++) {
                if (RegQueryValueExA(hKey, values[j], NULL, NULL, (LPBYTE)gpu_name, &size_read) == ERROR_SUCCESS) {
                    snprintf(buffer, size, "GPU: %s", gpu_name);
                    RegCloseKey(hKey);
                    return;
                }
            }
            RegCloseKey(hKey);
        }
    }

    // Fallback: try the primary display device path
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
    }
    snprintf(buffer, size, "GPU: Unknown");
#else
    snprintf(buffer, size, "GPU: Unknown");
#endif
}

// Get kernel info
void get_kernel_info(char* buffer, size_t size)
{
#ifdef __linux__
    FILE* f = popen("uname -r", "r");
    if (f) {
        char kernel[128];
        if (fgets(kernel, sizeof(kernel), f)) {
            char* newline = strchr(kernel, '\n');
            if (newline) *newline = '\0';
            snprintf(buffer, size, "Kernel: %s", kernel);
            pclose(f);
            return;
        }
        pclose(f);
    }
    snprintf(buffer, size, "Kernel: Unknown");
#elif defined(_WIN32)
    OSVERSIONINFOA osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    if (GetVersionExA(&osvi)) {
        snprintf(buffer, size, "Kernel: Windows %lu.%lu", osvi.dwMajorVersion, osvi.dwMinorVersion);
    } else {
        snprintf(buffer, size, "Kernel: Unknown");
    }
#else
    snprintf(buffer, size, "Kernel: Unknown");
#endif
}

// Get the Nth-from-last line from chathistory file (1 = last line, 2 = second-to-last, etc.)
// Returns false if there are fewer than N lines in the file
bool get_chat_history_line(int lines_back, char* out_line, size_t max_len)
{
    out_line[0] = '\0';

    if (lines_back < 1) return false;

    FILE* file = fopen("./chathistory", "r");
    if (!file) return false;

    // Read all lines into a dynamic array
    char** lines = NULL;
    int line_count = 0;
    int capacity = 0;
    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Skip empty lines
        if (line[0] == '\0') continue;

        // Resize array if needed
        if (line_count >= capacity) {
            capacity = (capacity == 0) ? 10 : capacity * 2;
            char** new_lines = (char**)realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
                // Error - free and return
                for (int i = 0; i < line_count; i++) free(lines[i]);
                free(lines);
                fclose(file);
                return false;
            }
            lines = new_lines;
        }

        // Add line
        lines[line_count] = (char*)malloc(strlen(line) + 1);
        if (!lines[line_count]) {
            for (int i = 0; i < line_count; i++) free(lines[i]);
            free(lines);
            fclose(file);
            return false;
        }
        strcpy(lines[line_count], line);
        line_count++;
    }

    fclose(file);

    // Get the Nth-from-last line
    if (lines_back > line_count) {
        // Not enough lines
        for (int i = 0; i < line_count; i++) free(lines[i]);
        free(lines);
        return false;
    }

    int index = line_count - lines_back;
    strncpy(out_line, lines[index], max_len - 1);
    out_line[max_len - 1] = '\0';

    // Free memory
    for (int i = 0; i < line_count; i++) free(lines[i]);
    free(lines);

    return true;
}

// Trim whitespace from both ends of a string
void trim_string(char* str)
{
    if (!str || str[0] == '\0') return;

    // Trim trailing whitespace
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
                       str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }

    // Trim leading whitespace
    int start = 0;
    while (str[start] && (str[start] == ' ' || str[start] == '\t')) {
        start++;
    }

    if (start > 0) {
        memmove(str, str + start, strlen(str + start) + 1);
    }
}
