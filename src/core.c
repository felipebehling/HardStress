#include "core.h"
#include "metrics.h" // Para cpu_sampler_thread_func
#include "utils.h"   // Para now_sec, shuffle32, etc.
#include "ui.h"      // Para gui_log

/* --- Static Function Prototypes --- */
static thread_return_t worker_main(void *arg);
static void kernel_fpu(float *A, float *B, float *C, size_t n, int iters);
static inline uint64_t mix64(uint64_t x);
static void kernel_int(uint64_t *dst, size_t n, int iters);
static void kernel_stream(uint8_t *buf, size_t n);
static void kernel_ptrchase(uint32_t *idx, size_t n, int rounds);

/* --- Controller Thread Implementation --- */

thread_return_t controller_thread_func(void *arg){
    AppContext *app = (AppContext*)arg;
    atomic_store(&app->running, 1);
    atomic_store(&app->errors, 0);
    atomic_store(&app->total_iters, 0);
    app->start_time = now_sec();

    int sampler_started = 0;
    int workers_started = 0;

    app->cpu_count = detect_cpu_count();
    app->cpu_usage = calloc(app->cpu_count, sizeof(double));
    if (!app->cpu_usage) {
        gui_log(app, "[Controller] Falha ao alocar buffer de uso de CPU.\n");
        goto cleanup;
    }
#ifndef _WIN32
    app->prev_cpu_samples = calloc(app->cpu_count, sizeof(cpu_sample_t));
    if (!app->prev_cpu_samples) {
        gui_log(app, "[Controller] Falha ao alocar amostras anteriores de CPU.\n");
        goto cleanup;
    }
#endif

    app->history_len = HISTORY_SAMPLES;
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
        for (int i=0; i<app->threads; i++) free(app->thread_history[i]);
        free(app->thread_history);
        app->thread_history = NULL;
    }
    free(app->workers); app->workers = NULL;
    free(app->worker_threads); app->worker_threads = NULL;
    free(app->cpu_usage); app->cpu_usage = NULL;
#ifndef _WIN32
    free(app->prev_cpu_samples); app->prev_cpu_samples = NULL;
#endif

    // A thread se desvincula. Não há mais um handle para ela ser aguardada (joined).
    thread_detach(app->controller_thread); 
    app->controller_thread = 0;

    // Sinaliza para a UI que o teste terminou
    g_idle_add((GSourceFunc)gui_update_stopped, app);
    return 0;
}

/* --- Worker Thread and Kernels Implementation --- */

/**
 * @brief Main function for each worker thread.
 *
 * This function is the entry point for the stress-testing threads. It allocates
 * its memory buffer, initializes it with random data, and then enters a tight
 * loop, calling the selected stress kernel functions until signaled to stop.
 * @param arg A pointer to the worker's `worker_t` context.
 */
static thread_return_t worker_main(void *arg){
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

    // Set up pointers and random seed
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

    // Initialize buffer for FPU kernel
    if (app->kernel_fpu_en && A && B && C && floats_per_vec > 0) {
        for (size_t i=0; i < floats_per_vec; i++){
            A[i] = (float)(splitmix64(&seed) & 0xFFFF) / 65535.0f;
            B[i] = (float)(splitmix64(&seed) & 0xFFFF) / 65535.0f;
            C[i] = (float)(splitmix64(&seed) & 0xFFFF) / 65535.0f;
        }
    }
    // Initialize buffer for Integer kernel
    if (app->kernel_int_en && w->buf) {
        size_t ints64 = w->buf_bytes / sizeof(uint64_t);
        for (size_t i=0;i<ints64;i++) I64[i] = splitmix64(&seed);
    }
    
    // Initialize index array for Pointer Chasing kernel
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
            w->idx[w->idx_len-1] = 0; // Ensure the chase is a cycle
        }
    }

    atomic_store(&w->running, 1u);

    // Main stress loop
    while (atomic_load(&w->running) && atomic_load(&app->running)){
        if (w->buf) {
            if(app->kernel_fpu_en && floats_per_vec > 0 && A && B && C) kernel_fpu(A,B,C, floats_per_vec, 4);
            if(app->kernel_int_en) kernel_int(I64, (w->buf_bytes / sizeof(uint64_t)) > 1024 ? 1024 : (w->buf_bytes / sizeof(uint64_t)), 4);
            if(app->kernel_stream_en) kernel_stream(w->buf, w->buf_bytes);
            if(app->kernel_ptr_en && w->idx) kernel_ptrchase(w->idx, w->idx_len, 4);
        }
        
        atomic_fetch_add(&w->iters, 1u);
        atomic_fetch_add(&app->total_iters, 1u);
        
        // Record iteration count for the history graph
        g_mutex_lock(&app->history_mutex);
        if (app->thread_history) app->thread_history[w->tid][app->history_pos] = atomic_load(&w->iters);
        g_mutex_unlock(&app->history_mutex);
    }

    // Cleanup
    if(w->idx) free(w->idx);
    if(w->buf) free(w->buf);
    return 0;
}

/**
 * @brief Performs a floating-point intensive computation (FMA).
 * Stresses the Floating Point Unit (FPU).
 */
static void kernel_fpu(float *A, float *B, float *C, size_t n, int iters){
    for (int k = 0; k < iters; ++k)
        for (size_t i = 0; i < n; ++i) C[i] = A[i]*B[i] + C[i];
}

/**
 * @brief A 64-bit mixing function to generate pseudo-random behavior.
 * Used by the integer kernel.
 */
static inline uint64_t mix64(uint64_t x){
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return x;
}

/**
 * @brief Performs a series of complex integer and bitwise operations.
 * Stresses the Arithmetic Logic Units (ALUs).
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
 * @brief Performs large memory copy operations.
 * Stresses the memory bus and controllers.
 */
static void kernel_stream(uint8_t *buf, size_t n){
    memset(buf, 0xA5, n/2);
    memcpy(buf + n/2, buf, n/2);
}

/**
 * @brief Traverses a shuffled array of pointers in a pseudo-random order.
 * Stresses the CPU cache and memory prefetcher by creating a long dependency chain.
 */
static void kernel_ptrchase(uint32_t *idx, size_t n, int rounds){
    size_t i = 0;
    for (int r=0;r<rounds;r++)
        for (size_t s=0;s<n;s++) i = idx[i];
    (void)i; // Avoid unused variable warning
}
