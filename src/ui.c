/* ui.c - Modernized HardStress interface with a KDE Plasma-inspired style */
#include "ui.h"
#include "core.h"
#include "metrics.h"
#include "utils.h"
#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <float.h>

/* --- Dark Theme Color Definitions --- */
typedef struct {
    double r, g, b, a;
} rgba_t;

// Modern color palette inspired by KDE Plasma
//static const rgba_t THEME_BG_PRIMARY = {0.118, 0.118, 0.180, 1.0};      // #1e1e2e - Main background
static const rgba_t THEME_BG_SECONDARY = {0.157, 0.157, 0.227, 1.0};    // #28283a - Panels
static const rgba_t THEME_BG_TERTIARY = {0.196, 0.196, 0.274, 1.0};     // #32324a - Elevated elements
static const rgba_t THEME_ACCENT = {0.0, 0.749, 1.0, 1.0};              // #00bfff - Vibrant cyan blue
static const rgba_t THEME_WARN = {0.976, 0.886, 0.686, 1.0};            // #f9e2af - Amber/Orange
static const rgba_t THEME_ERROR = {0.949, 0.561, 0.678, 1.0};           // #f28fad - Light red
//static const rgba_t THEME_SUCCESS = {0.565, 0.933, 0.565, 1.0};         // #90ee90 - Light green
static const rgba_t THEME_TEXT_PRIMARY = {0.878, 0.878, 0.878, 1.0};    // #e0e0e0 - Primary text
static const rgba_t THEME_TEXT_SECONDARY = {0.627, 0.627, 0.627, 1.0}; // #a0a0a0 - Secondary text
static const rgba_t THEME_GRID = {0.235, 0.235, 0.314, 0.5};            // Subtle grid

/* --- Static Function Prototypes --- */
static gboolean on_draw_system_graph(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static gboolean on_draw_iters(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static void on_btn_start_clicked(GtkButton *b, gpointer ud);
static void on_btn_stop_clicked(GtkButton *b, gpointer ud);
static void on_btn_defaults_clicked(GtkButton *b, gpointer ud);
static void on_btn_clear_log_clicked(GtkButton *b, gpointer ud);
static gboolean on_window_delete(GtkWidget *w, GdkEvent *e, gpointer ud);
static void on_window_destroy(GtkWidget *w, gpointer ud);
static gboolean ui_tick(gpointer ud);
static void set_controls_sensitive(AppContext *app, gboolean state);
static gboolean gui_update_started(gpointer ud);
static void apply_css_theme(GtkWidget *window);
static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r);
static void draw_grid_background(cairo_t *cr, int width, int height, int spacing);
static double clamp01(double v);
static rgba_t lerp_color(rgba_t a, rgba_t b, double t);
static rgba_t heatmap_color(double normalized);

gboolean gui_update_stopped(gpointer ud);

#ifndef TESTING_BUILD
typedef struct {
    AppContext *app;
    char *message;
    time_t when;
} GuiLogMessage;

static gboolean gui_log_dispatch(gpointer data);
static void gui_log_message_free(gpointer data);
#endif

/* --- Implementations --- */

/**
 * @brief Aplica o tema CSS da aplicação.
 *
 * Tenta carregar `style.css` de vários locais possíveis para suportar
 * tanto ambientes de desenvolvimento quanto instalados.
 * @param window A janela principal da aplicação.
 */
static void apply_css_theme(GtkWidget *window) {
    GtkCssProvider *provider = gtk_css_provider_new();
    // Try loading from multiple locations, including the development directory
    const char *css_paths[] = {
        "src/style.css",
        "style.css",
        "/usr/share/hardstress/style.css",
        NULL
    };

    gboolean loaded = FALSE;
    for (int i = 0; css_paths[i] != NULL; i++) {
        if (g_file_test(css_paths[i], G_FILE_TEST_EXISTS)) {
            if (gtk_css_provider_load_from_path(provider, css_paths[i], NULL)) {
                loaded = TRUE;
                break;
            }
        }
    }

    if (!loaded) {
        g_warning("Could not load CSS file 'style.css'. The appearance may be incorrect.");
        g_object_unref(provider);
        return;
    }
    
    GdkScreen *screen = gtk_widget_get_screen(window);
    gtk_style_context_add_provider_for_screen(screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider);
}

/**
 * @brief Função auxiliar do Cairo para desenhar um retângulo com cantos arredondados.
 */
static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
    cairo_arc(cr, x + w - r, y + r, r, 1.5 * M_PI, 2 * M_PI);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, 0.5 * M_PI);
    cairo_arc(cr, x + r, y + h - r, r, 0.5 * M_PI, M_PI);
    cairo_close_path(cr);
}

/**
 * @brief Função auxiliar do Cairo para desenhar um padrão de fundo de grade.
 */
