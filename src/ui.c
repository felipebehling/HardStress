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

    // CPU Temperature List
    GtkWidget *cpu_frame = gtk_frame_new("Temperaturas dos Núcleos Físicos");
    app->cpu_drawing = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->cpu_drawing, -1, 220);
    gtk_container_add(GTK_CONTAINER(cpu_frame), app->cpu_drawing);
    gtk_box_pack_start(GTK_BOX(main_area), cpu_frame, FALSE, FALSE, 0);

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
 * @brief Cairo drawing handler for the CPU temperature list.
 *
 * Instead of a line chart, the widget now renders a list of all detected
 * physical cores with their current temperatures, making it easier to monitor
 * thermal headroom during a stress test.
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

    // Copy sensor readings under lock
    char **labels = NULL;
    double *temps = NULL;
    int sensor_count = 0;

    g_mutex_lock(&app->temp_mutex);
    if (app->core_temp_count > 0 && app->core_temp_labels && app->core_temps) {
        int src = app->core_temp_count;
        labels = g_new0(char*, src);
        temps = g_new(double, src);
        if (labels && temps) {
            int copied = 0;
            for (; copied < src; ++copied) {
                temps[copied] = app->core_temps[copied];
                const char *src_label = app->core_temp_labels[copied];
                if (!src_label) {
                    char fallback[32];
                    snprintf(fallback, sizeof(fallback), "Core %d", copied);
                    src_label = fallback;
                    labels[copied] = g_strdup(fallback);
                } else {
                    labels[copied] = g_strdup(src_label);
                }
                if (!labels[copied]) {
                    break;
                }
            }
            if (copied == src) {
                sensor_count = src;
            } else {
                for (int i = 0; i < copied; ++i) g_free(labels[i]);
                g_free(labels);
                g_free(temps);
                labels = NULL;
                temps = NULL;
            }
        } else {
            g_free(labels);
            g_free(temps);
            labels = NULL;
            temps = NULL;
        }
    } else if (app->temp_celsius > TEMP_UNAVAILABLE) {
        labels = g_new0(char*, 1);
        temps = g_new(double, 1);
        if (labels && temps) {
            labels[0] = g_strdup("Sensor térmico");
            if (labels[0]) {
                temps[0] = app->temp_celsius;
                sensor_count = 1;
            } else {
                g_free(labels);
                g_free(temps);
                labels = NULL;
                temps = NULL;
            }
        } else {
            g_free(labels);
            g_free(temps);
            labels = NULL;
            temps = NULL;
        }
    }
    g_mutex_unlock(&app->temp_mutex);

    const double margin = 20.0;
    const double row_height = 38.0;
    const double row_spacing = 8.0;
    const double row_width = w - (margin * 2.0);
    const double content_top = margin + 40.0;

    // Title
    cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, margin, margin + 18);
    cairo_show_text(cr, "Temperaturas por núcleo físico");

    // Subtitle
    cairo_set_source_rgba(cr, THEME_TEXT_SECONDARY.r, THEME_TEXT_SECONDARY.g, THEME_TEXT_SECONDARY.b, 0.9);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    char subtitle[64];
    if (sensor_count > 0) {
        snprintf(subtitle, sizeof(subtitle), "%d sensores monitorados", sensor_count);
    } else {
        snprintf(subtitle, sizeof(subtitle), "Sensores físicos indisponíveis");
    }
    cairo_move_to(cr, margin, margin + 36);
    cairo_show_text(cr, subtitle);

    if (sensor_count == 0) {
        cairo_set_source_rgba(cr, THEME_WARN.r, THEME_WARN.g, THEME_WARN.b, 0.8);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14);
        cairo_text_extents_t ext;
        const char *msg = "Nenhum núcleo físico com leitura de temperatura";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, (w - ext.width) / 2.0, h / 2.0 + ext.height / 2.0);
        cairo_show_text(cr, msg);
    } else {
        double max_temp = temps[0];
        double min_temp = temps[0];
        for (int i = 1; i < sensor_count; ++i) {
            if (temps[i] > max_temp) max_temp = temps[i];
            if (temps[i] < min_temp) min_temp = temps[i];
        }

        // Range badge
        cairo_set_source_rgba(cr, THEME_BG_TERTIARY.r, THEME_BG_TERTIARY.g, THEME_BG_TERTIARY.b, 0.9);
        double badge_width = 180;
        double badge_height = 26;
        double badge_x = w - badge_width - margin;
        double badge_y = margin + 12;
        draw_rounded_rect(cr, badge_x, badge_y, badge_width, badge_height, 13);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
        cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        char range_label[64];
        snprintf(range_label, sizeof(range_label), "Intervalo %.1f – %.1f °C", min_temp, max_temp);
        cairo_text_extents_t range_ext;
        cairo_text_extents(cr, range_label, &range_ext);
        cairo_move_to(cr, badge_x + (badge_width - range_ext.width) / 2.0, badge_y + badge_height - 9);
        cairo_show_text(cr, range_label);

        // Draw list rows
        for (int i = 0; i < sensor_count; ++i) {
            double row_y = content_top + i * (row_height + row_spacing);
            if (row_y > h - row_height) break;
            cairo_set_source_rgba(cr, THEME_BG_TERTIARY.r, THEME_BG_TERTIARY.g, THEME_BG_TERTIARY.b, 0.75);
            draw_rounded_rect(cr, margin, row_y, row_width, row_height, 10.0);
            cairo_fill(cr);

            // Label
            cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
            cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 12);
            const char *label = labels[i] ? labels[i] : "Núcleo";
            cairo_move_to(cr, margin + 14, row_y + row_height / 2.0 + 4);
            cairo_show_text(cr, label);

            // Bar gauge
            double bar_x = margin + 150;
            double bar_w = row_width - 220;
            if (bar_w < 50) bar_w = 50;
            double bar_h = row_height - 16;
            double bar_y = row_y + 8;
            cairo_set_source_rgba(cr, THEME_BG_SECONDARY.r, THEME_BG_SECONDARY.g, THEME_BG_SECONDARY.b, 0.9);
            draw_rounded_rect(cr, bar_x, bar_y, bar_w, bar_h, bar_h / 2.0);
            cairo_fill(cr);

            double normalized = (temps[i] - 25.0) / 80.0;
            if (normalized < 0.0) normalized = 0.0;
            if (normalized > 1.0) normalized = 1.0;
            double severity = (temps[i] - 60.0) / 35.0;
            if (severity < 0.0) severity = 0.0;
            if (severity > 1.0) severity = 1.0;
            double fill_w = bar_w * normalized;
            if (fill_w > 2.0) {
                cairo_set_source_rgba(cr,
                    THEME_ACCENT.r * (1.0 - severity) + THEME_ERROR.r * severity,
                    THEME_ACCENT.g * (1.0 - severity) + THEME_ERROR.g * severity,
                    THEME_ACCENT.b * (1.0 - severity) + THEME_ERROR.b * severity,
                    0.95);
                draw_rounded_rect(cr, bar_x, bar_y, fill_w, bar_h, bar_h / 2.0);
                cairo_fill(cr);
            }

            // Temperature value
            cairo_set_source_rgba(cr, THEME_TEXT_PRIMARY.r, THEME_TEXT_PRIMARY.g, THEME_TEXT_PRIMARY.b, 1.0);
            cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 13);
            char temp_label[32];
            snprintf(temp_label, sizeof(temp_label), "%.1f °C", temps[i]);
            cairo_text_extents_t temp_ext;
            cairo_text_extents(cr, temp_label, &temp_ext);
            cairo_move_to(cr, margin + row_width - temp_ext.width - 18, row_y + row_height / 2.0 + 4);
            cairo_show_text(cr, temp_label);
        }
    }
    g_free(temps);

    if (labels) {
        for (int i = 0; i < sensor_count; ++i) {
            g_free(labels[i]);
        }
        g_free(labels);
    }
    g_free(temps);

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
