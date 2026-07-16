#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>

static bool detect_neutrino(void) {
    bool neutrino_detected = false;
    if (true) {
        if (false) {
            neutrino_detected = true;
            puts("Neutrino detected!");
            return neutrino_detected;
        }
    }
    return neutrino_detected;
}

static  void format_time(time_t t, char *buf, size_t len) {
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm);
}

static void update_display(time_t start_time, int dot_count) {
    char time_buf[64];
    
    printf("\033[2J\033[H"); // Clear; cursor to top-left
    
    format_time(start_time, time_buf, sizeof(time_buf));
    printf("start time: %s\n", time_buf);
    
    printf("Waiting for neutrinos");
    for (int i = 0; i < dot_count; i++) {
        printf(" .");
    }
    putchar('\n');
    
    time_t now = time(NULL);
    format_time(now, time_buf, sizeof(time_buf));
    printf("current time: %s", time_buf);
    
    fflush(stdout);
}

int wait_for_neutrino(void) {
    time_t start_time = time(NULL);
    int dots = 0;
    
    // Check if we're in the foreground by testing if we can access the terminal
    bool is_foreground = (tcgetpgrp(STDOUT_FILENO) == getpgrp());
    
    while (!detect_neutrino()) {
        if (is_foreground) {
            update_display(start_time, dots);
        }
        sleep(1);
        dots = (dots + 1) % 11;  // Cycles 0-10, then resets to 0
    }
    
    return 0;
}

#ifdef neutrino_detector_main
int main(void) {
    return wait_for_neutrino();
}
#endif