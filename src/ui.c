/* ui.c - Modern HardStress Interface with KDE Plasma style */
#include "ui.h"
#include "core.h"
#include "metrics.h"
#include "utils.h"
#include <math.h>
#include <time.h>

/* --- Definições de Cores do Tema Escuro --- */
typedef struct {
    double r, g, b, a;
} rgba_t;

// Paleta de cores moderna inspirada no KDE Plasma
//static const rgba_t THEME_BG_PRIMARY = {0.118, 0.118, 0.180, 1.0};      // #1e1e2e - Fundo principal
static const rgba_t THEME_BG_SECONDARY = {0.157, 0.157, 0.227, 1.0};    // #28283a - Painéis
static const rgba_t THEME_BG_TERTIARY = {0.196, 0.196, 0.274, 1.0};     // #32324a - Elementos elevados
static const rgba_t THEME_ACCENT = {0.0, 0.749, 1.0, 1.0};              // #00bfff - Azul ciano vibrante
static const rgba_t THEME_ACCENT_DIM = {0.0, 0.498, 0.667, 1.0};        // #007faa - Azul mais escuro
static const rgba_t THEME_WARN = {0.976, 0.886, 0.686, 1.0};            // #f9e2af - Âmbar/Laranja
static const rgba_t THEME_ERROR = {0.949, 0.561, 0.678, 1.0};           // #f28fad - Vermelho claro
//static const rgba_t THEME_SUCCESS = {0.565, 0.933, 0.565, 1.0};         // #90ee90 - Verde claro
static const rgba_t THEME_TEXT_PRIMARY = {0.878, 0.878, 0.878, 1.0};    // #e0e0e0 - Texto principal
static const rgba_t THEME_TEXT_SECONDARY = {0.627, 0.627, 0.627, 1.0}; // #a0a0a0 - Texto secundário
static const rgba_t THEME_GRID = {0.235, 0.235, 0.314, 0.5};            // Grade sutil

/* --- Protótipos de Funções Estáticas --- */
static gboolean on_draw_cpu(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static gboolean on_draw_iters(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static void on_btn_start_clicked(GtkButton *b, gpointer ud);
static void on_btn_stop_clicked(GtkButton *b, gpointer ud);
static void on_btn_export_clicked(GtkButton *b, gpointer ud);
static gboolean on_window_delete(GtkWidget *w, GdkEvent *e, gpointer ud);
static void on_window_destroy(GtkWidget *w, gpointer ud);
static gboolean ui_tick(gpointer ud);
static void set_controls_sensitive(AppContext *app, gboolean state);
static void export_csv_dialog(AppContext *app);
static gboolean gui_update_started(gpointer ud);
static void apply_css_theme(GtkWidget *window);
static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r);
static void draw_grid_background(cairo_t *cr, int width, int height, int spacing);

gboolean gui_update_stopped(gpointer ud);

/* --- Implementações --- */

static void apply_css_theme(GtkWidget *window) {
    GtkCssProvider *provider = gtk_css_provider_new();
    // Tenta carregar de múltiplos locais, incluindo o diretório de desenvolvimento
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
        g_warning("Could not load 'style.css'. The appearance may be incorrect.");
        g_object_unref(provider);
        return;
    }
    
    GdkScreen *screen = gtk_widget_get_screen(window);
    gtk_style_context_add_provider_for_screen(screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider);
}

static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
    cairo_arc(cr, x + w - r, y + r, r, 1.5 * M_PI, 2 * M_PI);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, 0.5 * M_PI);
    cairo_arc(cr, x + r, y + h - r, r, 0.5 * M_PI, M_PI);
    cairo_close_path(cr);
}

static void draw_grid_background(cairo_t *cr, int width, int height, int spacing) {
    cairo_set_source_rgba(cr, THEME_GRID.r, THEME_GRID.g, THEME_GRID.b, THEME_GRID.a);
    cairo_set_line_width(cr, 0.5);
    
    // Linhas verticais
    for (int x = 0; x <= width; x += spacing) {
        cairo_move_to(cr, x + 0.5, 0);
        cairo_line_to(cr, x + 0.5, height);
    }
    
    // Linhas horizontais
    for (int y = 0; y <= height; y += spacing) {
        cairo_move_to(cr, 0, y + 0.5);
        cairo_line_to(cr, width, y + 0.5);
    }
    
    cairo_stroke(cr);
}