static void G_GNUC_UNUSED draw_grid_background(cairo_t *cr, int width, int height, int spacing) {
    cairo_set_source_rgba(cr, THEME_GRID.r, THEME_GRID.g, THEME_GRID.b, THEME_GRID.a);
    cairo_set_line_width(cr, 0.5);
    
    // Vertical lines
    for (int x = 0; x <= width; x += spacing) {
        cairo_move_to(cr, x + 0.5, 0);
        cairo_line_to(cr, x + 0.5, height);
    }
    
    // Horizontal lines
    for (int y = 0; y <= height; y += spacing) {
        cairo_move_to(cr, 0, y + 0.5);
        cairo_line_to(cr, width, y + 0.5);
    }
    
    cairo_stroke(cr);
}

static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

static rgba_t lerp_color(rgba_t a, rgba_t b, double t) {
    rgba_t r = {
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    };
    return r;
}

static rgba_t heatmap_color(double normalized) {
    static const rgba_t cold = {0.047, 0.203, 0.725, 1.0};
    static const rgba_t warm = {1.0, 0.933, 0.0, 1.0};
    static const rgba_t hot = {0.913, 0.231, 0.231, 1.0};

    double n = clamp01(normalized);
    if (n <= 0.5) {
        return lerp_color(cold, warm, n / 0.5);
    }
    return lerp_color(warm, hot, (n - 0.5) / 0.5);
}

/**
 * @brief Callback para o evento "destroy" da janela.
 *
 * Esta função limpa todos os recursos da aplicação, incluindo mutexes e
 * a struct principal AppContext, antes de sair do loop principal do GTK.
 */
static void on_window_destroy(GtkWidget *w, gpointer ud) {
    (void)w;
    AppContext *app = (AppContext*)ud;

    if (atomic_load(&app->running)) {
        atomic_store(&app->running, 0);
    }

    if (app->status_tick_id > 0) {
        g_source_remove(app->status_tick_id);
        app->status_tick_id = 0;
    }

    if (app->controller_thread) {
        thread_join(app->controller_thread);
        app->controller_thread = 0;
    }

    g_mutex_lock(&app->temp_mutex);
    if (app->core_temp_labels) {
        for (int i = 0; i < app->core_temp_count; ++i) {
            g_free(app->core_temp_labels[i]);
        }
        g_free(app->core_temp_labels);
        app->core_temp_labels = NULL;
    }
    g_free(app->core_temps);
    app->core_temps = NULL;
    app->core_temp_count = 0;
    g_mutex_unlock(&app->temp_mutex);
    g_mutex_clear(&app->cpu_mutex);
    g_mutex_clear(&app->history_mutex);
    g_mutex_clear(&app->temp_mutex);
    g_mutex_clear(&app->system_history_mutex);

    // Free system history buffers
    free(app->temp_history);
    app->temp_history = NULL;
    free(app->avg_cpu_history);
    app->avg_cpu_history = NULL;

    free(app);

    gtk_main_quit();
}

typedef struct {
    AppContext *app;
    gboolean visible;
} TempPanelVisibilityUpdate;

static gboolean gui_set_temp_panel_visibility_dispatch(gpointer data) {
    TempPanelVisibilityUpdate *update = data;
    if (update->app && update->app->cpu_frame) {
        gtk_widget_set_visible(update->app->cpu_frame, update->visible);
    }
    g_free(update);
    return G_SOURCE_REMOVE;
}

void gui_set_temp_panel_visibility(AppContext *app, gboolean visible) {
    if (!app) return;

    // Check if the state is already as requested to prevent redundant UI updates
    if (app->temp_visibility_state == (int)visible) {
        return;
    }
    app->temp_visibility_state = (int)visible;

    TempPanelVisibilityUpdate *update = g_new(TempPanelVisibilityUpdate, 1);
    update->app = app;
    update->visible = visible;
    g_idle_add(gui_set_temp_panel_visibility_dispatch, update);
}


#ifndef TESTING_BUILD
/**
 * @brief Adiciona uma mensagem formatada e com carimbo de data/hora ao log da GUI.
 */
void gui_log(AppContext *app, const char *fmt, ...){
    if (!app || !fmt) return;

    va_list ap;
    va_start(ap, fmt);
    char *formatted = g_strdup_vprintf(fmt, ap);
    va_end(ap);

    if (!formatted) return;

    GuiLogMessage *msg = g_new(GuiLogMessage, 1);
    if (!msg) {
        g_free(formatted);
        return;
    }

    msg->app = app;
    msg->message = formatted;
    msg->when = time(NULL);

    if (g_idle_add_full(G_PRIORITY_DEFAULT, gui_log_dispatch, msg, gui_log_message_free) == 0) {
        gui_log_message_free(msg);
        g_warning("Failed to enqueue log message for GUI thread.");
    }
}
#endif

#ifndef TESTING_BUILD
static gboolean gui_log_dispatch(gpointer data) {
    GuiLogMessage *msg = data;
    AppContext *app = msg->app;

    if (!app || !app->log_buffer || !app->log_view) {
        return G_SOURCE_REMOVE;
    }

    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &msg->when);
#else
    localtime_r(&msg->when, &t);
