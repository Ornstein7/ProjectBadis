#include "ihm.h"
#include "calcul.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *port;
    int baud;
} LaunchConfig;

typedef struct {
    ArduinoState arduino;

    GtkWidget *temperature_label;
    GtkWidget *humidite_label;
    GtkWidget *seuil_label;
    GtkWidget *status_label;

    GtkWidget *freq_spin;
    GtkWidget *seuil_spin;

    char rx_line[512];
    size_t rx_len;

    guint poll_timer_id;
    guint request_timer_id;
} AppData;

static void update_labels(AppData *data) {
    char text[256];

    if (data->arduino.has_temperature) {
        snprintf(text, sizeof(text), "Température : %.2f °C", data->arduino.temperature);
    } else {
        snprintf(text, sizeof(text), "Température : —");
    }
    gtk_label_set_text(GTK_LABEL(data->temperature_label), text);

    if (data->arduino.has_humidite) {
        snprintf(text, sizeof(text), "Humidité : %.2f %%", data->arduino.humidite);
    } else {
        snprintf(text, sizeof(text), "Humidité : —");
    }
    gtk_label_set_text(GTK_LABEL(data->humidite_label), text);

    if (data->arduino.has_seuil) {
        snprintf(text, sizeof(text), "Seuil actuel : %.2f °C", data->arduino.seuil);
    } else {
        snprintf(text, sizeof(text), "Seuil actuel : 30.00 °C");
    }
    gtk_label_set_text(GTK_LABEL(data->seuil_label), text);

    gtk_label_set_text(GTK_LABEL(data->status_label), data->arduino.last_error);
}

static void send_command(AppData *data, const char *command) {
    if (arduino_send(&data->arduino, command) == 0) {
        char status[256];
        snprintf(status, sizeof(status), "Commande envoyée : %s", command);
        gtk_label_set_text(GTK_LABEL(data->status_label), status);
    } else {
        update_labels(data);
    }
}

static void on_command_button_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;

    AppData *data = g_object_get_data(G_OBJECT(button), "app-data");
    const char *command = g_object_get_data(G_OBJECT(button), "command");

    send_command(data, command);
}

static GtkWidget *make_command_button(const char *label, AppData *data, const char *command) {
    GtkWidget *button = gtk_button_new_with_label(label);

    g_object_set_data(G_OBJECT(button), "app-data", data);
    g_object_set_data(G_OBJECT(button), "command", (gpointer)command);

    g_signal_connect(button, "clicked", G_CALLBACK(on_command_button_clicked), NULL);

    return button;
}

static void on_apply_frequency(GtkButton *button, gpointer user_data) {
    (void)button;

    AppData *data = user_data;

    int frequency = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->freq_spin));

    char command[64];
    snprintf(command, sizeof(command), "FREQ:%d", frequency);

    send_command(data, command);
}

static void on_apply_seuil(GtkButton *button, gpointer user_data) {
    (void)button;

    AppData *data = user_data;

    double seuil = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->seuil_spin));

    char command[64];
    snprintf(command, sizeof(command), "SEUIL:%.1f", seuil);

    send_command(data, command);
}

static void on_refresh_temperature(GtkButton *button, gpointer user_data) {
    (void)button;

    AppData *data = user_data;
    send_command(data, "GET_TEMP");
}

static gboolean poll_serial(gpointer user_data) {
    AppData *data = user_data;

    char buffer[256];
    int n = arduino_read_available(&data->arduino, buffer, sizeof(buffer));

    if (n < 0) {
        update_labels(data);
        return G_SOURCE_CONTINUE;
    }

    for (int i = 0; i < n; i++) {
        char c = buffer[i];

        if (c == '\n' || c == '\r') {
            if (data->rx_len > 0) {
                data->rx_line[data->rx_len] = '\0';

                arduino_process_line(&data->arduino, data->rx_line);
                update_labels(data);

                data->rx_len = 0;
            }
        } else {
            if (data->rx_len < sizeof(data->rx_line) - 1) {
                data->rx_line[data->rx_len++] = c;
            }
        }
    }

    return G_SOURCE_CONTINUE;
}

