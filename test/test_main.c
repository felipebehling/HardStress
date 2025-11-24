#include <stdio.h>
#include <assert.h>
#include "hardstress.h"
#include "core.h"
#include "metrics.h"
#include "utils.h"
#include <stdbool.h>

// Declaração antecipada de funções de teste
void test_detect_cpu_count();
void test_get_total_system_memory();
void test_now_sec();
void test_splitmix64();
void test_shuffle32();
void test_shuffle32_null_robustness();
void test_controller_thread_alloc_fail();
void test_shuffle_bias();
void test_time_now_sec();

// Declaração antecipada de funções de stubs
void set_calloc_will_fail(bool fail);
void set_calloc_fail_countdown(int count);

/**
 * @brief Função principal para execução dos testes.
 */
int main() {
    printf("Running tests...\n");

    test_detect_cpu_count();
    test_get_total_system_memory();
    test_now_sec();
    test_time_now_sec();
    test_splitmix64();
    test_shuffle32();
    test_shuffle32_null_robustness();
    test_controller_thread_alloc_fail();
    test_shuffle_bias();

    printf("\nAll tests passed!\n");
    return 0;
}

/**
 * @brief Testa a função de detecção de contagem de CPUs.
 */
void test_detect_cpu_count() {
    printf("\n- Running test_detect_cpu_count...\n");
    int cpu_count = detect_cpu_count();
    printf("  - Detected %d CPU(s)\n", cpu_count);
    assert(cpu_count > 0);
    printf("  - PASSED: cpu_count is greater than 0.\n");
}

/**
 * @brief Testa a função de obter a memória total do sistema.
 */
void test_get_total_system_memory() {
    printf("\n- Running test_get_total_system_memory...\n");
    unsigned long long total_mem = get_total_system_memory();
    printf("  - Detected %llu MB of total system memory\n", total_mem / (1024 * 1024));
    assert(total_mem > 0);
    printf("  - PASSED: total_mem is greater than 0.\n");
}

/**
 * @brief Testa a função de obter o tempo atual.
 */
void test_now_sec() {
    printf("\n- Running test_now_sec...\n");
    double time1 = now_sec();
    printf("  - Initial time: %f\n", time1);

    // Sleep for a short duration
#ifdef _WIN32
    Sleep(10); // 10 milliseconds
#else
    usleep(10000); // 10000 microseconds = 10 milliseconds
#endif

    double time2 = now_sec();
    printf("  - Time after delay: %f\n", time2);

    assert(time2 > time1);
    printf("  - PASSED: now_sec is monotonic and increasing.\n");
}

/**
 * @brief Testa a função do gerador de números aleatórios splitmix64.
 */
void test_splitmix64() {
    printf("\n- Running test_splitmix64...\n");
    uint64_t seed = 12345;
    uint64_t val1 = splitmix64(&seed);
    uint64_t val2 = splitmix64(&seed);
    printf("  - Generated values: %llu, %llu\n", (unsigned long long)val1, (unsigned long long)val2);
    assert(val1 != val2);
    printf("  - PASSED: Subsequent values are different.\n");

    seed = 12345; // Reset seed
    uint64_t val3 = splitmix64(&seed);
    assert(val1 == val3);
    printf("  - PASSED: Same seed produces the same value.\n");
}

/**
 * @brief Testa a função shuffle32.
 */
void test_shuffle32() {
    printf("\n- Running test_shuffle32...\n");
    uint32_t arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint32_t arr_copy[10];
    memcpy(arr_copy, arr, sizeof(arr));

    uint64_t seed = 67890;
    shuffle32(arr, 10, &seed);

    int different = 0;
    for (int i = 0; i < 10; i++) {
        if (arr[i] != arr_copy[i]) {
            different = 1;
            break;
        }
    }
    assert(different == 1);
    printf("  - PASSED: Array is shuffled.\n");

    // Check if the shuffled array contains all original elements
    int found_count = 0;
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            if (arr_copy[i] == arr[j]) {
                found_count++;
                break;
            }
        }
    }
    assert(found_count == 10);
    printf("  - PASSED: Shuffled array contains all original elements.\n");
}

/**
 * @brief Testa a robustez da função shuffle32 com ponteiros nulos.
 */
void test_shuffle32_null_robustness() {
    printf("\n- Running test_shuffle32_null_robustness...\n");
    uint64_t seed = 123;
    // This call should not crash the program.
    // The function should gracefully handle the NULL pointer.
    shuffle32(NULL, 10, &seed);
    shuffle32(NULL, 0, &seed);
    shuffle32(NULL, 1, &seed);
    printf("  - PASSED: shuffle32 handled NULL pointer without crashing.\n");
}

/**
 * @brief Testa a falha de alocação na thread controladora.
 */
void test_controller_thread_alloc_fail() {
    printf("\n- Running test_controller_thread_alloc_fail...\n");
    AppContext app = {0};
    app.threads = 4;
    app.duration_sec = 1; // Run for a short time

    // Simulate failure on the 3rd allocation inside the loop
    set_calloc_fail_countdown(3);

    // This should not crash
    controller_thread_func(&app);

    printf("  - PASSED: controller_thread_func handled allocation failure without crashing.\n");

    // Reset calloc behavior for subsequent tests
    set_calloc_will_fail(false);
}