#endif
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S]", &t);

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app->log_buffer, &end);
    gtk_text_buffer_insert(app->log_buffer, &end, timestamp, -1);
    gtk_text_buffer_insert(app->log_buffer, &end, " ", -1);
    gtk_text_buffer_insert(app->log_buffer, &end, msg->message, -1);

    GtkTextMark *mark = gtk_text_buffer_create_mark(app->log_buffer, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(app->log_view), mark, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(app->log_buffer, mark);

    return G_SOURCE_REMOVE;
}

static void gui_log_message_free(gpointer data) {
    GuiLogMessage *msg = data;
    if (!msg) {
        return;
    }
    g_free(msg->message);
    g_free(msg);
}
#endif

/**
 * @brief GSourceFunc para atualizar a GUI quando um teste inicia.
 *
 * Chamado via `g_idle_add` para atualizar com segurança os widgets GTK da thread principal.
 */
static gboolean gui_update_started(gpointer ud){
    AppContext *app = (AppContext*)ud;
    gtk_widget_set_sensitive(app->btn_stop, TRUE);
    gtk_label_set_text(GTK_LABEL(app->status_label), "🚀 Running...");
    gui_log(app, "[GUI] Test started: threads=%d mem/thread=%zu dur=%ds pin=%d\n",
            app->threads, app->mem_mib_per_thread, app->duration_sec, app->pin_affinity);
    return G_SOURCE_REMOVE;
}

/**
 * @brief GSourceFunc para atualizar a GUI quando um teste para.
 *
 * Chamado via `g_idle_add` para atualizar com segurança os widgets GTK da thread principal.
 */
gboolean gui_update_stopped(gpointer ud){
    AppContext *app = (AppContext*)ud;
    if (app->controller_thread) {
        thread_join(app->controller_thread);
        app->controller_thread = 0;
    }
    set_controls_sensitive(app, TRUE);
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    gtk_label_set_text(GTK_LABEL(app->status_label), "⏹ Stopped");
    gui_log(app, "[GUI] Test stopped.\n");
    return G_SOURCE_REMOVE;
}

/**
 * @brief Callback para o evento de clique do botão "Iniciar".
 *
 * Analisa e valida a entrada do usuário, configura o AppContext, desabilita
 * os controles de configuração e gera a thread controladora principal para iniciar
 * o teste de estresse.
 */
static void on_btn_start_clicked(GtkButton *b, gpointer ud){
    (void)b;
    AppContext *app = (AppContext*)ud;
    if (atomic_load(&app->running)) return;

    char *end;
    char *threads_str = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->entry_threads));
    if (!threads_str) {
        gui_log(app, "[GUI] Could not read thread value.\n");
        return;
    }
    long threads;
    if (strcmp(threads_str, "Auto") == 0) {
        threads = 0;
    } else {
        errno = 0;
        threads = strtol(threads_str, &end, 10);
        if (*end != '\0' || threads < 0 || errno == ERANGE){
            gui_log(app, "[GUI] Invalid threads value\n");
            g_free(threads_str);
            return;
        }
    }
    g_free(threads_str);

    errno = 0;
    long dur = strtol(gtk_entry_get_text(GTK_ENTRY(app->entry_dur)), &end, 10);
    if (*end != '\0' || dur < 0 || errno == ERANGE){
        gui_log(app, "[GUI] Invalid duration value\n");
        return;
    }

    app->threads = (threads == 0) ? detect_cpu_count() : (int)threads;
    app->mem_mib_per_thread = DEFAULT_MEM_MIB;
    app->duration_sec = (int)dur;
    app->pin_affinity = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_pin));
    app->kernel_fpu_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_fpu));
    app->kernel_int_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_int));
    app->kernel_stream_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_stream));
    app->kernel_ptr_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_ptr));

    if (!app->kernel_fpu_en && !app->kernel_int_en && !app->kernel_stream_en && !app->kernel_ptr_en) {
        gui_log(app, "[GUI] ERROR: At least one stress kernel must be selected.\n");
        return;
    }

    unsigned long long total_mem_bytes = get_total_system_memory();
    if (total_mem_bytes > 0 && app->threads > 0) {
        unsigned long long required_bytes = app->mem_mib_per_thread * 1024ULL * 1024ULL;
        required_bytes *= (unsigned long long)app->threads;
        long double usage_ratio = (long double)required_bytes / (long double)total_mem_bytes;
        if (usage_ratio >= 0.90L) {
            gui_log(app,
                "[GUI] ERROR: Configuration would reserve ~%llu MiB but only %llu MiB are available. Reduce the thread count.\n",
                required_bytes / (1024ULL * 1024ULL), total_mem_bytes / (1024ULL * 1024ULL));
            return;
        }
    }

    set_controls_sensitive(app, FALSE);
    g_idle_add(gui_update_started, app);
    thread_create(&app->controller_thread, controller_thread_func, app);
}

/**
 * @brief Callback para o evento de clique do botão "Parar".
 *
 * Sinaliza para a thread controladora terminar o teste atualmente em execução.
 */