static void on_window_destroy(GtkWidget *w, gpointer ud) {
    (void)w;
    AppContext *app = (AppContext*)ud;

    if (atomic_load(&app->running)) {
        atomic_store(&app->running, 0);
    }
    g_mutex_clear(&app->cpu_mutex);
    g_mutex_clear(&app->history_mutex);
    g_mutex_clear(&app->temp_mutex);
    free(app);

    gtk_main_quit();
}

void gui_log(AppContext *app, const char *fmt, ...){
    va_list ap; va_start(ap, fmt);
    char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    if (!s) return;
    
    // Adiciona timestamp ao log
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S]", t);
    
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app->log_buffer, &end);
    gtk_text_buffer_insert(app->log_buffer, &end, timestamp, -1);
    gtk_text_buffer_insert(app->log_buffer, &end, " ", -1);
    gtk_text_buffer_insert(app->log_buffer, &end, s, -1);
    g_free(s);
}

static gboolean gui_update_started(gpointer ud){
    AppContext *app = (AppContext*)ud;
    gtk_widget_set_sensitive(app->btn_stop, TRUE);
    gtk_label_set_text(GTK_LABEL(app->status_label), "🚀 Running...");
    gui_log(app, "[GUI] Test started: threads=%d mem/thread=%zu dur=%ds pin=%d\n",
            app->threads, app->mem_mib_per_thread, app->duration_sec, app->pin_affinity);
    return G_SOURCE_REMOVE;
}

gboolean gui_update_stopped(gpointer ud){
    AppContext *app = (AppContext*)ud;
    set_controls_sensitive(app, TRUE);
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    gtk_label_set_text(GTK_LABEL(app->status_label), "⏹ Stopped");
    gui_log(app, "[GUI] Test stopped.\n");
    return G_SOURCE_REMOVE;
}

static void on_btn_start_clicked(GtkButton *b, gpointer ud){
    (void)b;
    AppContext *app = (AppContext*)ud;
    if (atomic_load(&app->running)) return;

    char *end;
    long threads = strtol(gtk_entry_get_text(GTK_ENTRY(app->entry_threads)), &end, 10);
    if (*end != '\0' || threads < 0){ gui_log(app, "[GUI] Invalid threads\n"); return; }
    long mem = strtol(gtk_entry_get_text(GTK_ENTRY(app->entry_mem)), &end, 10);
    if (*end != '\0' || mem <= 0){ gui_log(app, "[GUI] Invalid memory\n"); return; }
    long dur = strtol(gtk_entry_get_text(GTK_ENTRY(app->entry_dur)), &end, 10);
    if (*end != '\0' || dur < 0){ gui_log(app, "[GUI] Invalid duration\n"); return; }

    app->threads = (threads == 0) ? detect_cpu_count() : (int)threads;
    app->mem_mib_per_thread = (size_t)mem;
    app->duration_sec = (int)dur;
    app->pin_affinity = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_pin));
    app->kernel_fpu_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_fpu));
    app->kernel_int_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_int));
    app->kernel_stream_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_stream));
    app->kernel_ptr_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_ptr));
    app->csv_realtime_en = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->check_csv_realtime));

    if (!app->kernel_fpu_en && !app->kernel_int_en && !app->kernel_stream_en && !app->kernel_ptr_en) {
        gui_log(app, "[GUI] ERROR: At least one stress kernel must be selected.\n");
        return;
    }

    set_controls_sensitive(app, FALSE);
    g_idle_add(gui_update_started, app);
    thread_create(&app->controller_thread, (thread_func_t)controller_thread_func, app);
}

static void on_btn_stop_clicked(GtkButton *b, gpointer ud){
    (void)b;
    AppContext *app = (AppContext*)ud;
    if (!atomic_load(&app->running)) return;
    atomic_store(&app->running, 0);
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    gui_log(app, "[GUI] Stop requested by user\n");
}

