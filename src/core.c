#include "core.h"
#include "metrics.h" // Para cpu_sampler_thread_func
#include "utils.h"   // Para now_sec, shuffle32, etc.
#include "ui.h"      // Para gui_log

/* --- Static Function Prototypes --- */
static thread_return_t THREAD_CALL worker_main(void *arg);
static void kernel_fpu(float *A, float *B, float *C, size_t n, int iters);
static inline uint64_t mix64(uint64_t x);
static void kernel_int(uint64_t *dst, size_t n, int iters);
static void kernel_stream(uint8_t *buf, size_t n);
static void kernel_ptrchase(uint32_t *idx, size_t n, int rounds);

/* --- Implementação da Thread Controladora --- */

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
thread_return_t THREAD_CALL controller_thread_func(void *arg){
    AppContext *app = (AppContext*)arg;
    atomic_store(&app->running, 1);
    atomic_store(&app->errors, 0);
    atomic_store(&app->total_iters, 0);
    app->start_time = now_sec();

    int sampler_started = 0;
    int workers_started = 0;
    int thread_history_allocated = 0;

    app->cpu_count = detect_cpu_count();
    app->cpu_usage = calloc(app->cpu_count, sizeof(double));
    if (!app->cpu_usage) {
        gui_log(app, "[Controller] Falha ao alocar buffer de uso de CPU.\n");
        goto cleanup;
    }

    app->cpu_history_len = CPU_HISTORY_SAMPLES;
    app->cpu_history_pos = -1;
    app->cpu_history_filled = 0;
    app->cpu_history = calloc(app->cpu_count, sizeof(double*));
    if (!app->cpu_history) {
        gui_log(app, "[Controller] Falha ao alocar histórico de CPU.\n");
        goto cleanup;
    }
    for (int c = 0; c < app->cpu_count; c++) {
        app->cpu_history[c] = calloc(app->cpu_history_len, sizeof(double));
        if (!app->cpu_history[c]) {
            gui_log(app, "[Controller] Falha ao alocar histórico para CPU %d.\n", c);
            goto cleanup;
        }
    }
#ifndef _WIN32
    app->prev_cpu_samples = calloc(app->cpu_count, sizeof(cpu_sample_t));
    app->curr_cpu_samples = calloc(app->cpu_count, sizeof(cpu_sample_t));
    if (!app->prev_cpu_samples || !app->curr_cpu_samples) {
        gui_log(app, "[Controller] Falha ao alocar buffers de amostragem de CPU.\n");
        goto cleanup;
    }
    if (read_proc_stat(app->prev_cpu_samples, app->cpu_count, "/proc/stat") <= 0) {
        gui_log(app, "[Controller] Aviso: não foi possível capturar amostra inicial de CPU.\n");
        memset(app->prev_cpu_samples, 0, app->cpu_count * sizeof(cpu_sample_t));
        memset(app->curr_cpu_samples, 0, app->cpu_count * sizeof(cpu_sample_t));
    } else {
        memcpy(app->curr_cpu_samples, app->prev_cpu_samples, app->cpu_count * sizeof(cpu_sample_t));
    }
#endif

    int history_span = HISTORY_SAMPLES;
    if (app->duration_sec > 0) {
        history_span = app->duration_sec;
    }
    if (history_span <= 0) {
        history_span = HISTORY_SAMPLES;
    }
    app->history_len = history_span;
    app->history_pos = 0;
    app->thread_history = calloc(app->threads, sizeof(unsigned*));
    if (!app->thread_history) {
        gui_log(app, "[Controller] Falha ao alocar histórico de threads.\n");
        goto cleanup;
    }
    for (int t=0; t<app->threads; t++) {
        app->thread_history[t] = calloc(app->history_len, sizeof(unsigned));
        if (!app->thread_history[t]) {
            gui_log(app, "[Controller] Falha ao alocar histórico para thread %d.\n", t);
            goto cleanup;
        }
        thread_history_allocated++;
    }

    app->workers = calloc(app->threads, sizeof(worker_t));
    app->worker_threads = calloc(app->threads, sizeof(thread_handle_t));
    if (!app->workers || !app->worker_threads) {
        gui_log(app, "[Controller] Falha ao alocar estruturas de worker.\n");
        goto cleanup;
    }
    for (int i=0; i<app->threads; i++){
        app->workers[i] = (worker_t){ .tid = i, .app = app };
        app->workers[i].buf_bytes = app->mem_mib_per_thread * 1024ULL * 1024ULL;
        atomic_init(&app->workers[i].status, WORKER_OK);
    }

    if (thread_create(&app->cpu_sampler_thread, cpu_sampler_thread_func, app) != 0){
        gui_log(app, "[Controller] Falha ao iniciar thread de métricas.\n");
        goto cleanup;
    }
    sampler_started = 1;

    for (int i=0; i<app->threads; i++){
        if (thread_create(&app->worker_threads[i], worker_main, &app->workers[i]) != 0){
            gui_log(app, "[Controller] Falha ao iniciar worker %d.\n", i);
            atomic_fetch_add(&app->errors, 1);
            goto cleanup;
        }
        workers_started++;
        if (app->pin_affinity){
#ifdef _WIN32
            if(app->worker_threads[i]) SetThreadAffinityMask(app->worker_threads[i], (DWORD_PTR)(1ULL << (i % app->cpu_count)));
#else
            cpu_set_t set; CPU_ZERO(&set); CPU_SET(i % app->cpu_count, &set);
            pthread_setaffinity_np(app->worker_threads[i], sizeof(cpu_set_t), &set);
#endif
        }
    }

    double end_time = (app->duration_sec > 0) ? app->start_time + app->duration_sec : 0;
    while (atomic_load(&app->running)){
        if (end_time > 0 && now_sec() >= end_time){
             gui_log(app, "[GUI] Duração de %d s atingida. Parando...\n", app->duration_sec);
             atomic_store(&app->running, 0);
             break;
        }
        struct timespec r = {0, 200*1000000}; nanosleep(&r,NULL);
    }

cleanup:
    atomic_store(&app->running, 0);
    if (app->workers){
        for (int i=0; i<workers_started; i++) atomic_store(&app->workers[i].running, 0);
    }
    if (app->worker_threads){
        for (int i=0; i<workers_started; i++){
            if (app->worker_threads[i]) thread_join(app->worker_threads[i]);
        }
    }

    if (sampler_started){
        thread_join(app->cpu_sampler_thread);
    }

    // Limpeza final dos buffers, mas NÃO da estrutura 'app'
    if (app->thread_history) {
        for (int i=0; i<thread_history_allocated; i++) free(app->thread_history[i]);
        free(app->thread_history);
        app->thread_history = NULL;
    }
    free(app->workers); app->workers = NULL;
    free(app->worker_threads); app->worker_threads = NULL;

    g_mutex_lock(&app->cpu_mutex);
    if (app->cpu_history) {
        for (int i = 0; i < app->cpu_count; i++) {
            free(app->cpu_history[i]);
        }
        free(app->cpu_history);
        app->cpu_history = NULL;
    }
    app->cpu_history_len = 0;
    app->cpu_history_filled = 0;
    app->cpu_history_pos = -1;
    free(app->cpu_usage); app->cpu_usage = NULL;
    g_mutex_unlock(&app->cpu_mutex);
#ifndef _WIN32
    free(app->prev_cpu_samples); app->prev_cpu_samples = NULL;
    free(app->curr_cpu_samples); app->curr_cpu_samples = NULL;
#endif

    // Sinaliza para a UI que o teste terminou
    g_idle_add((GSourceFunc)gui_update_stopped, app);
    return 0;
}