static void on_btn_stop_clicked(GtkButton *b, gpointer ud){
    (void)b;
    AppContext *app = (AppContext*)ud;
    if (!atomic_load(&app->running)) return;
    atomic_store(&app->running, 0);
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    gui_log(app, "[GUI] Stop requested by user.\n");
}

/**
 * @brief Callback para o botão "Restaurar Padrões".
 *
 * Redefine todas as opções de configuração na UI para seus valores padrão.
 */
static void on_btn_defaults_clicked(GtkButton *b, gpointer ud) {
    (void)b;
    AppContext *app = (AppContext*)ud;

    gtk_combo_box_set_active(GTK_COMBO_BOX(app->entry_threads), 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_pin), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_fpu), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_int), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_stream), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_ptr), TRUE);

    char dur_buf[32];
    snprintf(dur_buf, sizeof(dur_buf), "%d", DEFAULT_DURATION_SEC);
    gtk_entry_set_text(GTK_ENTRY(app->entry_dur), dur_buf);

    gui_log(app, "[GUI] Settings restored to defaults.\n");
}

/**
 * @brief Callback para o botão "Limpar Log".
 */
static void on_btn_clear_log_clicked(GtkButton *b, gpointer ud) {
    (void)b;
    AppContext *app = (AppContext*)ud;
    gtk_text_buffer_set_text(app->log_buffer, "", -1);
    gui_log(app, "[GUI] Log cleared.\n");
}

