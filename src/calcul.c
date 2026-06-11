#define _DEFAULT_SOURCE

#include "calcul.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t baud_to_speed(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;
    }
}

void arduino_init_state(ArduinoState *state) {
    memset(state, 0, sizeof(*state));
    state->fd = -1;
    state->baud = 9600;
    state->seuil = 30.0;
}

int arduino_open(ArduinoState *state, const char *port, int baud) {
    arduino_init_state(state);

    strncpy(state->port, port, sizeof(state->port) - 1);
    state->baud = baud;

    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        snprintf(state->last_error, sizeof(state->last_error),
                 "Impossible d'ouvrir %s: %s", port, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        snprintf(state->last_error, sizeof(state->last_error),
                 "tcgetattr erreur: %s", strerror(errno));
        close(fd);
        return -1;
    }

    cfmakeraw(&tty);

    speed_t speed = baud_to_speed(baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
#endif

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        snprintf(state->last_error, sizeof(state->last_error),
                 "tcsetattr erreur: %s", strerror(errno));
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    state->fd = fd;
    state->connected = 1;

    snprintf(state->last_error, sizeof(state->last_error),
             "Connecté à %s à %d baud", port, baud);

    return 0;
}

void arduino_close(ArduinoState *state) {
    if (state->fd >= 0) {
        close(state->fd);
    }

    state->fd = -1;
    state->connected = 0;
}

int arduino_send(ArduinoState *state, const char *command) {
    if (!state->connected || state->fd < 0) {
        snprintf(state->last_error, sizeof(state->last_error),
                 "Arduino non connecté");
        return -1;
    }

    char line[128];
    snprintf(line, sizeof(line), "%s\n", command);

    ssize_t written = write(state->fd, line, strlen(line));

    if (written < 0) {
        snprintf(state->last_error, sizeof(state->last_error),
                 "Erreur envoi commande: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int arduino_read_available(ArduinoState *state, char *buffer, size_t size) {
    if (!state->connected || state->fd < 0 || size == 0) {
        return 0;
    }

    ssize_t n = read(state->fd, buffer, size - 1);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        snprintf(state->last_error, sizeof(state->last_error),
                 "Erreur lecture série: %s", strerror(errno));
        return -1;
    }

    buffer[n] = '\0';
    return (int)n;
}

void arduino_process_line(ArduinoState *state, const char *line) {
    if (strncmp(line, "TEMP:", 5) == 0) {
        state->temperature = atof(line + 5);
        state->has_temperature = 1;
    } else if (strncmp(line, "HUM:", 4) == 0) {
        state->humidite = atof(line + 4);
        state->has_humidite = 1;
    } else if (strncmp(line, "SEUIL:", 6) == 0) {
        state->seuil = atof(line + 6);
        state->has_seuil = 1;
    } else if (strncmp(line, "ERR:", 4) == 0) {
        snprintf(state->last_error, sizeof(state->last_error), "%s", line);
    } else if (strcmp(line, "ARDUINO_READY") == 0) {
        snprintf(state->last_error, sizeof(state->last_error), "Arduino prêt");
    } else if (strncmp(line, "FREQ_OK:", 8) == 0) {
        snprintf(state->last_error, sizeof(state->last_error), "%s", line);
    } else if (strncmp(line, "SEUIL_OK:", 9) == 0) {
        snprintf(state->last_error, sizeof(state->last_error), "%s", line);
    }
}