/* --- Implementação da Thread Worker e Kernels --- */

/**
 * @brief Função principal para cada thread de trabalho.
 *
 * Esta função é o ponto de entrada para as threads de teste de estresse. Ela aloca
 * seu buffer de memória, inicializa-o com dados aleatórios e, em seguida, entra em um loop
 * apertado, chamando as funções do kernel de estresse selecionadas até ser sinalizada para parar.
 * @param arg Um ponteiro para o contexto `worker_t` do trabalhador.
 */
static thread_return_t THREAD_CALL worker_main(void *arg){
    worker_t *w = (worker_t*)arg;
    AppContext *app = w->app;
    
    atomic_store(&w->status, WORKER_OK);
    if (w->buf_bytes > 0) {
        w->buf = malloc(w->buf_bytes);
        if (!w->buf){
            gui_log(app, "[T%d] Buffer allocation failed (%zu bytes)\n", w->tid, w->buf_bytes);
            atomic_fetch_add(&app->errors, 1);
            atomic_store(&w->status, WORKER_ALLOC_FAIL);
            return 0;
        }
    }

    // Configura ponteiros e semente aleatória
    size_t floats = w->buf_bytes / sizeof(float);
    size_t floats_per_vec = floats / 3;
    float *A = (float*)w->buf;
    float *B = NULL;
    float *C = NULL;
    if (A && floats_per_vec > 0) {
        B = A + floats_per_vec;
        C = B + floats_per_vec;
    }
    uint64_t *I64 = (uint64_t*)w->buf;
    uint64_t seed = 0x12340000 + (uint64_t)w->tid;

    // Inicializa o buffer para o kernel FPU
    if (app->kernel_fpu_en && A && B && C && floats_per_vec > 0) {
        for (size_t i=0; i < floats_per_vec; i++){
            A[i] = (float)(splitmix64(&seed) & 0xFFFF) / 65535.0f;
            B[i] = (float)(splitmix64(&seed) & 0xFFFF) / 65535.0f;
            C[i] = (float)(splitmix64(&seed) & 0xFFFF) / 65535.0f;
        }
    }
    // Inicializa o buffer para o kernel de Inteiros
    if (app->kernel_int_en && w->buf) {
        size_t ints64 = w->buf_bytes / sizeof(uint64_t);
        for (size_t i=0;i<ints64;i++) I64[i] = splitmix64(&seed);
    }
    
    // Inicializa o array de índices para o kernel Pointer Chasing
    if(app->kernel_ptr_en && w->buf) {
        w->idx_len = (w->buf_bytes / sizeof(uint32_t));
        if (w->idx_len > 0) {
            w->idx = malloc(w->idx_len * sizeof(uint32_t));
            if (!w->idx){
                gui_log(app, "[T%d] Index allocation failed\n", w->tid);
                atomic_fetch_add(&app->errors, 1);
                atomic_store(&w->status, WORKER_ALLOC_FAIL);
                free(w->buf);
                return 0;
            }
            for (uint32_t i=0; i<w->idx_len; i++) w->idx[i] = i;
            shuffle32(w->idx, w->idx_len, &seed);
            w->idx[w->idx_len-1] = 0; // Garante que a perseguição seja um ciclo
        }
    }

    atomic_store(&w->running, 1u);

    // Loop principal de estresse
    while (atomic_load(&w->running) && atomic_load(&app->running)){
        if (w->buf) {
            if(app->kernel_fpu_en && floats_per_vec > 0 && A && B && C) kernel_fpu(A,B,C, floats_per_vec, 4);
            if(app->kernel_int_en) kernel_int(I64, (w->buf_bytes / sizeof(uint64_t)) > 1024 ? 1024 : (w->buf_bytes / sizeof(uint64_t)), 4);
            if(app->kernel_stream_en) kernel_stream(w->buf, w->buf_bytes);
            if(app->kernel_ptr_en && w->idx) kernel_ptrchase(w->idx, w->idx_len, 4);
        }
        
        atomic_fetch_add(&w->iters, 1u);
        atomic_fetch_add(&app->total_iters, 1u);
        
        // Registra a contagem de iterações para o gráfico de histórico
        g_mutex_lock(&app->history_mutex);
        if (app->thread_history) app->thread_history[w->tid][app->history_pos] = atomic_load(&w->iters);
        g_mutex_unlock(&app->history_mutex);
    }

    // Limpeza
    if(w->idx) free(w->idx);
    if(w->buf) free(w->buf);
    return 0;
}

