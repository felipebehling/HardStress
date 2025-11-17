/* ui.c - Modernized HardStress interface with a KDE Plasma-inspired style */
#include "ui.h"
#include "core.h"
#include "metrics.h"
#include "utils.h"
#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

/* --- Dark Theme Color Definitions --- */
typedef struct {
    double r, g, b, a;
} rgba_t;

// Modern color palette inspired by KDE Plasma
//static const rgba_t THEME_BG_PRIMARY = {0.118, 0.118, 0.180, 1.0};      // #1e1e2e - Main background
static const rgba_t THEME_BG_SECONDARY = {0.157, 0.157, 0.227, 1.0};    // #28283a - Panels
static const rgba_t THEME_BG_TERTIARY = {0.196, 0.196, 0.274, 1.0};     // #32324a - Elevated elements
static const rgba_t THEME_ACCENT = {0.0, 0.749, 1.0, 1.0};              // #00bfff - Vibrant cyan blue
static const rgba_t THEME_ACCENT_DIM = {0.0, 0.498, 0.667, 1.0};        // #007faa - Darker blue
static const rgba_t THEME_WARN = {0.976, 0.886, 0.686, 1.0};            // #f9e2af - Amber/Orange
static const rgba_t THEME_ERROR = {0.949, 0.561, 0.678, 1.0};           // #f28fad - Light red
//static const rgba_t THEME_SUCCESS = {0.565, 0.933, 0.565, 1.0};         // #90ee90 - Light green
static const rgba_t THEME_TEXT_PRIMARY = {0.878, 0.878, 0.878, 1.0};    // #e0e0e0 - Primary text
static const rgba_t THEME_TEXT_SECONDARY = {0.627, 0.627, 0.627, 1.0}; // #a0a0a0 - Secondary text
static const rgba_t THEME_GRID = {0.235, 0.235, 0.314, 0.5};            // Subtle grid

/* --- Static Function Prototypes --- */
static gboolean on_draw_cpu(GtkWidget *widget, cairo_t *cr, gpointer user_data);
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
 * @brief Applies the application's CSS theme.
 *
 * It tries to load `style.css` from several possible locations to support
 * both development and installed environments.
 * @param window The main application window.
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
 * @brief Cairo helper function to draw a rectangle with rounded corners.
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
 * @brief Cairo helper function to draw a grid background pattern.
 */
