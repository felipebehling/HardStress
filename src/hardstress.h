#ifndef HARDSTRESS_H
#define HARDSTRESS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Wbemidl.h>
#include <pdh.h>
#else
#include <pthread.h>
#include <unistd.h>
typedef struct { unsigned long long user,nice,system,idle,iowait,irq,softirq,steal; } cpu_sample_t;
#endif

#include <gtk/gtk.h>
#include <cairo.h>

/** @file hardstress.h
 *  @brief Arquivo de cabeçalho central para o utilitário HardStress.
 *
 *  Este arquivo define as estruturas de dados principais, constantes e protótipos de funções
 *  usados em toda a aplicação. Inclui cabeçalhos específicos da plataforma,
 *  define uma camada de abstração de thread multiplataforma e declara a struct
 *  principal `AppContext` que encapsula o estado da aplicação.
 */

/* --- CONSTANTES DE CONFIGURAÇÃO --- */
#define DEFAULT_MEM_MIB 256             ///< Memória padrão a ser alocada por thread de trabalho em MiB.
#define DEFAULT_DURATION_SEC 300        ///< Duração padrão do teste de estresse em segundos (5 minutos).
#define CPU_SAMPLE_INTERVAL_MS 1000     ///< Intervalo para amostragem de uso de CPU e temperatura em milissegundos.
#define HISTORY_SAMPLES 240             ///< Número de pontos de dados históricos a serem armazenados para gráficos de desempenho.
#define CPU_HISTORY_SAMPLES 60          ///< Número de amostras mantidas para o gráfico de histórico de uso da CPU.
#define ITER_SCALE 1000.0               ///< Divisor para escalar contagens de iteração para exibição.
#define TEMP_UNAVAILABLE -274.0         ///< Valor sentinela que indica que os dados de temperatura não estão disponíveis.

/* --- TEMA --- */
/** @struct color_t
 *  @brief Representa uma cor RGB para uso na UI desenhada com Cairo.
 */
typedef struct { double r, g, b; } color_t;
extern const color_t COLOR_BG, COLOR_FG, COLOR_WARN, COLOR_ERR, COLOR_TEXT, COLOR_TEMP; ///< Constantes de cor globais.

/* --- ABSTRAÇÃO DE THREAD --- */
#ifdef _WIN32
typedef HANDLE thread_handle_t;         ///< Definição de tipo para um handle de thread (Windows).
#define THREAD_CALL __stdcall            ///< Convenção de chamada para pontos de entrada de thread do Windows.
typedef unsigned thread_return_t;       ///< Tipo de retorno para pontos de entrada de thread (Windows).
#else
typedef pthread_t thread_handle_t;      ///< Definição de tipo para um handle de thread (POSIX).
#define THREAD_CALL                      ///< Macro de convenção de chamada que se expande para nada no POSIX.
typedef void * thread_return_t;         ///< Tipo de retorno para pontos de entrada de thread (POSIX).
#endif

typedef thread_return_t (THREAD_CALL *thread_func_t)(void *); ///< Definição de tipo para uma função de thread (multiplataforma).


/* --- FORWARD DECLARATIONS --- */
typedef struct AppContext AppContext;
typedef struct worker_t worker_t;

/* --- WORKER --- */
/**
 * @enum worker_status_t
 * @brief Representa o status de uma thread de trabalho.
 */
typedef enum {
    WORKER_OK = 0,          ///< O worker está operando normalmente.
    WORKER_ALLOC_FAIL       ///< O worker falhou em alocar seu buffer de memória.
} worker_status_t;

/**
 * @struct worker_t
 * @brief Encapsula o estado e os recursos para uma única thread de trabalho de teste de estresse.
 */
struct worker_t {
    int tid;                ///< ID da thread (0 a N-1).
    size_t buf_bytes;       ///< Tamanho do buffer de memória a ser alocado, em bytes.
    uint8_t *buf;           ///< Ponteiro para o buffer de memória alocado para padrões de acesso à memória.
    uint32_t *idx;          ///< Array de índices para acesso aleatório à memória.
    size_t idx_len;         ///< Número de elementos no array `idx`.
    atomic_int running;     ///< Flag para sinalizar à thread para continuar executando ou terminar.
    atomic_uint iters;      ///< Contador para o número de iterações concluídas.
    atomic_int status;      ///< O status do worker (por exemplo, `WORKER_OK`).
    AppContext *app;        ///< Um ponteiro de volta para o contexto principal da aplicação.
};

/* --- CONTEXTO DA APLICAÇÃO --- */
/**
 * @struct AppContext
 * @brief Encapsula todo o estado da aplicação HardStress.
 *
 * Esta estrutura contém toda a configuração, estado em tempo real, recursos de gerenciamento de threads,
 * buffers de dados e widgets da GUI para a aplicação. Passar um ponteiro para esta struct
 * evita o uso de variáveis globais.
 */
struct AppContext {
    /* --- Configuração (definida a partir da UI) --- */
    int threads;                    ///< Número de threads de trabalho a serem geradas.
    size_t mem_mib_per_thread;      ///< Memória a ser alocada por thread (em MiB).
    int duration_sec;               ///< Duração total do teste em segundos (0 para indefinido).
    int pin_affinity;               ///< Flag booleana para habilitar a fixação de núcleos de CPU.
    int kernel_fpu_en;              ///< Flag booleana para habilitar o kernel de estresse de FPU.
    int kernel_int_en;              ///< Flag booleana para habilitar o kernel de estresse de inteiros.
    int kernel_stream_en;           ///< Flag booleana para habilitar o kernel de streaming de memória.
    int kernel_ptr_en;              ///< Flag booleana para habilitar o kernel de perseguição de ponteiro.

