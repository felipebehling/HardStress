#ifndef METRICS_H
#define METRICS_H

/**
 * @file metrics.h
 * @brief Declara funções para coletar métricas de desempenho do sistema.
 *
 * Este módulo é responsável por coletar dados em tempo real, como
 * utilização de CPU por núcleo, temperatura do sistema e detectar o número de
 * núcleos de CPU disponíveis. Oferece implementações multiplataforma para
 * Linux e Windows.
 */

#include "hardstress.h"

/**
 * @brief A função principal da thread de amostragem de métricas.
 *
 * Esta thread é executada em segundo plano durante um teste de estresse, amostrando
 * periodicamente a utilização da CPU e a temperatura. Após coletar os dados, ela
 * aciona uma nova renderização dos gráficos relevantes da GUI para fornecer feedback em tempo real.
 * Também é responsável por avançar a posição do buffer de histórico.
 *
 * @param arg Um ponteiro para a estrutura global `AppContext`.
 */
thread_return_t THREAD_CALL cpu_sampler_thread_func(void *arg);

/**
 * @brief Detecta o número de núcleos de CPU lógicos no sistema.
 *
 * Esta função fornece uma maneira multiplataforma de determinar o número de
 * processadores, usando `sysconf` em sistemas POSIX e `GetSystemInfo` no Windows.
 *
 * @return O número de núcleos de CPU lógicos disponíveis.
 */
int detect_cpu_count(void);

#ifndef _WIN32
int read_proc_stat(cpu_sample_t *out, int maxcpu, const char *path);
double compute_usage(const cpu_sample_t *a, const cpu_sample_t *b);
#endif

#endif // METRICS_H
