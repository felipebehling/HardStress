#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "../src/utils.h"

#define N_ELEMENTS 3
#define N_ITERATIONS 100000

/**
 * @brief Testa a presença de viés estatístico na função shuffle32.
 *
 * Executa o embaralhamento um grande número de vezes e verifica se cada elemento
 * aparece em cada posição com uma frequência aproximadamente uniforme.
 */
void test_shuffle_bias() {
    uint32_t a[N_ELEMENTS];
    uint64_t seed = 12345;
    int counts[N_ELEMENTS][N_ELEMENTS] = {0};

    for (int i = 0; i < N_ITERATIONS; ++i) {
        for (uint32_t j = 0; j < N_ELEMENTS; ++j) {
            a[j] = j;
        }
        shuffle32(a, N_ELEMENTS, &seed);
        for (int j = 0; j < N_ELEMENTS; ++j) {
            counts[j][a[j]]++;
        }
    }

    double expected = (double)N_ITERATIONS / N_ELEMENTS;
    double tolerance = expected * 0.02; // 2% tolerância

    printf("\n- Running test_shuffle_bias...\n");
    printf("  - Shuffle distribution results (expected avg: %.2f, tolerance: %.2f):\n", expected, tolerance);
    for (int i = 0; i < N_ELEMENTS; i++) {
        for (int j = 0; j < N_ELEMENTS; j++) {
            printf("    - Element %d at index %d: %d times (diff: %.2f)\n", j, i, counts[i][j], fabs(counts[i][j] - expected));
            assert(fabs(counts[i][j] - expected) < tolerance);
        }
    }
    printf("  - PASSED: Shuffle distribution is within tolerance.\n");
}
