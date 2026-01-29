#include "utils.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

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
