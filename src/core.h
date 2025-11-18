#ifndef CORE_H
#define CORE_H

/**
 * @file core.h
 * @brief Declara a lógica principal de teste de estresse.
 *
 * Este arquivo contém a declaração da função da thread controladora principal,
 * que é o ponto de entrada para iniciar e gerenciar uma sessão de teste de estresse.
 */

#include "hardstress.h"

/**
 * @brief A função principal da thread controladora de teste.
 *
 * Esta função orquestra todo o ciclo de vida de um teste de estresse. É
 * iniciada em uma thread separada quando o usuário clica em "Iniciar". Suas
 * responsabilidades incluem:
 * - Inicializar o estado da aplicação para o teste.
 * - Alocar recursos (memória para workers, buffers de histórico, etc.).
 * - Criar e iniciar as threads de trabalho e a thread de amostragem de métricas.
 * - Fixar as threads de trabalho aos núcleos da CPU, se solicitado.
 * - Monitorar a duração do teste e pará-lo quando concluído.
 * - Limpar todos os recursos e sinalizar a UI quando o teste terminar.
 *
 * @param arg Um ponteiro para a estrutura global `AppContext`.
 */
thread_return_t THREAD_CALL controller_thread_func(void *arg);

#endif // CORE_H
