#include "utils.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

/**
 * @brief Testa a monotonicidade e validade básica da função now_sec.
 */
void test_time_now_sec(void) {
    printf("Testing now_sec()... ");
    double start_time = now_sec();
    assert(start_time > 0);
    // Dorme por uma curta duração para garantir que o tempo progrida
    #ifdef _WIN32
    Sleep(10); // Dorme por 10 milissegundos no Windows
    #else
    usleep(10000); // Dorme por 10000 microssegundos (10 ms) em outros sistemas
    #endif
    double end_time = now_sec();
    assert(end_time > start_time);
    printf("ok\n");
}