static void on_btn_export_clicked(GtkButton *b, gpointer ud){
    (void)b;
    AppContext *app = (AppContext*)ud;
    export_csv_dialog(app);
}

static gboolean on_window_delete(GtkWidget *w, GdkEvent *e, gpointer ud){
    (void)w; (void)e;
    AppContext *app = (AppContext*)ud;
    if (atomic_load(&app->running)){
        gui_log(app, "[GUI] Closing: requesting stop...\n");
        atomic_store(&app->running, 0);
        struct timespec r = {1, 500000000}; nanosleep(&r,NULL);
    }
    return FALSE;
}

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

GtkWidget* create_main_window(AppContext *app) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 1400, 900);
    gtk_window_set_title(GTK_WINDOW(win), "HardStress - Advanced System Stress Testing");
    
    // Aplica o tema CSS
    apply_css_theme(win);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(win), main_box);

    // PAINEL LATERAL ESQUERDO
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_size_request(sidebar, 320, -1);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 20);
    gtk_box_pack_start(GTK_BOX(main_box), sidebar, FALSE, FALSE, 0);

    // Título do painel
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span font='Inter Bold 18' foreground='#00bfff'>HardStress</span>\n"
        "<span font='Inter 10' foreground='#a0a0a0'>Stress Testing System</span>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_LEFT);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sidebar), title, FALSE, FALSE, 0);

    // Frame de Configurações
    GtkWidget *config_frame = gtk_frame_new("Settings");
    GtkWidget *config_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(config_grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(config_grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(config_grid), 10);
    gtk_container_add(GTK_CONTAINER(config_frame), config_grid);
    gtk_box_pack_start(GTK_BOX(sidebar), config_frame, FALSE, FALSE, 0);

    int row = 0;
    
    // Threads
    GtkWidget *threads_label = gtk_label_new("Threads (0=auto):");
    gtk_widget_set_halign(threads_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), threads_label, 0, row, 1, 1);
    app->entry_threads = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->entry_threads), "0");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_threads), "Number of threads");
    gtk_grid_attach(GTK_GRID(config_grid), app->entry_threads, 1, row++, 1, 1);

    // Memória
    GtkWidget *mem_label = gtk_label_new("Memory (MiB/thread):");
    gtk_widget_set_halign(mem_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), mem_label, 0, row, 1, 1);
    app->entry_mem = gtk_entry_new();
    char mem_buf[32]; snprintf(mem_buf, sizeof(mem_buf), "%zu", app->mem_mib_per_thread);
    gtk_entry_set_text(GTK_ENTRY(app->entry_mem), mem_buf);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_mem), "Memory per thread");
    gtk_grid_attach(GTK_GRID(config_grid), app->entry_mem, 1, row++, 1, 1);

    // Duração
    GtkWidget *dur_label = gtk_label_new("Duration (s, 0=∞):");
    gtk_widget_set_halign(dur_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(config_grid), dur_label, 0, row, 1, 1);
    app->entry_dur = gtk_entry_new();
    char dur_buf[32]; snprintf(dur_buf, sizeof(dur_buf), "%d", app->duration_sec);
    gtk_entry_set_text(GTK_ENTRY(app->entry_dur), dur_buf);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_dur), "Time in seconds");
    gtk_grid_attach(GTK_GRID(config_grid), app->entry_dur, 1, row++, 1, 1);

    // Frame de Kernels
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

    // Opções adicionais
    GtkWidget *options_frame = gtk_frame_new("Options");
    GtkWidget *options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(options_box), 10);
    gtk_container_add(GTK_CONTAINER(options_frame), options_box);
    gtk_box_pack_start(GTK_BOX(sidebar), options_frame, FALSE, FALSE, 0);

    app->check_pin = gtk_check_button_new_with_label("Pin threads to CPUs");
    app->check_csv_realtime = gtk_check_button_new_with_label("Real-time CSV Log");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->check_pin), TRUE);
    
    gtk_box_pack_start(GTK_BOX(options_box), app->check_pin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(options_box), app->check_csv_realtime, FALSE, FALSE, 0);

    // Botões de controle
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->btn_start = gtk_button_new_with_label("▶ Start");
    app->btn_stop = gtk_button_new_with_label("⏹ Stop");
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    
    gtk_box_pack_start(GTK_BOX(button_box), app->btn_start, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), app->btn_stop, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), button_box, FALSE, FALSE, 0);

    app->btn_export = gtk_button_new_with_label("📊 Export CSV");
    gtk_box_pack_start(GTK_BOX(sidebar), app->btn_export, FALSE, FALSE, 0);

    // Status
    app->status_label = gtk_label_new("⏹ Ready");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->status_label), "status-label");
    gtk_box_pack_start(GTK_BOX(sidebar), app->status_label, FALSE, FALSE, 0);

    // ÁREA PRINCIPAL (DIREITA)
    GtkWidget *main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(main_area), 20);
    gtk_box_pack_start(GTK_BOX(main_box), main_area, TRUE, TRUE, 0);

    // Gráfico de CPU
    GtkWidget *cpu_frame = gtk_frame_new("CPU Utilization per Core");
    app->cpu_drawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->cpu_drawing, -1, 150);
    gtk_container_add(GTK_CONTAINER(cpu_frame), app->cpu_drawing);
    gtk_box_pack_start(GTK_BOX(main_area), cpu_frame, FALSE, FALSE, 0);

    // Gráfico de Iterações
    GtkWidget *iters_frame = gtk_frame_new("Performance per Thread (Iterations/s)");
    app->iters_drawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->iters_drawing, -1, 300);
    gtk_container_add(GTK_CONTAINER(iters_frame), app->iters_drawing);
    gtk_box_pack_start(GTK_BOX(main_area), iters_frame, FALSE, FALSE, 0);

    // Log
    GtkWidget *log_frame = gtk_frame_new("System Log");
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(log_frame), scrolled);
    gtk_box_pack_start(GTK_BOX(main_area), log_frame, TRUE, TRUE, 0);

    // Conectar sinais
    g_signal_connect(win, "destroy", G_CALLBACK(on_window_destroy), app);
    g_signal_connect(win, "delete-event", G_CALLBACK(on_window_delete), app);
    g_signal_connect(app->btn_start, "clicked", G_CALLBACK(on_btn_start_clicked), app);
    g_signal_connect(app->btn_stop, "clicked", G_CALLBACK(on_btn_stop_clicked), app);
    g_signal_connect(app->btn_export, "clicked", G_CALLBACK(on_btn_export_clicked), app);
    g_signal_connect(app->cpu_drawing, "draw", G_CALLBACK(on_draw_cpu), app);
    g_signal_connect(app->iters_drawing, "draw", G_CALLBACK(on_draw_iters), app);

    // Timer para atualizar o status
    g_timeout_add(1000, ui_tick, app);

    return win;
}

