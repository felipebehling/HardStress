#include "utils.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

void test_time_now_sec(void) {
    printf("Testing now_sec()... ");
    double start_time = now_sec();
    assert(start_time > 0);
    // Sleep for a short duration to ensure time progresses
    #ifdef _WIN32
    Sleep(10); // Sleep for 10 milliseconds on Windows
    #else
    usleep(10000); // Sleep for 10000 microseconds (10 ms) on other systems
    #endif
    double end_time = now_sec();
    assert(end_time > start_time);
    printf("ok\n");
}
