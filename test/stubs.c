#include "hardstress.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

// Control variables for the calloc failure simulation
static bool g_calloc_will_fail = false;
static int g_calloc_fail_countdown = -1;

void set_calloc_will_fail(bool fail) {
    g_calloc_will_fail = fail;
    g_calloc_fail_countdown = -1;
}

void set_calloc_fail_countdown(int count) {
    g_calloc_will_fail = true;
    g_calloc_fail_countdown = count;
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    if (g_calloc_will_fail) {
        if (g_calloc_fail_countdown > 0) {
            g_calloc_fail_countdown--;
        }
        if (g_calloc_fail_countdown == 0) {
            return NULL;
        }
    }
    return calloc(nmemb, size);
}

void gui_log(AppContext *app, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void gui_update_stopped(gpointer user_data) {
    // Stub
}