static void draw_grid_background(cairo_t *cr, int width, int height, int spacing) {
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

/**
 * @brief Callback for the window's "destroy" event.
 *
 * This function cleans up all application resources, including mutexes and
 * the main AppContext struct, before quitting the GTK main loop.
 */
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

#ifndef TESTING_BUILD
/**
 * @brief Appends a formatted, timestamped message to the GUI log.
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
 * @brief GSourceFunc to update the GUI when a test starts.
 *
 * Called via `g_idle_add` to safely update GTK widgets from the main thread.
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
 * @brief GSourceFunc to update the GUI when a test stops.
 *
 * Called via `g_idle_add` to safely update GTK widgets from the main thread.
 */
gboolean gui_update_stopped(gpointer ud){
    AppContext *app = (AppContext*)ud;
    set_controls_sensitive(app, TRUE);
    gtk_widget_set_sensitive(app->btn_stop, FALSE);
    gtk_label_set_text(GTK_LABEL(app->status_label), "⏹ Stopped");
    gui_log(app, "[GUI] Test stopped.\n");
    return G_SOURCE_REMOVE;
}

/**
 * @brief Callback for the "Start" button click event.
 *
 * Parses and validates user input, configures the AppContext, disables
 * configuration controls, and spawns the main controller thread to begin
 * the stress test.
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

    set_controls_sensitive(app, FALSE);
    g_idle_add(gui_update_started, app);
    thread_create(&app->controller_thread, controller_thread_func, app);
}

/**
 * @brief Callback for the "Stop" button click event.
 *
 * Signals the controller thread to terminate the currently running test.
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
 * @brief Callback for the "Restore Defaults" button.
 *
 * Resets all configuration options in the UI to their default values.
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
 * @brief Callback for the "Clear Log" button.
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
 * @brief Callback for the window "delete" (close button) event.
 *
 * If a test is running, it signals it to stop before allowing the window
 * to close, preventing an abrupt termination.
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
 * @brief A timer callback (tick) that runs once per second.
 *
 * Updates the main status label with real-time performance data like
 * total iterations per second and error count.
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
 * @brief Builds and returns the main application window with all its widgets.
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

    // CPU Graph
    GtkWidget *cpu_frame = gtk_frame_new("CPU Utilization per Core");
    app->cpu_drawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->cpu_drawing, -1, 150);
    gtk_container_add(GTK_CONTAINER(cpu_frame), app->cpu_drawing);
    gtk_box_pack_start(GTK_BOX(main_area), cpu_frame, FALSE, FALSE, 0);

    // Iterations Graph
    GtkWidget *iters_frame = gtk_frame_new("Performance per Thread (Iterations/s)");
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
    g_signal_connect(app->cpu_drawing, "draw", G_CALLBACK(on_draw_cpu), app);
    g_signal_connect(app->iters_drawing, "draw", G_CALLBACK(on_draw_iters), app);

    // Timer to update the status label
    g_timeout_add(1000, ui_tick, app);

    return win;
}

/**
 * @brief Enables or disables the sensitivity of configuration controls.
 *
 * This is used to prevent the user from changing settings while a test is
 * running.
 * @param app The application context.
 * @param state `TRUE` to enable controls, `FALSE` to disable.
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
 * @brief Cairo drawing handler for the CPU utilization graph.
 *
 * This function is called whenever the `cpu_drawing` widget needs to be
 * repainted. It renders a history graph inspired by the Windows Task Manager,
 * plotting per-core utilization over time and displaying the current system
 * temperature.
 */
static gboolean on_draw_cpu(GtkWidget *widget, cairo_t *cr, gpointer user_data){
    AppContext *app = (AppContext*)user_data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    const int w = alloc.width;
    const int h = alloc.height;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    // Base background
    cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, THEME_BG_SECONDARY.a);
    draw_rounded_rect(cr, 0, 0, w, h, 8.0);
    cairo_fill(cr);

    const double margin_left = 60.0;
    const double margin_right = 22.0;
    const double margin_top = 22.0;
    const double margin_bottom = 48.0;

    double graph_w = w - margin_left - margin_right;
    double graph_h = h - margin_top - margin_bottom;
    if (graph_w < 1.0) graph_w = 1.0;
    if (graph_h < 1.0) graph_h = 1.0;

    cairo_save(cr);
    cairo_translate(cr, margin_left, margin_top);
    cairo_set_source_rgba(cr, THEME_BG_TERTIARY.r, THEME_BG_TERTIARY.g, THEME_BG_TERTIARY.b, 0.85);
    cairo_rectangle(cr, 0, 0, graph_w, graph_h);
    cairo_fill(cr);
    draw_grid_background(cr, (int)graph_w, (int)graph_h, 30);
    cairo_restore(cr);

    const rgba_t cpu_colors[] = {
        {0.2, 0.6, 1.0, 0.65}, {0.1, 0.9, 0.7, 0.65}, {1.0, 0.8, 0.2, 0.65}, {0.9, 0.3, 0.4, 0.65},
        {0.6, 0.4, 1.0, 0.65}, {0.2, 0.9, 0.2, 0.65}, {1.0, 0.5, 0.1, 0.65}, {0.9, 0.1, 0.8, 0.65}
    };
    const int num_colors = sizeof(cpu_colors) / sizeof(cpu_colors[0]);

    int cpu_count = 0;
    int sample_count = 0;
    int history_len = 0;
    int history_pos = 0;
    double average_usage = 0.0;
    double latest_mean = 0.0;
    double marker_canvas_y = margin_top + graph_h;
    double last_point_x = margin_left + graph_w;

    g_mutex_lock(&app->cpu_mutex);
    cpu_count = app->cpu_count;
    history_len = app->cpu_history_len;
    history_pos = app->cpu_history_pos;
    if (app->cpu_history && app->cpu_history_len > 0) {
        sample_count = app->cpu_history_filled < app->cpu_history_len ? app->cpu_history_filled : app->cpu_history_len;
    }
    if (cpu_count > 0 && app->cpu_usage) {
        for (int c = 0; c < cpu_count; c++) {
            average_usage += app->cpu_usage[c];
        }
        average_usage /= cpu_count;
    }

    if (sample_count > 0 && app->cpu_history && cpu_count > 0 && history_len > 0) {
        cairo_save(cr);
        cairo_translate(cr, margin_left, margin_top);

        double base_x = (sample_count > 1) ? 0.0 : graph_w;
        cairo_new_path(cr);
        cairo_move_to(cr, base_x, graph_h);
        for (int s = 0; s < sample_count; s++) {
            int idx = history_pos - (sample_count - 1 - s);
            while (idx < 0) idx += history_len;
            double mean = 0.0;
            for (int c = 0; c < cpu_count; c++) {
                if (app->cpu_history[c]) {
                    mean += app->cpu_history[c][idx];
                }
            }
            mean = (cpu_count > 0) ? mean / cpu_count : 0.0;
            if (mean < 0.0) mean = 0.0;
            if (mean > 1.0) mean = 1.0;
            double x = (sample_count > 1) ? ((double)s / (sample_count - 1)) * graph_w : graph_w;
            double y = graph_h - (mean * graph_h);
            cairo_line_to(cr, x, y);
            if (s == sample_count - 1) {
                latest_mean = mean;
            }
        }
        cairo_line_to(cr, graph_w, graph_h);
        cairo_close_path(cr);
        cairo_set_source_rgba(cr, THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b, 0.2);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b, 0.95);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);

        for (int c = 0; c < cpu_count; c++) {
            if (!app->cpu_history[c]) continue;
            cairo_new_path(cr);
            for (int s = 0; s < sample_count; s++) {
                int idx = history_pos - (sample_count - 1 - s);
                while (idx < 0) idx += history_len;
                double value = app->cpu_history[c][idx];
                if (value < 0.0) value = 0.0;
                if (value > 1.0) value = 1.0;
                double x = (sample_count > 1) ? ((double)s / (sample_count - 1)) * graph_w : graph_w;
                double y = graph_h - (value * graph_h);
                if (s == 0) {
                    cairo_move_to(cr, x, y);
                } else {
                    cairo_line_to(cr, x, y);
                }
            }
            cairo_set_line_width(cr, 1.0);
            const rgba_t color = cpu_colors[c % num_colors];
            cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
            cairo_stroke(cr);
        }

        cairo_set_source_rgba(cr, THEME_BG_TERTIARY.r, THEME_BG_TERTIARY.g, THEME_BG_TERTIARY.b, 0.8);
        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, 0.5, 0.5, graph_w - 1.0, graph_h - 1.0);
        cairo_stroke(cr);

        double marker_y = graph_h - (latest_mean * graph_h);
        marker_canvas_y = margin_top + marker_y;
        cairo_set_source_rgba(cr, THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b, 0.4);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0, marker_y);
        cairo_line_to(cr, graph_w, marker_y);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b, 0.85);
        cairo_arc(cr, graph_w, marker_y, 3.5, 0, 2 * M_PI);
        cairo_fill(cr);

        cairo_restore(cr);
    } else {
        latest_mean = average_usage;
        marker_canvas_y = margin_top + (graph_h - (latest_mean * graph_h));
    }

    int drawn_samples = sample_count;
    int logical_cores = cpu_count;
    double usage_percent = average_usage;
    double marker_percent = latest_mean;

    g_mutex_unlock(&app->cpu_mutex);

    // Axis ticks on the left
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, THEME_GRID.r, THEME_GRID.g, THEME_GRID.b, 0.85);
    cairo_new_path(cr);
    for (int i = 0; i <= 4; i++) {
        double ratio = (double)i / 4.0;
        double y = margin_top + graph_h - (ratio * graph_h);
        cairo_move_to(cr, margin_left - 6, y + 0.5);
        cairo_line_to(cr, margin_left, y + 0.5);
    }
    cairo_stroke(cr);

    // Percentage labels for the axis
    cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.9);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    for (int i = 0; i <= 4; i++) {
        double ratio = (double)i / 4.0;
        double percent = ratio * 100.0;
        double y = margin_top + graph_h - (ratio * graph_h);
        char label[16];
        snprintf(label, sizeof(label), "%.0f%%", percent);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, label, &ext);
        cairo_move_to(cr, margin_left - ext.width - 10, y + ext.height / 2.5);
        cairo_show_text(cr, label);
    }

    // Headline usage text
    cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    char usage_label[64];
    snprintf(usage_label, sizeof(usage_label), "Uso da CPU %.0f%%", usage_percent * 100.0);
    cairo_move_to(cr, margin_left + 14, margin_top + 24);
    cairo_show_text(cr, usage_label);

    // CPU count text
    cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.9);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    char cores_label[64];
    snprintf(cores_label, sizeof(cores_label), "Processadores lógicos: %d", logical_cores > 0 ? logical_cores : 0);
    cairo_move_to(cr, margin_left + 14, margin_top + 42);
    cairo_show_text(cr, cores_label);

    // Marker label near the latest data point
    cairo_set_source_rgba(cr, THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b, 0.9);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    char marker_label[16];
    snprintf(marker_label, sizeof(marker_label), "%.0f%%", marker_percent * 100.0);
    cairo_text_extents_t marker_ext;
    cairo_text_extents(cr, marker_label, &marker_ext);
    double marker_text_x = last_point_x - marker_ext.width - 8;
    if (marker_text_x < margin_left + 6) marker_text_x = margin_left + 6;
    double marker_text_y = marker_canvas_y - 6;
    if (marker_text_y < margin_top + 16) marker_text_y = margin_top + 16;
    if (marker_text_y > margin_top + graph_h - 4) marker_text_y = margin_top + graph_h - 4;
    cairo_move_to(cr, marker_text_x, marker_text_y);
    cairo_show_text(cr, marker_label);

    // Time range label at the bottom right
    double sample_interval_sec = (double)CPU_SAMPLE_INTERVAL_MS / 1000.0;
    double range_seconds = sample_interval_sec * (drawn_samples > 0 ? drawn_samples : 1);
    char range_label[64];
    if (range_seconds >= 10.0) {
        snprintf(range_label, sizeof(range_label), "Últimos %.0f s", range_seconds);
    } else {
        snprintf(range_label, sizeof(range_label), "Últimos %.1f s", range_seconds);
    }
    cairo_set_source_rgba(cr, THEME_ACCENT_DIM.r, THEME_ACCENT_DIM.g, THEME_ACCENT_DIM.b, 0.9);
    cairo_text_extents_t range_ext;
    cairo_text_extents(cr, range_label, &range_ext);
    cairo_move_to(cr, margin_left + graph_w - range_ext.width, margin_top + graph_h + 28);
    cairo_show_text(cr, range_label);

    // Temperature indicator in the top-right corner of the graph
    g_mutex_lock(&app->temp_mutex);
    double temp = app->temp_celsius;
    g_mutex_unlock(&app->temp_mutex);

    if (temp > TEMP_UNAVAILABLE) {
        char temp_label[64];
        snprintf(temp_label, sizeof(temp_label), "🌡️ %.1f °C", temp);
        cairo_set_source_rgba(cr, THEME_WARN.r, THEME_WARN.g, THEME_WARN.b, 1.0);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        cairo_text_extents_t temp_ext;
        cairo_text_extents(cr, temp_label, &temp_ext);
        cairo_move_to(cr, margin_left + graph_w - temp_ext.width - 10, margin_top + 20);
        cairo_show_text(cr, temp_label);
    }

    return FALSE;
}