static gboolean check_if_stopped_and_close(gpointer user_data) {
    AppContext *app = (AppContext*)user_data;

    if (app->controller_thread == 0 || !atomic_load(&app->running)) {
        gtk_widget_destroy(app->win);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

/**
 * @brief Callback para o evento "delete" da janela (botão de fechar).
 *
 * Se um teste estiver em execução, ele sinaliza para parar antes de permitir que a janela
 * feche, evitando uma terminação abrupta.
 */
static gboolean on_window_delete(GtkWidget *w, GdkEvent *e, gpointer ud){
    (void)e;
    AppContext *app = (AppContext*)ud;
    if (atomic_load(&app->running)){
        gui_log(app, "[GUI] Closing: requesting stop...\n");
        atomic_store(&app->running, 0);
        g_timeout_add(100, check_if_stopped_and_close, app);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Um callback de timer (tick) que é executado uma vez por segundo.
 *
 * Atualiza o rótulo de status principal com dados de desempenho em tempo real, como
 * total de iterações por segundo e contagem de erros.
 */
static gboolean ui_tick(gpointer ud){
    AppContext *app = (AppContext*)ud;
    if (!atomic_load(&app->running)) {
        if (strcmp(gtk_label_get_text(GTK_LABEL(app->status_label)), "⏹ Stopped") != 0) {
            gtk_label_set_text(GTK_LABEL(app->status_label), "⏹ Stopped");
        }
        return TRUE;
    }
    static unsigned long long last_total = 0;
    unsigned long long cur = atomic_load(&app->total_iters);
    unsigned long long diff = cur - last_total;
    last_total = cur;
    char buf[256];
    snprintf(buf, sizeof(buf), "⚡ Performance: %llu iters/s | Errors: %d", diff, atomic_load(&app->errors));
    gtk_label_set_text(GTK_LABEL(app->status_label), buf);
    return TRUE;
}

/**
 * @brief Constrói e retorna a janela principal da aplicação com todos os seus widgets.
 */
GtkWidget* create_main_window(AppContext *app) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->win = win;
    gtk_window_set_default_size(GTK_WINDOW(win), 1400, 900);
    gtk_window_set_title(GTK_WINDOW(win), "HardStress - Advanced System Stress Testing");
    
    // Apply CSS theme
    apply_css_theme(win);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(win), main_box);

    // --- LEFT SIDEBAR ---
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_size_request(sidebar, 320, -1);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 20);
    gtk_box_pack_start(GTK_BOX(main_box), sidebar, FALSE, FALSE, 0);

    // Panel Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span font='Inter Bold 18' foreground='#00bfff'>HardStress</span>\n"
        "<span font='Inter 10' foreground='#a0a0a0'>Stress Testing System</span>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_LEFT);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sidebar), title, FALSE, FALSE, 0);

    // Settings Frame
    GtkWidget *config_frame = gtk_frame_new("Settings");
    GtkWidget *config_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(config_grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(config_grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(config_grid), 10);
    gtk_container_add(GTK_CONTAINER(config_frame), config_grid);
    gtk_box_pack_start(GTK_BOX(sidebar), config_frame, FALSE, FALSE, 0);

    int row = 0;
    
    // Threads
    GtkWidget *threads_label = gtk_label_new("Threads:");
    gtk_widget_set_halign(threads_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), threads_label, 0, row, 1, 1);
    app->entry_threads = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->entry_threads), "Auto");
    int num_cpus = detect_cpu_count();
    for (int i = 1; i <= num_cpus; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->entry_threads), buf);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->entry_threads), 0);
    gtk_grid_attach(GTK_GRID(config_grid), app->entry_threads, 1, row++, 1, 1);

    // Memory (fixed)
    GtkWidget *mem_label = gtk_label_new("Memory per thread:");
    gtk_widget_set_halign(mem_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), mem_label, 0, row, 1, 1);
    char mem_buf[64];
    snprintf(mem_buf, sizeof(mem_buf), "%zu MiB (fixed)", app->mem_mib_per_thread);
    GtkWidget *mem_value_label = gtk_label_new(mem_buf);
    gtk_widget_set_halign(mem_value_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), mem_value_label, 1, row++, 1, 1);

    // Duration
    GtkWidget *dur_label = gtk_label_new("Duration (s, 0=∞):");
    gtk_widget_set_halign(dur_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), dur_label, 0, row, 1, 1);
    app->entry_dur = gtk_entry_new();
    char dur_buf[32]; snprintf(dur_buf, sizeof(dur_buf), "%d", app->duration_sec);
    gtk_entry_set_text(GTK_ENTRY(app->entry_dur), dur_buf);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_dur), "Time in seconds");
    gtk_grid_attach(GTK_GRID(config_grid), app->entry_dur, 1, row++, 1, 1);

    // Kernels Frame
    GtkWidget *kernel_frame = gtk_frame_new("Stress Kernels");
    GtkWidget *kernel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(kernel_box), 10);
    gtk_container_add(GTK_CONTAINER(kernel_frame), kernel_box);
    gtk_box_pack_start(GTK_BOX(sidebar), kernel_frame, FALSE, FALSE, 0);

    app->check_fpu = gtk_check_button_new_with_label("FPU (Floating Point)");
    app->check_int = gtk_check_button_new_with_label("ALU (Integers)");
    app->check_stream = gtk_check_button_new_with_label("Memory Stream");
    app->check_ptr = gtk_check_button_new_with_label("Pointer Chasing");
    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_fpu), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_int), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_stream), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_ptr), TRUE);
    
    gtk_box_pack_start(GTK_BOX(kernel_box), app->check_fpu, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(kernel_box), app->check_int, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(kernel_box), app->check_stream, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(kernel_box), app->check_ptr, FALSE, FALSE, 0);

    // Additional Options
    GtkWidget *options_frame = gtk_frame_new("Options");
    GtkWidget *options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(options_box), 10);
    gtk_container_add(GTK_CONTAINER(options_frame), options_box);
    gtk_box_pack_start(GTK_BOX(sidebar), options_frame, FALSE, FALSE, 0);

    app->check_pin = gtk_check_button_new_with_label("Pin threads to CPUs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_pin), TRUE);

    gtk_box_pack_start(GTK_BOX(options_box), app->check_pin, FALSE, FALSE, 0);

    // Control Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->btn_start = gtk_button_new_with_label("▶ Start");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_start), "styled-button");
    app->btn_stop = gtk_button_new_with_label("⏹ Stop");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_stop), "styled-button");
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    
    gtk_box_pack_start(GTK_BOX(button_box), app->btn_start, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), app->btn_stop, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), button_box, FALSE, FALSE, 0);

    app->btn_defaults = gtk_button_new_with_label("Restore Defaults");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_defaults), "styled-button");
    gtk_box_pack_start(GTK_BOX(sidebar), app->btn_defaults, FALSE, FALSE, 0);

    // Status Label
    app->status_label = gtk_label_new("⏹ Ready");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->status_label), "status-label");
    gtk_box_pack_start(GTK_BOX(sidebar), app->status_label, FALSE, FALSE, 0);

    // --- MAIN AREA (RIGHT) ---
    GtkWidget *main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(main_area), 20);
    gtk_box_pack_start(GTK_BOX(main_box), main_area, TRUE, TRUE, 0);

    // CPU Temperature List
    app->cpu_frame = gtk_frame_new("Monitor do Sistema");
    app->cpu_drawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->cpu_drawing, -1, 220);
    gtk_container_add(GTK_CONTAINER(app->cpu_frame), app->cpu_drawing);
    gtk_box_pack_start(GTK_BOX(main_area), app->cpu_frame, FALSE, FALSE, 0);

    // Iterations Graph
    GtkWidget *iters_frame = gtk_frame_new("Thread Activity Heatmap");
    app->iters_drawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->iters_drawing, -1, 300);
    gtk_container_add(GTK_CONTAINER(iters_frame), app->iters_drawing);
    gtk_box_pack_start(GTK_BOX(main_area), iters_frame, FALSE, FALSE, 0);

    // System Log
    GtkWidget *log_frame = gtk_frame_new("System Log");
    GtkWidget *log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(log_frame), log_box);

    app->btn_clear_log = gtk_button_new_with_label("Clear Log");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_clear_log), "styled-button");
    gtk_widget_set_halign(app->btn_clear_log, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(log_box), app->btn_clear_log, FALSE, FALSE, 0);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    app->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->log_view), GTK_WRAP_WORD);
    app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->log_view));
    gtk_container_add(GTK_CONTAINER(scrolled), app->log_view);
    gtk_box_pack_start(GTK_BOX(log_box), scrolled, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_area), log_frame, TRUE, TRUE, 0);

    // Connect signals
    g_signal_connect(win, "destroy", G_CALLBACK(on_window_destroy), app);
    g_signal_connect(win, "delete-event", G_CALLBACK(on_window_delete), app);
    g_signal_connect(app->btn_start, "clicked", G_CALLBACK(on_btn_start_clicked), app);
    g_signal_connect(app->btn_stop, "clicked", G_CALLBACK(on_btn_stop_clicked), app);
    g_signal_connect(app->btn_defaults, "clicked", G_CALLBACK(on_btn_defaults_clicked), app);
    g_signal_connect(app->btn_clear_log, "clicked", G_CALLBACK(on_btn_clear_log_clicked), app);
    g_signal_connect(app->cpu_drawing, "draw", G_CALLBACK(on_draw_system_graph), app);
    g_signal_connect(app->iters_drawing, "draw", G_CALLBACK(on_draw_iters), app);

    // Timer to update the status label
    app->status_tick_id = g_timeout_add(1000, ui_tick, app);

    return win;
}