static void set_controls_sensitive(AppContext *app, gboolean state){
    gtk_widget_set_sensitive(app->entry_threads, state);
    gtk_widget_set_sensitive(app->entry_mem, state);
    gtk_widget_set_sensitive(app->entry_dur, state);
    gtk_widget_set_sensitive(app->check_pin, state);
    gtk_widget_set_sensitive(app->check_fpu, state);
    gtk_widget_set_sensitive(app->check_int, state);
    gtk_widget_set_sensitive(app->check_stream, state);
    gtk_widget_set_sensitive(app->check_ptr, state);
    gtk_widget_set_sensitive(app->check_csv_realtime, state);
    gtk_widget_set_sensitive(app->btn_start, state);
}

static void export_csv_dialog(AppContext *app){
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Export History to CSV",
        GTK_WINDOW(app->win),
        GTK_FILE_CHOOSER_ACTION_SAVE, 
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT, NULL);

    char default_name[64];
    snprintf(default_name, sizeof(default_name), "HardStress_Export_%.0f.csv", (double)time(NULL));
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_name);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT){
        char *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        FILE *f = fopen(fname, "w");
        if (!f){
            gui_log(app, "[GUI] ERROR: Failed to open %s for writing\n", fname);
            g_free(fname);
            gtk_widget_destroy(dialog);
            return;
        }

        fprintf(f, "timestamp_sec");
        for (int t=0; t<app->threads; t++) fprintf(f, ",thread%d_iters_total", t);
        fprintf(f, "\n");

        g_mutex_lock(&app->history_mutex);
        if(app->thread_history) {
            for (int s=0; s<app->history_len; s++){
                int idx = (app->history_pos + 1 + s) % app->history_len;
                fprintf(f, "%.3f", (double)s * (CPU_SAMPLE_INTERVAL_MS / 1000.0));
                for (int t=0; t<app->threads; t++) {
                    fprintf(f, ",%u", app->thread_history[t][idx]);
                }
                fprintf(f, "\n");
            }
        }
        g_mutex_unlock(&app->history_mutex);

        fclose(f);
        gui_log(app, "[GUI] CSV (memory snapshot) exported to %s\n", fname);
        g_free(fname);
    }
    gtk_widget_destroy(dialog);
}