/**
 * @brief Cairo drawing handler for the per-thread performance graph.
 *
 * This function is called whenever the `iters_drawing` widget needs to be
 * repainted. It draws a line chart showing the historical performance
 * (iterations per second) for each worker thread over a sliding time window.
 */
static gboolean on_draw_iters(GtkWidget *widget, cairo_t *cr, gpointer user_data){
    AppContext *app = (AppContext*)user_data;
    if (!atomic_load(&app->running) || !app->workers) return FALSE;

    GtkAllocation alloc; 
    gtk_widget_get_allocation(widget, &alloc);
    int W = alloc.width, H = alloc.height;
    
    // Divide the area: 70% for the graph, 30% for the legend table
    int graph_W = W * 0.7;
    int table_X = graph_W + 10;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    // Background
    cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, THEME_BG_SECONDARY.a);
    draw_rounded_rect(cr, 0, 0, W, H, 8.0);
    cairo_fill(cr);
    
    // Grid for the graph area
    draw_grid_background(cr, graph_W, H, 30);

    const rgba_t thread_colors[] = {
        {0.2, 0.6, 1.0, 0.8}, {0.1, 0.9, 0.7, 0.8}, {1.0, 0.8, 0.2, 0.8}, {0.9, 0.3, 0.4, 0.8},
        {0.6, 0.4, 1.0, 0.8}, {0.2, 0.9, 0.2, 0.8}, {1.0, 0.5, 0.1, 0.8}, {0.9, 0.1, 0.8, 0.8}
    };
    const int num_colors = sizeof(thread_colors) / sizeof(rgba_t);

    unsigned long long total_diff = 0;
    unsigned* diffs = calloc(app->threads, sizeof(unsigned));

    g_mutex_lock(&app->history_mutex);

    // First pass: calculate diffs and total_diff
    int samples = app->history_len;
    int start_idx = (app->history_pos + 1) % samples;
    int end_idx = app->history_pos;

    if (app->thread_history) {
        for (int t = 0; t < app->threads; t++) {
            unsigned start_v = app->thread_history[t][start_idx];
            unsigned end_v = app->thread_history[t][end_idx];
            unsigned diff = (end_v > start_v) ? (end_v - start_v) : 0;
            if (diffs) diffs[t] = diff;
            total_diff += diff;
        }
    }

    // Second pass: draw graphs
    for (int t=0; t < app->threads; t++){
        worker_status_t status = atomic_load(&app->workers[t].status);

        if (status == WORKER_ALLOC_FAIL) {
            cairo_set_source_rgba(cr, THEME_ERROR.r, THEME_ERROR.g, THEME_ERROR.b, 1.0);
            cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 16);
            cairo_text_extents_t extents;
            cairo_text_extents(cr, "ALLOCATION FAILED", &extents);
            cairo_move_to(cr, W/2.0 - extents.width/2.0, H/2.0 + extents.height/2.0);
            cairo_show_text(cr, "ALLOCATION FAILED");
            break;
        }
        
        const rgba_t c = thread_colors[t % num_colors];
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
        cairo_set_line_width(cr, 2.5);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        
        double step_x = (samples > 1) ? ((double)graph_W / (samples - 1)) : graph_W;
        
        unsigned last_v = app->thread_history ? app->thread_history[t][start_idx] : 0;

        cairo_move_to(cr, -10, H + 10);

        for (int s = 0; s < samples; s++) {
            int current_idx = (start_idx + s) % samples;
            unsigned current_v = app->thread_history ? app->thread_history[t][current_idx] : 0;
            unsigned diff_hist = (current_v > last_v) ? (current_v - last_v) : 0;
            
            double sample_interval_sec = (double)CPU_SAMPLE_INTERVAL_MS / 1000.0;
            double y_val = ((double)diff_hist / sample_interval_sec) / ITER_SCALE;
            double y = H - y_val * H;
            y = fmax(0, fmin(H, y));
            
            cairo_line_to(cr, s * step_x, y);
            last_v = current_v;
        }
        cairo_stroke(cr);
    }
    g_mutex_unlock(&app->history_mutex);

    // --- Legend Table ---
    double y_pos = 25;
    double row_h = 22;
    // Header
    cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 1.0);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);
    cairo_move_to(cr, table_X + 25, y_pos); cairo_show_text(cr, "Thread");
    cairo_move_to(cr, table_X + 90, y_pos); cairo_show_text(cr, "Iters/s");
    cairo_move_to(cr, table_X + 160, y_pos); cairo_show_text(cr, "Contrib.");
    y_pos += row_h;

    // Rows
    for (int t=0; t < app->threads; t++) {
        const rgba_t c = thread_colors[t % num_colors];
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
        cairo_rectangle(cr, table_X + 5, y_pos - 12, 12, 12);
        cairo_fill(cr);

        char lbl[64];
        cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11);

        // Thread ID
        snprintf(lbl, sizeof(lbl), "%d", t);
        cairo_move_to(cr, table_X + 25, y_pos);
        cairo_show_text(cr, lbl);

        // Iters/s (raw number)
        double sample_interval_sec = (double)CPU_SAMPLE_INTERVAL_MS / 1000.0;
        double iters_s = diffs ? ((double)diffs[t] / sample_interval_sec) : 0.0;
        snprintf(lbl, sizeof(lbl), "%.0f", iters_s);
        cairo_move_to(cr, table_X + 90, y_pos);
        cairo_show_text(cr, lbl);

        // Contribution (%)
        double percentage = (total_diff > 0 && diffs) ? ((double)diffs[t] * 100.0 / total_diff) : 0.0;
        snprintf(lbl, sizeof(lbl), "%.1f%%", percentage);
        cairo_move_to(cr, table_X + 160, y_pos);
        cairo_show_text(cr, lbl);

        y_pos += row_h;
    }
    
    free(diffs);
    return FALSE;
}
