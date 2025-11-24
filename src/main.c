/**
 * @file main.c
 * @brief O ponto de entrada principal para a aplicação HardStress.
 *
 * Este arquivo contém a função `main`, que inicializa a aplicação,
 * incluindo o GTK, a estrutura principal AppContext e a interface do usuário.
 * Também define as constantes de cores globais para o tema da UI.
 */
#include "hardstress.h"
#include "ui.h" // Necessário para create_main_window

// Define as cores globais que foram declaradas no cabeçalho.
const color_t COLOR_BG = {0.12, 0.12, 0.12};
const color_t COLOR_FG = {0.15, 0.65, 0.90};
const color_t COLOR_WARN = {0.8, 0.4, 0.1};
const color_t COLOR_ERR = {0.9, 0.2, 0.2};
const color_t COLOR_TEXT = {1.0, 1.0, 1.0};
const color_t COLOR_TEMP = {1.0, 1.0, 0.8};

/**
 * @brief O ponto de entrada principal da aplicação HardStress.
 *
 * Esta função executa os seguintes passos:
 * 1. Inicializa o toolkit GTK.
 * 2. Aloca e inicializa a estrutura principal `AppContext`, que mantém
 *    todo o estado da aplicação.
 * 3. Inicializa mutexes para acesso seguro aos dados por threads.
 * 4. Define valores de configuração padrão.
 * 5. Chama `create_main_window` para construir a GUI.
 * 6. Mostra a janela principal e inicia o loop principal de eventos do GTK.
 *
 * A limpeza de recursos é tratada no callback `on_window_destroy` em `ui.c`.
 *
 * @param argc O número de argumentos da linha de comando.
 * @param argv Um array de strings de argumentos da linha de comando.
 * @return 0 em caso de execução bem-sucedida, 1 em caso de falha.
 */
int main(int argc, char **argv){
    gtk_init(&argc, &argv);
    
    // Allocate and zero out the main application structure
    AppContext *app = calloc(1, sizeof(AppContext));
    if (!app) {
        fprintf(stderr, "Failed to allocate AppContext. Exiting.\n");
        return 1;
    }

    // Initialize mutexes
    g_mutex_init(&app->cpu_mutex);
    g_mutex_init(&app->history_mutex);
    g_mutex_init(&app->temp_mutex);
    g_mutex_init(&app->system_history_mutex);
    
    // Set default configuration
    app->mem_mib_per_thread = DEFAULT_MEM_MIB;
    app->duration_sec = DEFAULT_DURATION_SEC;
    app->pin_affinity = 1;
    app->history_len = HISTORY_SAMPLES;
    app->temp_celsius = TEMP_UNAVAILABLE;
    app->temp_visibility_state = -1; // -1 = unknown

    // Allocate and initialize system metrics history buffers
    app->system_history_len = CPU_HISTORY_SAMPLES;
    app->temp_history = calloc(app->system_history_len, sizeof(double));
    app->avg_cpu_history = calloc(app->system_history_len, sizeof(double));
    if (!app->temp_history || !app->avg_cpu_history) {
        fprintf(stderr, "Failed to allocate system history buffers. Exiting.\n");
        // Perform cleanup before exiting
        free(app->temp_history);
        free(app->avg_cpu_history);
        free(app);
        return 1;
    }

    // Create the main window
    app->win = create_main_window(app);

    gui_log(app, "[GUI] Ready\n");
    gtk_widget_show_all(app->win);
    
    // Start the GTK main event loop
    gtk_main();

    // NOTE: Cleanup is handled in the on_window_destroy callback in ui.c
    
    return 0;
}