static gboolean on_draw_cpu(GtkWidget *widget, cairo_t *cr, gpointer user_data){
    AppContext *app = (AppContext*)user_data;
    GtkAllocation alloc; 
    gtk_widget_get_allocation(widget, &alloc);
    int w = alloc.width, h = alloc.height;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    // Fundo
    cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, THEME_BG_SECONDARY.a);
    draw_rounded_rect(cr, 0, 0, w, h, 8.0);
    cairo_fill(cr);

    // Grade
    draw_grid_background(cr, w, h - 25, 20);

    int n = app->cpu_count > 0 ? app->cpu_count : 1;
    double spacing = 8.0;
    double bw = (w - (n + 1) * spacing) / n;

    g_mutex_lock(&app->cpu_mutex);
    for (int i=0; i<n; i++){
        double u = (app->cpu_usage && i < app->cpu_count) ? app->cpu_usage[i] : 0.0;
        double x = spacing + i * (bw + spacing);
        double bar_h = u * (h - 35); // Espaço para textos

        // Barra de fundo
        cairo_set_source_rgba(cr, THEME_BG_TERTIARY.r, THEME_BG_TERTIARY.g, THEME_BG_TERTIARY.b, 0.7);
        draw_rounded_rect(cr, x, 10, bw, h - 35, 6.0);
        cairo_fill(cr);
        
        // Barra de uso
        if(bar_h > 0) {
            cairo_pattern_t *pat = cairo_pattern_create_linear(x, h, x, h - bar_h);
            cairo_pattern_add_color_stop_rgba(pat, 0, THEME_ACCENT_DIM.r, THEME_ACCENT_DIM.g, THEME_ACCENT_DIM.b, THEME_ACCENT_DIM.a);
            cairo_pattern_add_color_stop_rgba(pat, 1, THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b, THEME_ACCENT.a);
            cairo_set_source(cr, pat);
            draw_rounded_rect(cr, x, (h - 25) - bar_h, bw, bar_h, 6.0);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);
        }

        // Texto da porcentagem sobre a barra
        char txt[32]; 
        snprintf(txt, sizeof(txt), "%.0f%%", u * 100.0);
        cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 0.9);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, txt, &extents);
        cairo_move_to(cr, x + (bw / 2.0) - (extents.width / 2.0), 25); 
        cairo_show_text(cr, txt);

        // Rótulo do núcleo abaixo da barra
        snprintf(txt, sizeof(txt), "CPU %d", i);
        cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 1.0);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10);
        cairo_text_extents(cr, txt, &extents);
        cairo_move_to(cr, x + (bw / 2.0) - (extents.width / 2.0), h - 8);
        cairo_show_text(cr, txt);
    }
    g_mutex_unlock(&app->cpu_mutex);

    // Display de Temperatura
    g_mutex_lock(&app->temp_mutex);
    double temp = app->temp_celsius;
    g_mutex_unlock(&app->temp_mutex);

    if (temp > TEMP_UNAVAILABLE){
        char tbuf[64]; 
        snprintf(tbuf, sizeof(tbuf), "🌡️ %.1f °C", temp);
        cairo_set_source_rgba(cr, THEME_WARN.r, THEME_WARN.g, THEME_WARN.b, 1.0);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, tbuf, &extents);
        cairo_move_to(cr, w - extents.width - 15, 20);
        cairo_show_text(cr, tbuf);
    }
    return FALSE;
}

