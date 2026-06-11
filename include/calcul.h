#ifndef CALCUL_H
#define CALCUL_H

#include <stddef.h>

typedef struct {
    int fd;
    int connected;
    int baud;

    char port[128];
    char last_error[256];

    double temperature;
    double humidite;
    double seuil;

    int has_temperature;
    int has_humidite;
    int has_seuil;
} ArduinoState;

void arduino_init_state(ArduinoState *state);

int arduino_open(ArduinoState *state, const char *port, int baud);
void arduino_close(ArduinoState *state);

int arduino_send(ArduinoState *state, const char *command);
int arduino_read_available(ArduinoState *state, char *buffer, size_t size);

void arduino_process_line(ArduinoState *state, const char *line);

#endif
