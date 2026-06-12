cat > src/ihm.c <<'EOF'
#include "ihm.h"
#include "calcul.h"

#include <gtk/gtk.h>
#include <stdio.h>

typedef struct {
    const char *port;
    int baud;
} LaunchConfig;

typedef struct {
    AppContext *ctx;

    GtkWidget *temperature_label;
    GtkWidget *humidite_label;
    GtkWidget *seuil_label;
    GtkWidget *connection_label;
    GtkWidget *status_label;

    GtkWidget *freq_spin;
    GtkWidget *seuil_spin;

    guint gui_timer_id;
} AppData;

static void update_labels(AppData *data) {
    SensorSnapshot snapshot;
    app_get_snapshot(data->ctx, &snapshot);

    char text[256];

    snprintf(text, sizeof(text), "Connexion : %s (%s, %d baud)",
             snapshot.connected ? "connecté" : "non connecté",
             snapshot.port,
             snapshot.baud);
    gtk_label_set_text(GTK_LABEL(data->connection_label), text);

    if (snapshot.has_temperature) {
        snprintf(text, sizeof(text), "Température : %.2f °C", snapshot.temperature);
    } else {
        snprintf(text, sizeof(text), "Température : —");
    }
    gtk_label_set_text(GTK_LABEL(data->temperature_label), text);

    if (snapshot.has_humidite) {
        snprintf(text, sizeof(text), "Humidité : %.2f %%", snapshot.humidite);
    } else {
        snprintf(text, sizeof(text), "Humidité : —");
    }
    gtk_label_set_text(GTK_LABEL(data->humidite_label), text);

    if (snapshot.has_seuil) {
        snprintf(text, sizeof(text), "Seuil actuel : %.2f °C", snapshot.seuil);
    } else {
        snprintf(text, sizeof(text), "Seuil actuel : 30.00 °C");
    }
    gtk_label_set_text(GTK_LABEL(data->seuil_label), text);

    gtk_label_set_text(GTK_LABEL(data->status_label), snapshot.status);
}

static gboolean gui_refresh_timer(gpointer user_data) {
    AppData *data = user_data;
    update_labels(data);
    return G_SOURCE_CONTINUE;
}

static void on_led_on(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;
    app_light_on(data->ctx);
}

static void on_led_off(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;
    app_light_off(data->ctx);
}

static void on_blink_on(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;
    app_blink_on(data->ctx);
}

static void on_blink_off(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;
    app_blink_off(data->ctx);
}

static void on_apply_frequency(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;

    int frequency = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->freq_spin));
    app_set_blink_frequency(data->ctx, frequency);
}

static void on_apply_seuil(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;

    double seuil = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->seuil_spin));
    app_set_temperature_threshold(data->ctx, seuil);
}

static void on_refresh_temperature(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *data = user_data;

    app_request_temperature(data->ctx);
}

static GtkWidget *create_luminaire_section(AppData *data) {
    GtkWidget *frame = gtk_frame_new("Gestion luminaire");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(frame), box);

    GtkWidget *btn_on = gtk_button_new_with_label("Allumer LED");
    GtkWidget *btn_off = gtk_button_new_with_label("Éteindre LED");
    GtkWidget *btn_blink_on = gtk_button_new_with_label("Clignoter");
    GtkWidget *btn_blink_off = gtk_button_new_with_label("Arrêter clignotement");

    g_signal_connect(btn_on, "clicked", G_CALLBACK(on_led_on), data);
    g_signal_connect(btn_off, "clicked", G_CALLBACK(on_led_off), data);
    g_signal_connect(btn_blink_on, "clicked", G_CALLBACK(on_blink_on), data);
    g_signal_connect(btn_blink_off, "clicked", G_CALLBACK(on_blink_off), data);

    gtk_box_append(GTK_BOX(box), btn_on);
    gtk_box_append(GTK_BOX(box), btn_off);
    gtk_box_append(GTK_BOX(box), btn_blink_on);
    gtk_box_append(GTK_BOX(box), btn_blink_off);

    return frame;
}

