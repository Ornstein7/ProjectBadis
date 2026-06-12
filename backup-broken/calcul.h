cat > include/calcul.h <<'EOF'
#ifndef CALCUL_H
#define CALCUL_H

typedef struct AppContext AppContext;

typedef struct {
    int connected;
    int baud;

    char port[128];
    char status[256];

    double temperature;
    double humidite;
    double seuil;

    int has_temperature;
    int has_humidite;
    int has_seuil;
} SensorSnapshot;

AppContext *app_context_new(const char *port, int baud);
int app_context_start(AppContext *ctx);
void app_context_stop(AppContext *ctx);
void app_context_free(AppContext *ctx);

void app_get_snapshot(AppContext *ctx, SensorSnapshot *snapshot);

int app_light_on(AppContext *ctx);
int app_light_off(AppContext *ctx);
int app_blink_on(AppContext *ctx);
int app_blink_off(AppContext *ctx);
int app_set_blink_frequency(AppContext *ctx, int frequency_ms);

int app_request_temperature(AppContext *ctx);
int app_set_temperature_threshold(AppContext *ctx, double seuil);

#endif
EOF