static gboolean request_temperature_timer(gpointer user_data) {
    AppData *data = user_data;

    if (data->arduino.connected) {
        arduino_send(&data->arduino, "GET_TEMP");
    }

    return G_SOURCE_CONTINUE;
}

static void on_window_destroy(GtkWidget *window, gpointer user_data) {
    (void)window;

    AppData *data = user_data;

    if (data->poll_timer_id != 0) {
        g_source_remove(data->poll_timer_id);
    }

    if (data->request_timer_id != 0) {
        g_source_remove(data->request_timer_id);
    }

    arduino_close(&data->arduino);
    g_free(data);
}

static GtkWidget *create_luminaire_section(AppData *data) {
    GtkWidget *frame = gtk_frame_new("Gestion luminaire");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(frame), box);

    gtk_box_append(GTK_BOX(box), make_command_button("Allumer LED", data, "LED_ON"));
    gtk_box_append(GTK_BOX(box), make_command_button("Éteindre LED", data, "LED_OFF"));
    gtk_box_append(GTK_BOX(box), make_command_button("Clignoter", data, "BLINK_ON"));
    gtk_box_append(GTK_BOX(box), make_command_button("Arrêter clignotement", data, "BLINK_OFF"));

    return frame;
}

static GtkWidget *create_temperature_section(AppData *data) {
    GtkWidget *frame = gtk_frame_new("Gestion température");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(frame), box);

    data->temperature_label = gtk_label_new("Température : —");
    data->humidite_label = gtk_label_new("Humidité : —");
    data->seuil_label = gtk_label_new("Seuil actuel : 30.00 °C");

    gtk_box_append(GTK_BOX(box), data->temperature_label);
    gtk_box_append(GTK_BOX(box), data->humidite_label);
    gtk_box_append(GTK_BOX(box), data->seuil_label);

    GtkWidget *refresh_button = gtk_button_new_with_label("Rafraîchir");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_temperature), data);
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

static void on_activate(GtkApplication *app, gpointer user_data) {
    LaunchConfig *config = user_data;

    AppData *data = g_new0(AppData, 1);

    arduino_open(&data->arduino, config->port, config->baud);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "ProjetBadis - Arduino GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 420, 520);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root, 16);
    gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 16);
    gtk_widget_set_margin_end(root, 16);

    gtk_window_set_child(GTK_WINDOW(window), root);

    gtk_box_append(GTK_BOX(root), create_luminaire_section(data));
    gtk_box_append(GTK_BOX(root), create_temperature_section(data));
    gtk_box_append(GTK_BOX(root), create_parameters_section(data));

    GtkWidget *status_frame = gtk_frame_new("Status");
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_frame_set_child(GTK_FRAME(status_frame), status_box);

    data->status_label = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(data->status_label), TRUE);

    gtk_box_append(GTK_BOX(status_box), data->status_label);
    gtk_box_append(GTK_BOX(root), status_frame);

    update_labels(data);

    data->poll_timer_id = g_timeout_add(100, poll_serial, data);
    data->request_timer_id = g_timeout_add_seconds(3, request_temperature_timer, data);

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), data);

    gtk_window_present(GTK_WINDOW(window));
}

void ihm_run(int argc, char **argv, const char *port, int baud) {
    GtkApplication *app = gtk_application_new("fr.projetbadis.gui", G_APPLICATION_DEFAULT_FLAGS);

    LaunchConfig config = {
        .port = port,
        .baud = baud
    };

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &config);

    char *clean_argv[] = { argv[0], NULL };
    int clean_argc = 1;

    g_application_run(G_APPLICATION(app), clean_argc, clean_argv);

    g_object_unref(app);

    (void)argc;
}