/**
 * @brief Ativa ou desativa a sensibilidade dos controles de configuração.
 *
 * Isso é usado para impedir que o usuário altere as configurações enquanto um teste
 * está em execução.
 * @param app O contexto da aplicação.
 * @param state `TRUE` para ativar os controles, `FALSE` para desativar.
 */
static void set_controls_sensitive(AppContext *app, gboolean state){
    gtk_widget_set_sensitive(app->entry_threads, state);
    gtk_widget_set_sensitive(app->entry_dur, state);
    gtk_widget_set_sensitive(app->check_pin, state);
    gtk_widget_set_sensitive(app->check_fpu, state);
    gtk_widget_set_sensitive(app->check_int, state);
    gtk_widget_set_sensitive(app->check_stream, state);
    gtk_widget_set_sensitive(app->check_ptr, state);
    gtk_widget_set_sensitive(app->btn_start, state);
}

/**
 * @brief Manipulador de desenho do Cairo para o gráfico de métricas do sistema.
 *
 * Renderiza um gráfico de linhas em tempo real mostrando o histórico da temperatura
 * da CPU e o uso médio da CPU. O gráfico apresenta dois eixos Y para acomodar
 * as diferentes escalas das duas métricas.
 */
static gboolean on_draw_system_graph(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppContext *app = (AppContext *)user_data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    const int w = alloc.width;
    const int h = alloc.height;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    // Desenha o fundo do widget
    cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, THEME_BG_SECONDARY.a);
    draw_rounded_rect(cr, 0, 0, w, h, 8.0);
    cairo_fill(cr);

    // Se não houver dados históricos para exibir, mostra uma mensagem e encerra.
    if (app->system_history_filled == 0) {
        cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.8);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_text_extents_t ext;
        const char *msg = "Aguardando dados de monitoramento do sistema...";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, (w - ext.width) / 2.0, (h + ext.height) / 2.0);
        cairo_show_text(cr, msg);
        return FALSE;
    }

    // --- Configuração do Gráfico ---
    const double margin_top = 30.0, margin_bottom = 20.0, margin_left = 50.0, margin_right = 50.0;
    const double chart_w = w - margin_left - margin_right;
    const double chart_h = h - margin_top - margin_bottom;
    const int num_x_labels = 6; // Rótulos no eixo X
    const int num_y_labels = 5; // Rótulos em cada eixo Y

    // --- Coleta e Análise de Dados ---
    g_mutex_lock(&app->system_history_mutex);
    // Cria cópias locais dos dados para evitar manter o mutex bloqueado durante o desenho
    int len = app->system_history_filled;
    double *temp_data = malloc(len * sizeof(double));
    double *cpu_data = malloc(len * sizeof(double));
    if (!temp_data || !cpu_data) {
        if(temp_data) free(temp_data);
        if(cpu_data) free(cpu_data);
        g_mutex_unlock(&app->system_history_mutex);
        return FALSE; // Falha na alocação de memória
    }

    int start_pos = (app->system_history_pos + 1) % app->system_history_len;
    for (int i = 0; i < len; i++) {
        int idx = (start_pos + i) % app->system_history_len;
        temp_data[i] = app->temp_history[idx];
        cpu_data[i] = app->avg_cpu_history[idx];
    }
    g_mutex_unlock(&app->system_history_mutex);

    // Encontra os valores mínimo e máximo para a temperatura para escalar o eixo Y esquerdo
    double temp_min = 120.0, temp_max = 0.0;
    for (int i = 0; i < len; i++) {
        if (temp_data[i] < temp_min) temp_min = temp_data[i];
        if (temp_data[i] > temp_max) temp_max = temp_data[i];
    }
    temp_min = floor(temp_min / 10.0) * 10.0; // Arredonda para baixo para a dezena mais próxima
    temp_max = ceil(temp_max / 10.0) * 10.0;  // Arredonda para cima para a dezena mais próxima
    if (temp_max - temp_min < 10.0) temp_max = temp_min + 10.0; // Garante um intervalo mínimo

    // --- Desenho da Grade e Rótulos ---
    cairo_set_line_width(cr, 1.0);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);

    // Linhas da grade horizontal e rótulos do eixo Y
    for (int i = 0; i <= num_y_labels; i++) {
        double y = margin_top + (chart_h * i) / num_y_labels;
        // Desenha a linha da grade
        cairo_set_source_rgba(cr, THEME_GRID.r, THEME_GRID.g, THEME_GRID.b, THEME_GRID.a);
        cairo_move_to(cr, margin_left, y);
        cairo_line_to(cr, margin_left + chart_w, y);
        cairo_stroke(cr);

        // Rótulo da Temperatura (Eixo Y Esquerdo - Vermelho)
        char label_temp[16];
        double temp_val = temp_max - (i * (temp_max - temp_min) / num_y_labels);
        snprintf(label_temp, sizeof(label_temp), "%.0f°C", temp_val);
        cairo_set_source_rgba(cr, 1.0, 0.5, 0.5, 0.9);
        cairo_move_to(cr, margin_left - 35, y + 4);
        cairo_show_text(cr, label_temp);

        // Rótulo de Uso da CPU (Eixo Y Direito - Azul)
        char label_cpu[16];
        double cpu_val = 100.0 - (i * 100.0 / num_y_labels);
        snprintf(label_cpu, sizeof(label_cpu), "%.0f%%", cpu_val);
        cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.9);
        cairo_move_to(cr, w - margin_right + 10, y + 4);
        cairo_show_text(cr, label_cpu);
    }

    // Rótulos do eixo X (Tempo)
    cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.9);
    for (int i = 0; i <= num_x_labels; i++) {
        char label[16];
        int sec = (app->system_history_len -1) * i / num_x_labels;
        snprintf(label, sizeof(label), "%ds", sec);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, label, &ext);
        double x = margin_left + chart_w - (chart_w * i / num_x_labels) - (ext.width / 2.0);
        cairo_move_to(cr, x, h - margin_bottom + 15);
        cairo_show_text(cr, label);
    }

    // --- Desenho das Linhas do Gráfico ---
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    // Linha de Temperatura (Vermelho)
    cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, 0.9);
    for (int i = 0; i < len; i++) {
        double x = margin_left + chart_w * (double)i / (app->system_history_len - 1);
        double y = margin_top + chart_h * (temp_max - temp_data[i]) / (temp_max - temp_min);
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    // Linha de Uso da CPU (Azul)
    cairo_set_source_rgba(cr, 0.2, 0.2, 1.0, 0.9);
    for (int i = 0; i < len; i++) {
        double x = margin_left + chart_w * (double)i / (app->system_history_len - 1);
        double y = margin_top + chart_h * (1.0 - cpu_data[i]);
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    // --- Título e Legenda ---
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
    cairo_move_to(cr, margin_left, margin_top - 10);
    cairo_show_text(cr, "Monitor do Sistema");

    // Libera a memória alocada para os dados do gráfico
    free(temp_data);
    free(cpu_data);

    return FALSE;
}

/**
 * @brief Manipulador de desenho do Cairo para o gráfico de desempenho por thread.
 *
 * Esta função é chamada sempre que o widget `iters_drawing` precisa ser
 * redesenhado. Ele desenha um gráfico de linhas mostrando o desempenho histórico
 * (iterações por segundo) para cada thread de trabalho ao longo de uma janela de tempo deslizante.
 */
static gboolean on_draw_iters(GtkWidget *widget, cairo_t *cr, gpointer user_data){
    AppContext *app = (AppContext*)user_data;
    if (!app || !app->workers) return FALSE;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    const double W = alloc.width;
    const double H = alloc.height;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, THEME_BG_SECONDARY.a);
    draw_rounded_rect(cr, 0, 0, W, H, 8.0);
    cairo_fill(cr);

    if (!app->thread_history || app->history_len <= 1 || app->threads <= 0) {
        cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.8);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_text_extents_t ext;
        const char *msg = "Heatmap aguardando dados...";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, (W - ext.width)/2.0, (H + ext.height)/2.0);
        cairo_show_text(cr, msg);
        return FALSE;
    }

    const double margin_left = 70.0;
    const double margin_right = 90.0;
    const double margin_top = 24.0;
    const double margin_bottom = 36.0;

    const double heat_w = fmax(1.0, W - margin_left - margin_right);
    const double heat_h = fmax(1.0, H - margin_top - margin_bottom);
    const double cell_w = heat_w / app->history_len;
    const double cell_h = heat_h / app->threads;

    const int samples = app->history_len;
    const int threads = app->threads;
    const double sample_interval_sec = (double)CPU_SAMPLE_INTERVAL_MS / 1000.0;

    double *values = calloc((size_t)threads * samples, sizeof(double));
    gboolean *thread_active = calloc(threads, sizeof(gboolean));
    double min_val = DBL_MAX;
    double max_val = 0.0;

    g_mutex_lock(&app->history_mutex);
    int start_idx = (app->history_pos + 1) % samples;
    for (int t = 0; t < threads; t++) {
        for (int s = 0; s < samples; s++) {
            int idx = (start_idx + s) % samples;
            int prev_idx = (idx + samples - 1) % samples;
            unsigned current_v = app->thread_history[t][idx];
            unsigned prev_v = app->thread_history[t][prev_idx];
            unsigned diff = (current_v > prev_v) ? (current_v - prev_v) : 0;
            double metric = diff / sample_interval_sec;
            values[t * samples + s] = metric;
            if (metric > 0.0) {
                thread_active[t] = TRUE;
                if (metric > max_val) max_val = metric;
                if (metric < min_val) min_val = metric;
            }
        }
    }
    g_mutex_unlock(&app->history_mutex);

    if (min_val == DBL_MAX) {
        min_val = 0.0;
    }
    if (max_val < min_val) {
        max_val = min_val;
    }
    double range = (max_val > min_val) ? (max_val - min_val) : 1.0;

    cairo_save(cr);
    cairo_rectangle(cr, margin_left, margin_top, heat_w, heat_h);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, THEME_BG_TERTIARY.r, THEME_BG_TERTIARY.g, THEME_BG_TERTIARY.b, 0.8);
    cairo_paint(cr);

    for (int t = 0; t < threads; t++) {
        for (int s = 0; s < samples; s++) {
            double value = values[t * samples + s];
            double normalized = (value - min_val) / range;
            rgba_t c = heatmap_color(normalized);
            cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
            double x = margin_left + s * cell_w;
            double y = margin_top + t * cell_h;
            cairo_rectangle(cr, x, y, ceil(cell_w) + 1, ceil(cell_h) + 1);
            cairo_fill(cr);
        }
    }

    cairo_set_source_rgba(cr, THEME_GRID.r, THEME_GRID.g, THEME_GRID.b, 0.4);
    cairo_set_line_width(cr, 0.5);
    for (int t = 0; t <= threads; t++) {
        double y = margin_top + t * cell_h;
        cairo_move_to(cr, margin_left, y + 0.5);
        cairo_line_to(cr, margin_left + heat_w, y + 0.5);
    }
    for (int s = 0; s <= samples; s++) {
        double x = margin_left + s * cell_w;
        cairo_move_to(cr, x + 0.5, margin_top);
        cairo_line_to(cr, x + 0.5, margin_top + heat_h);
    }
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    for (int t = 0; t < threads; t++) {
        double text_y = margin_top + (t + 0.5) * cell_h + 4;
        cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
        char label[32];
        snprintf(label, sizeof(label), "T%d", t);
        cairo_move_to(cr, 12, text_y);
        cairo_show_text(cr, label);

        worker_status_t status = atomic_load(&app->workers[t].status);
        if (status == WORKER_ALLOC_FAIL) {
            cairo_set_source_rgba(cr, THEME_ERROR.r, THEME_ERROR.g, THEME_ERROR.b, 0.9);
            cairo_move_to(cr, 35, text_y);
            cairo_show_text(cr, "erro");
        } else if (!thread_active[t]) {
            cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.8);
            cairo_move_to(cr, 35, text_y);
            cairo_show_text(cr, "ocioso");
        }
    }

    cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.9);
    cairo_set_font_size(cr, 11);
    double time_span = ((double)samples * CPU_SAMPLE_INTERVAL_MS) / 1000.0;
    char time_label[64];
    if (time_span >= 10.0) snprintf(time_label, sizeof(time_label), "Janela de %.0f s", time_span);
    else snprintf(time_label, sizeof(time_label), "Janela de %.1f s", time_span);
    cairo_move_to(cr, margin_left, H - 12);
    cairo_show_text(cr, time_label);

    cairo_set_font_size(cr, 13);
    cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
    cairo_move_to(cr, margin_left, margin_top - 6);
    cairo_show_text(cr, "Histórico (mais recente à direita)");

    double legend_x = margin_left + heat_w + 20.0;
    double legend_y = margin_top;
    double legend_w = 20.0;
    double legend_h = heat_h;
    for (int i = 0; i < (int)legend_h; i++) {
        double ratio = 1.0 - ((double)i / legend_h);
        rgba_t c = heatmap_color(ratio);
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
        cairo_rectangle(cr, legend_x, legend_y + i, legend_w, 1.0);
        cairo_fill(cr);
    }
    cairo_rectangle(cr, legend_x, legend_y, legend_w, legend_h);
    cairo_set_source_rgba(cr, THEME_GRID.r, THEME_GRID.g, THEME_GRID.b, 1.0);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
    cairo_set_font_size(cr, 11);
    char max_label[64];
    char min_label[64];
    snprintf(max_label, sizeof(max_label), "%.0f it/s", max_val);
    snprintf(min_label, sizeof(min_label), "%.0f it/s", min_val);
    cairo_move_to(cr, legend_x + legend_w + 8, legend_y + 10);
    cairo_show_text(cr, max_label);
    cairo_move_to(cr, legend_x + legend_w + 8, legend_y + legend_h);
    cairo_show_text(cr, min_label);

    free(values);
    free(thread_active);
    return FALSE;
}