    /* --- Estado de Tempo de Execução --- */
    atomic_int running;             ///< Flag que indica se um teste de estresse está atualmente ativo.
    atomic_int errors;              ///< Contador de erros encontrados durante o teste.
    atomic_uint total_iters;        ///< Contagem de iterações agregadas em todas as threads.
    double start_time;              ///< Timestamp (de `now_sec`) quando o teste começou.

    /* --- Workers & Threads --- */
    worker_t *workers;              ///< Array de contextos de thread de trabalho.
    thread_handle_t *worker_threads;///< Array de handles para as threads de trabalho.
    thread_handle_t cpu_sampler_thread; ///< Handle para a thread de amostragem de métricas.
    thread_handle_t controller_thread;  ///< Handle para a thread controladora de teste principal.

    /* --- Monitoramento de Uso da CPU --- */
    int cpu_count;                  ///< Número de núcleos de CPU lógicos detectados.
    double *cpu_usage;              ///< Array para armazenar a utilização de cada núcleo de CPU (0.0 a 1.0).
    GMutex cpu_mutex;               ///< Mutex para proteger o acesso ao array `cpu_usage`.
#ifdef _WIN32
    /* --- Handles específicos do Windows para monitoramento de desempenho --- */
    PDH_HQUERY pdh_query;           ///< Um handle de consulta para a biblioteca Performance Data Helper (PDH).
    PDH_HCOUNTER *pdh_counters;     ///< Um array de handles de contador para núcleos de CPU individuais.
    IWbemServices *pSvc;            ///< Um ponteiro para os serviços WMI para consulta de temperatura.
    IWbemLocator *pLoc;             ///< Um ponteiro para o localizador WMI para conexão ao WMI.
#else
    cpu_sample_t *prev_cpu_samples; ///< Um buffer para armazenar a amostra de CPU anterior para calcular o delta de uso.
    cpu_sample_t *curr_cpu_samples; ///< Buffer de rascunho reutilizado para a amostra de CPU mais recente.
#endif
    double **cpu_history;           ///< Buffer de histórico circular que armazena amostras de uso de CPU por núcleo.
    int cpu_history_pos;            ///< Índice da entrada mais recente no buffer de histórico da CPU.
    int cpu_history_len;            ///< Capacidade total do buffer de histórico da CPU.
    int cpu_history_filled;         ///< Número de amostras válidas atualmente armazenadas no buffer de histórico.

    /* --- Histórico de Desempenho por Thread --- */
    unsigned **thread_history;      ///< Um buffer circular 2D para armazenar o histórico de iterações por thread.
    int history_pos;                ///< A posição de escrita atual no buffer circular.
    int history_len;                ///< O número de amostras válidas atualmente no buffer.
    GMutex history_mutex;           ///< Mutex para proteger o acesso ao buffer de histórico.

    /* --- Monitoramento de Temperatura --- */
    double temp_celsius;            ///< A última temperatura da CPU medida em graus Celsius.
    GMutex temp_mutex;              ///< Mutex para proteger o acesso a `temp_celsius`.
    char **core_temp_labels;        ///< Rótulos de exibição para cada sensor de temperatura de núcleo físico.
    double *core_temps;             ///< Leituras de temperatura em cache para núcleos físicos.
    int core_temp_count;            ///< Número de entradas válidas em `core_temp_labels/core_temps`.

    /* --- Histórico de Métricas do Sistema --- */
    double *temp_history;           ///< Buffer de histórico circular para a temperatura geral da CPU.
    double *avg_cpu_history;        ///< Buffer de histórico circular para o uso médio de CPU.
    int system_history_pos;         ///< Posição de escrita atual nos buffers de histórico do sistema.
    int system_history_len;         ///< Capacidade total dos buffers de histórico do sistema.
    int system_history_filled;      ///< Número de amostras válidas nos buffers de histórico.
    GMutex system_history_mutex;    ///< Mutex para proteger o acesso aos buffers de histórico do sistema.
    int temp_visibility_state;      ///< Estado de visibilidade do painel de temperatura (-1=unknown, 0=hidden, 1=visible).

    /* --- Widgets da GUI --- */
    GtkWidget *win;                 ///< A janela principal da aplicação.
    GtkWidget *cpu_frame;           ///< O frame que contém a área de desenho do gráfico de sistema.
    GtkWidget *entry_threads, *entry_dur; ///< Campos de entrada para parâmetros de teste.
    GtkWidget *check_pin;           ///< Checkbox para habilitar a fixação de CPU.
    GtkWidget *check_fpu, *check_int, *check_stream, *check_ptr; ///< Checkboxes para kernels de estresse.
    GtkWidget *btn_start, *btn_stop, *btn_defaults, *btn_clear_log; ///< Botões de controle.
    GtkTextBuffer *log_buffer;      ///< Buffer de texto para o painel de log de eventos.
    GtkWidget *log_view;            ///< Widget de visualização de texto para o log de eventos.
    GtkWidget *cpu_drawing;         ///< Área de desenho para o gráfico de utilização de CPU por núcleo.
    GtkWidget *iters_drawing;       ///< Área de desenho para o gráfico de histórico de desempenho por thread.
    GtkWidget *status_label;        ///< Rótulo para exibir o status atual da aplicação.
    guint status_tick_id;           ///< ID da fonte para o temporizador de atualização periódica do rótulo de status.
};

/* --- FUNCTION PROTOTYPES --- */

/* --- ui.c --- */
// Prototypes are now in ui.h

/* --- core.c --- */
// Prototypes are now in core.h

/* --- metrics.c --- */
// Prototypes are now in metrics.h

/* --- utils.c --- */
// Prototypes are now in utils.h

#endif // HARDSTRESS_H