static gboolean on_draw_iters(GtkWidget *widget, cairo_t *cr, gpointer user_data){
    AppContext *app = (AppContext*)user_data;
    if (!atomic_load(&app->running) || !app->workers) return FALSE;

    GtkAllocation alloc; 
    gtk_widget_get_allocation(widget, &alloc);
    int W = alloc.width, H = alloc.height;
    
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    // Fundo
    cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, THEME_BG_SECONDARY.a);
    draw_rounded_rect(cr, 0, 0, W, H, 8.0);
    cairo_fill(cr);
    
    // Grade
    draw_grid_background(cr, W, H, 30);

    const rgba_t thread_colors[] = {
        {0.2, 0.6, 1.0, 0.8}, {0.1, 0.9, 0.7, 0.8}, {1.0, 0.8, 0.2, 0.8}, {0.9, 0.3, 0.4, 0.8},
        {0.6, 0.4, 1.0, 0.8}, {0.2, 0.9, 0.2, 0.8}, {1.0, 0.5, 0.1, 0.8}, {0.9, 0.1, 0.8, 0.8}
    };
    const int num_colors = sizeof(thread_colors) / sizeof(rgba_t);

    g_mutex_lock(&app->history_mutex);
    for (int t=0; t < app->threads; t++){
        worker_status_t status = atomic_load(&app->workers[t].status);

        if (status == WORKER_ALLOC_FAIL) {
            cairo_set_source_rgba(cr, THEME_ERROR.r, THEME_ERROR.g, THEME_ERROR.b, 1.0);
            cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 16);
            cairo_text_extents_t extents;
            cairo_text_extents(cr, "ALLOCATION FAILURE", &extents);
            cairo_move_to(cr, W/2.0 - extents.width/2.0, H/2.0 + extents.height/2.0);
            cairo_show_text(cr, "ALLOCATION FAILURE");
            break; // Mostra apenas a falha
        }
        
        const rgba_t c = thread_colors[t % num_colors];
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
        cairo_set_line_width(cr, 2.5);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        
        int samples = app->history_len;
        double step_x = (samples > 1) ? ((double)W / (samples - 1)) : W;
        int start_idx = (app->history_pos + 1) % samples;
        unsigned last_v = app->thread_history ? app->thread_history[t][start_idx] : 0;
        
        cairo_move_to(cr, -10, H + 10); // Inicia fora da tela

        for (int s = 0; s < samples; s++) {
            int current_idx = (start_idx + s) % samples;
            unsigned current_v = app->thread_history ? app->thread_history[t][current_idx] : 0;
            unsigned diff = (current_v > last_v) ? (current_v - last_v) : 0;
            
            // Normaliza o valor para a altura do gráfico
            double y_val = ((double)diff) / (ITER_SCALE * (CPU_SAMPLE_INTERVAL_MS / 1000.0));
            double y = H - y_val * H;
            y = (y < 0) ? 0 : y;
            y = (y > H) ? H : y;
            
            cairo_line_to(cr, s * step_x, y);
            last_v = current_v;
        }
        cairo_stroke(cr);
    }
    g_mutex_unlock(&app->history_mutex);

    // Legenda
    for (int t=0; t < app->threads; t++) {
        const rgba_t c = thread_colors[t % num_colors];
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
        cairo_rectangle(cr, 15, 15 + t * 20, 12, 12);
        cairo_fill(cr);

        char lbl[32];
        snprintf(lbl, sizeof(lbl), "Thread %d", t);
        cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11);
        cairo_move_to(cr, 35, 25 + t * 20);
        cairo_show_text(cr, lbl);
    }
    
    return FALSE;
}
