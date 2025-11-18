#ifndef UTILS_H
#define UTILS_H

/**
 * @file utils.h
 * @brief Fornece funções utilitárias e uma camada de abstração de thread multiplataforma.
 *
 * Este arquivo declara várias funções auxiliares para medição de tempo, geração de números aleatórios
 * e consulta de informações do sistema. Também define um wrapper em torno de
 * pthreads e da API Win32 para fornecer uma interface consistente para o gerenciamento de threads
 * em diferentes sistemas operacionais.
 */

#include "hardstress.h"

/**
 * @brief Obtém o tempo atual como um timestamp de alta resolução em segundos.
 *
 * Esta função usa um relógio monotônico para fornecer uma medição de tempo estável e de alta precisão
 * que não é afetada por mudanças no tempo do sistema.
 *
 * @return O tempo atual em segundos, com precisão de microssegundo ou melhor.
 */
double now_sec(void);

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
uint64_t splitmix64(uint64_t *x);

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
void shuffle32(uint32_t *a, size_t n, uint64_t *seed);

/**
 * @brief Recupera a quantidade total de RAM física no sistema.
 *
 * Esta função é multiplataforma, usando `/proc/meminfo` no Linux e
 * `GlobalMemoryStatusEx` no Windows.
 *
 * @return A memória física total em bytes. Retorna 0 em caso de falha.
 */
unsigned long long get_total_system_memory(void);

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
int thread_create(thread_handle_t *t, thread_func_t func, void *arg);

/**
 * @brief Aguarda a finalização de uma thread específica e limpa seus recursos (multiplataforma).
 *
 * Este é um wrapper em torno de `pthread_join` (POSIX) e `WaitForSingleObject` / `CloseHandle` (Windows).
 *
 * @param t O handle da thread a ser aguardada.
 * @return 0 em caso de sucesso, não-zero em caso de falha.
 */
int thread_join(thread_handle_t t);

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
int thread_detach(thread_handle_t t);

#endif // UTILS_H