static GtkWidget *create_temperature_section(AppData *data) {
    GtkWidget *frame = gtk_frame_new("Gestion température");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(frame), box);

    data->temperature_label = gtk_label_new("Température : —");
    data->humidite_label = gtk_label_new("Humidité : —");
    data->seuil_label = gtk_label_new("Seuil actuel : 30.00 °C");

    GtkWidget *refresh_button = gtk_button_new_with_label("Rafraîchir température");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_temperature), data);

    gtk_box_append(GTK_BOX(box), data->temperature_label);
    gtk_box_append(GTK_BOX(box), data->humidite_label);
    gtk_box_append(GTK_BOX(box), data->seuil_label);
    gtk_box_append(GTK_BOX(box), refresh_button);

    return frame;
}

static GtkWidget *create_parameters_section(AppData *data) {
    GtkWidget *frame = gtk_frame_new("Paramètres");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(frame), box);

    GtkWidget *freq_label = gtk_label_new("Fréquence de clignotement, en ms");
    data->freq_spin = gtk_spin_button_new_with_range(100, 5000, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->freq_spin), 500);

    GtkWidget *freq_button = gtk_button_new_with_label("Appliquer fréquence");
    g_signal_connect(freq_button, "clicked", G_CALLBACK(on_apply_frequency), data);

    GtkWidget *seuil_label = gtk_label_new("Seuil température, en °C");
    data->seuil_spin = gtk_spin_button_new_with_range(-20.0, 80.0, 0.5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->seuil_spin), 30.0);

    GtkWidget *seuil_button = gtk_button_new_with_label("Appliquer seuil");
    g_signal_connect(seuil_button, "clicked", G_CALLBACK(on_apply_seuil), data);

    gtk_box_append(GTK_BOX(box), freq_label);
    gtk_box_append(GTK_BOX(box), data->freq_spin);
    gtk_box_append(GTK_BOX(box), freq_button);

    gtk_box_append(GTK_BOX(box), seuil_label);
    gtk_box_append(GTK_BOX(box), data->seuil_spin);
    gtk_box_append(GTK_BOX(box), seuil_button);

    return frame;
}

static GtkWidget *create_status_section(AppData *data) {
    GtkWidget *frame = gtk_frame_new("Status");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(frame), box);

    data->connection_label = gtk_label_new("Connexion : —");
    data->status_label = gtk_label_new("Status : —");

    gtk_label_set_wrap(GTK_LABEL(data->connection_label), TRUE);
    gtk_label_set_wrap(GTK_LABEL(data->status_label), TRUE);

    gtk_box_append(GTK_BOX(box), data->connection_label);
    gtk_box_append(GTK_BOX(box), data->status_label);

    return frame;
}

static void on_window_destroy(GtkWidget *window, gpointer user_data) {
    (void)window;

    AppData *data = user_data;

    if (data->gui_timer_id != 0) {
        g_source_remove(data->gui_timer_id);
    }

    app_context_stop(data->ctx);
    app_context_free(data->ctx);

    g_free(data);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    LaunchConfig *config = user_data;

    AppData *data = g_new0(AppData, 1);

    data->ctx = app_context_new(config->port, config->baud);
    app_context_start(data->ctx);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "ProjetBadis - Arduino GUI pthread");
    gtk_window_set_default_size(GTK_WINDOW(window), 440, 560);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root, 16);
    gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 16);
    gtk_widget_set_margin_end(root, 16);

    gtk_window_set_child(GTK_WINDOW(window), root);

    gtk_box_append(GTK_BOX(root), create_luminaire_section(data));
    gtk_box_append(GTK_BOX(root), create_temperature_section(data));
    gtk_box_append(GTK_BOX(root), create_parameters_section(data));
    gtk_box_append(GTK_BOX(root), create_status_section(data));

    update_labels(data);

    data->gui_timer_id = g_timeout_add(200, gui_refresh_timer, data);

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), data);

    gtk_window_present(GTK_WINDOW(window));
}

void ihm_run(int argc, char **argv, const char *port, int baud) {
    (void)argc;

    GtkApplication *app = gtk_application_new("fr.projetbadis.gui", G_APPLICATION_FLAGS_NONE);

    LaunchConfig config = {
        .port = port,
        .baud = baud
    };

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &config);

    char *clean_argv[] = { argv[0], NULL };
    int clean_argc = 1;

    g_application_run(G_APPLICATION(app), clean_argc, clean_argv);

    g_object_unref(app);
}
EOF