/**
 * @brief Realiza um cálculo intensivo de ponto flutuante (FMA).
 * Estressa a Unidade de Ponto Flutuante (FPU).
 */
static void kernel_fpu(float *A, float *B, float *C, size_t n, int iters){
    for (int k = 0; k < iters; ++k)
        for (size_t i = 0; i < n; ++i) C[i] = A[i]*B[i] + C[i];
}

/**
 * @brief Uma função de mistura de 64 bits para gerar comportamento pseudoaleatório.
 * Usado pelo kernel de inteiros.
 */
static inline uint64_t mix64(uint64_t x){
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return x;
}

/**
 * @brief Realiza uma série de operações complexas de inteiros e bitwise.
 * Estressa as Unidades Lógicas e Aritméticas (ALUs).
 */
static void kernel_int(uint64_t *dst, size_t n, int iters){
    uint64_t acc = 0xC0FFEE;
    for (int k=0;k<iters;k++){
        for (size_t i=0;i<n;i++){
            acc ^= mix64(dst[i] + i);
            dst[i] = acc + (dst[i] << 1) + (dst[i] >> 3);
        }
    }
}

/**
 * @brief Realiza grandes operações de cópia de memória.
 * Estressa o barramento de memória e os controladores.
 */
static void kernel_stream(uint8_t *buf, size_t n){
    memset(buf, 0xA5, n/2);
    memcpy(buf + n/2, buf, n/2);
}

/**
 * @brief Percorre um array embaralhado de ponteiros em uma ordem pseudoaleatória.
 * Estressa o cache da CPU e o prefetcher de memória criando uma longa cadeia de dependência.
 */
static void kernel_ptrchase(uint32_t *idx, size_t n, int rounds){
    size_t i = 0;
    for (int r=0;r<rounds;r++)
        for (size_t s=0;s<n;s++) i = idx[i];
    (void)i; // Avoid unused variable warning
}
