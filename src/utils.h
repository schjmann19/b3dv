#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

int get_process_memory_mb(void);
void get_screenshot_filename(char* buffer, size_t size);
void get_cpu_model(char* buffer, size_t size);
void get_gpu_model(char* buffer, size_t size);
void get_kernel_info(char* buffer, size_t size);
bool get_chat_history_line(int lines_back, char* out_line, size_t max_len);
void trim_string(char* str);

#endif
