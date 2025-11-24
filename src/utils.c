#include "utils.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdio.h>
#include <string.h>
#endif

/* --- Funções Utilitárias Independentes de Plataforma --- */

/**
 * @brief Obtém o tempo atual como um timestamp de alta resolução em segundos.
 *
 * Esta função usa um relógio monotônico para fornecer uma medição de tempo estável e de alta precisão
 * que não é afetada por mudanças no tempo do sistema.
 *
 * @return O tempo atual em segundos, com precisão de microssegundo ou melhor.
 */
#ifdef _WIN32
double now_sec(void){
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ns100 = (((uint64_t)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    // The timestamp is in 100-nanosecond intervals since January 1, 1601.
    // To convert to seconds, we first subtract the epoch difference and then
    // divide by 10^7.
    // Epoch difference (1970-1601) in 100-nanosecond intervals: 116444736000000000
    return ((double)(ns100 - 116444736000000000ULL)) / 10000000.0;
}
#else
double now_sec(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec*1e-9;
}
#endif

/**
 * @brief Um gerador de números pseudoaleatórios (PRNG) de 64 bits rápido e de alta qualidade.
 *
 * Implementa o algoritmo `splitmix64`, conhecido por sua velocidade
 * e boas propriedades estatísticas. É usado como mecanismo de semente para
 * outras operações.
 *
 * @param x Um ponteiro para a variável de estado de 64 bits para o PRNG. Este estado
 *          é atualizado a cada chamada.
 * @return O próximo número pseudoaleatório de 64 bits na sequência.
 */
uint64_t splitmix64(uint64_t *x){
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

#include <stdint.h>

/**
 * @brief Embaralha um array de inteiros de 32 bits usando o algoritmo de Fisher-Yates.
 *
 * Esta função realiza um embaralhamento in-place do array fornecido, garantindo
 * uma permutação aleatória uniforme de seus elementos.
 *
 * @param a O array de inteiros de 32 bits a ser embaralhado.
 * @param n O número de elementos no array.
 * @param seed Um ponteiro para o estado da semente de 64 bits usado pelo PRNG `splitmix64`.
 */
void shuffle32(uint32_t *a, size_t n, uint64_t *seed){
    if (a == NULL || n <= 1) return;
    for (size_t i = n - 1; i > 0; --i){
        // Use rejection sampling to avoid modulo bias.
        // This ensures that the selection of j is uniformly distributed.
        uint64_t limit = UINT64_MAX - (UINT64_MAX % (i + 1));
        uint64_t r;
        do {
            r = splitmix64(seed);
        } while (r >= limit);

        size_t j = (size_t)(r % (i + 1));
        uint32_t tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }
}

/**
 * @brief Recupera a quantidade total de RAM física no sistema.
 *
 * Esta função é multiplataforma, usando `/proc/meminfo` no Linux e
 * `GlobalMemoryStatusEx` no Windows.
 *
 * @return A memória física total em bytes. Retorna 0 em caso de falha.
 */
unsigned long long get_total_system_memory() {
#ifdef _WIN32
    // On Windows, use the GlobalMemoryStatusEx function to get memory info.
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
    return 0;
#else
    // On Linux, parse the MemTotal field from /proc/meminfo.
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo == NULL) {
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), meminfo)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            unsigned long long total_mem_kb;
            sscanf(line + 9, "%llu", &total_mem_kb);
            fclose(meminfo);
            return total_mem_kb * 1024; // Convert from KB to bytes
        }
    }
    fclose(meminfo);
    return 0;
#endif
}


/* --- Implementação da Abstração de Thread --- */

#ifdef _WIN32
// Windows-specific implementation using the Win32 API
#include <process.h> 

/**
 * @brief Cria uma nova thread (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_create` (POSIX) e `_beginthreadex` (Windows).
 *
 * @param t Ponteiro para um `thread_handle_t` onde o handle da nova thread será armazenado.
 * @param func A função que a nova thread executará.
 * @param arg O argumento a ser passado para a função da thread.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_create(thread_handle_t *t, thread_func_t func, void *arg) {
    // _beginthreadex is the recommended way to create threads on Windows for C runtime compatibility.
    *t = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    return (*t == NULL) ? -1 : 0;
}

/**
 * @brief Aguarda a finalização de uma thread específica e limpa seus recursos (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_join` (POSIX) e `WaitForSingleObject` / `CloseHandle` (Windows).
 *
 * @param t O handle da thread a ser aguardada.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_join(thread_handle_t t) {
    if(t) {
        // Wait for the thread to finish and then close the handle.
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
    return 0;
}

/**
 * @brief Desanexa uma thread, permitindo que ela execute de forma independente e tenha seus recursos liberados na finalização (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_detach` (POSIX) e `CloseHandle` (Windows).
 * Nota: No Windows, desanexar simplesmente fecha o handle, permitindo que a thread continue a ser executada,
 * mas o SO gerencia a limpeza dos recursos.
 *
 * @param t O handle da thread a ser desanexada.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_detach(thread_handle_t t) {
    // On Windows, "detaching" is achieved by simply closing the handle.
    // The thread will continue to run, and its resources will be freed by the OS on termination.
    if(t) CloseHandle(t); 
    return 0;
}
#else
// POSIX-specific implementation using pthreads

/**
 * @brief Cria uma nova thread (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_create` (POSIX) e `_beginthreadex` (Windows).
 *
 * @param t Ponteiro para um `thread_handle_t` onde o handle da nova thread será armazenado.
 * @param func A função que a nova thread executará.
 * @param arg O argumento a ser passado para a função da thread.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_create(thread_handle_t *t, thread_func_t func, void *arg) {
    return pthread_create(t, NULL, func, arg);
}

/**
 * @brief Aguarda a finalização de uma thread específica e limpa seus recursos (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_join` (POSIX) e `WaitForSingleObject` / `CloseHandle` (Windows).
 *
 * @param t O handle da thread a ser aguardada.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_join(thread_handle_t t) {
    return pthread_join(t, NULL);
}

/**
 * @brief Desanexa uma thread, permitindo que ela execute de forma independente e tenha seus recursos liberados na finalização (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_detach` (POSIX) e `CloseHandle` (Windows).
 * Nota: No Windows, desanexar simplesmente fecha o handle, permitindo que a thread continue a ser executada,
 * mas o SO gerencia a limpeza dos recursos.
 *
 * @param t O handle da thread a ser desanexada.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_detach(thread_handle_t t) {
    return pthread_detach(t);
}
#endif
