#include "hardstress.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

// Variáveis de controle para a simulação de falha do calloc
static bool g_calloc_will_fail = false;
static int g_calloc_fail_countdown = -1;

/**
 * @brief Configura se a próxima chamada para calloc deve falhar.
 * @param fail Se true, calloc retornará NULL.
 */
void set_calloc_will_fail(bool fail) {
    g_calloc_will_fail = fail;
    g_calloc_fail_countdown = -1;
}

/**
 * @brief Configura uma contagem regressiva para a falha do calloc.
 * @param count O número de chamadas bem-sucedidas antes de uma falha.
 */
void set_calloc_fail_countdown(int count) {
    g_calloc_will_fail = true;
    g_calloc_fail_countdown = count;
}

/**
 * @brief Wrapper para a função calloc para permitir a injeção de falhas nos testes.
 */
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

/**
 * @brief Stub para gui_log que imprime no stdout.
 */
void gui_log(AppContext *app, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/**
 * @brief Stub para gui_update_stopped.
 */
void gui_update_stopped(gpointer user_data) {
    // Stub
}

/**
 * @brief Stub para gui_set_temp_panel_visibility.
 */
void gui_set_temp_panel_visibility(AppContext *app, gboolean visible) {
    (void)app;
    (void)visible;
}
