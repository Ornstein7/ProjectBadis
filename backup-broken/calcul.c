cat > src/calcul.c <<'EOF'
#define _DEFAULT_SOURCE

#include "calcul.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define QUEUE_SIZE 64

typedef enum {
    CMD_LED_ON,
    CMD_LED_OFF,
    CMD_BLINK_ON,
    CMD_BLINK_OFF,
    CMD_SET_FREQ,
    CMD_GET_TEMP,
    CMD_SET_SEUIL
} CommandType;

typedef struct {
    CommandType type;
    double value;
} WorkerCommand;

typedef struct {
    WorkerCommand items[QUEUE_SIZE];
    int head;
    int tail;
    int count;
} CommandQueue;

struct AppContext {
    int fd;
    int connected;
    int baud;
    int running;

    char port[128];
    char status[256];

    double temperature;
    double humidite;
    double seuil;

    int has_temperature;
    int has_humidite;
    int has_seuil;

    pthread_t temperature_thread;
    pthread_t light_thread;

    int temperature_thread_started;
    int light_thread_started;

    pthread_mutex_t state_mutex;
    pthread_mutex_t serial_mutex;

    pthread_mutex_t light_mutex;
    pthread_cond_t light_cond;
    CommandQueue light_queue;

    pthread_mutex_t measure_mutex;
    pthread_cond_t measure_cond;
    CommandQueue measure_queue;

    char rx_line[512];
    size_t rx_len;
};

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

static void queue_init(CommandQueue *queue) {
    memset(queue, 0, sizeof(*queue));
}

static int queue_push(CommandQueue *queue, WorkerCommand cmd) {
    if (queue->count >= QUEUE_SIZE) {
        return -1;
    }

    queue->items[queue->tail] = cmd;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    queue->count++;

    return 0;
}

static int queue_pop(CommandQueue *queue, WorkerCommand *cmd) {
    if (queue->count <= 0) {
        return 0;
    }

    *cmd = queue->items[queue->head];
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    queue->count--;

    return 1;
}

static void set_status(AppContext *ctx, const char *message) {
    pthread_mutex_lock(&ctx->state_mutex);
    snprintf(ctx->status, sizeof(ctx->status), "%s", message);
    pthread_mutex_unlock(&ctx->state_mutex);
}

static void set_statusf(AppContext *ctx, const char *prefix, const char *detail) {
    pthread_mutex_lock(&ctx->state_mutex);
    snprintf(ctx->status, sizeof(ctx->status), "%s%s", prefix, detail);
    pthread_mutex_unlock(&ctx->state_mutex);
}

static int is_running(AppContext *ctx) {
    pthread_mutex_lock(&ctx->state_mutex);
    int running = ctx->running;
    pthread_mutex_unlock(&ctx->state_mutex);
    return running;
}

static int open_serial(AppContext *ctx) {
    int fd = open(ctx->port, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        pthread_mutex_lock(&ctx->state_mutex);
        ctx->connected = 0;
        snprintf(ctx->status, sizeof(ctx->status),
                 "Impossible d'ouvrir %s: %s", ctx->port, strerror(errno));
        pthread_mutex_unlock(&ctx->state_mutex);
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        pthread_mutex_lock(&ctx->state_mutex);
        ctx->connected = 0;
        snprintf(ctx->status, sizeof(ctx->status),
                 "tcgetattr erreur: %s", strerror(errno));
        pthread_mutex_unlock(&ctx->state_mutex);
        close(fd);
        return -1;
    }

    cfmakeraw(&tty);

    speed_t speed = baud_to_speed(ctx->baud);
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
        pthread_mutex_lock(&ctx->state_mutex);
        ctx->connected = 0;
        snprintf(ctx->status, sizeof(ctx->status),
                 "tcsetattr erreur: %s", strerror(errno));
        pthread_mutex_unlock(&ctx->state_mutex);
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    pthread_mutex_lock(&ctx->state_mutex);
    ctx->fd = fd;
    ctx->connected = 1;
    snprintf(ctx->status, sizeof(ctx->status),
             "Connecté à %s à %d baud", ctx->port, ctx->baud);
    pthread_mutex_unlock(&ctx->state_mutex);

    return 0;
}

static int serial_send_line(AppContext *ctx, const char *line) {
    pthread_mutex_lock(&ctx->serial_mutex);

    if (!ctx->connected || ctx->fd < 0) {
        pthread_mutex_unlock(&ctx->serial_mutex);
        set_status(ctx, "Arduino non connecté");
        return -1;
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s\n", line);

    size_t total = strlen(buffer);
    size_t sent = 0;

    while (sent < total) {
        ssize_t n = write(ctx->fd, buffer + sent, total - sent);

        if (n < 0) {
            pthread_mutex_unlock(&ctx->serial_mutex);

            pthread_mutex_lock(&ctx->state_mutex);
            snprintf(ctx->status, sizeof(ctx->status),
                     "Erreur écriture série: %s", strerror(errno));
            pthread_mutex_unlock(&ctx->state_mutex);

            return -1;
        }

        sent += (size_t)n;
    }

    pthread_mutex_unlock(&ctx->serial_mutex);
    return 0;
}

static int serial_read_available(AppContext *ctx, char *buffer, size_t size) {
    if (!ctx->connected || ctx->fd < 0 || size == 0) {
        return 0;
    }

    ssize_t n = read(ctx->fd, buffer, size - 1);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        pthread_mutex_lock(&ctx->state_mutex);
        snprintf(ctx->status, sizeof(ctx->status),
                 "Erreur lecture série: %s", strerror(errno));
        pthread_mutex_unlock(&ctx->state_mutex);

        return -1;
    }

    buffer[n] = '\0';
    return (int)n;
}

static void process_serial_line(AppContext *ctx, const char *line) {
    pthread_mutex_lock(&ctx->state_mutex);

    if (strncmp(line, "TEMP:", 5) == 0) {
        ctx->temperature = atof(line + 5);
        ctx->has_temperature = 1;
    } else if (strncmp(line, "HUM:", 4) == 0) {
        ctx->humidite = atof(line + 4);
        ctx->has_humidite = 1;
    } else if (strncmp(line, "SEUIL:", 6) == 0) {
        ctx->seuil = atof(line + 6);
        ctx->has_seuil = 1;
    } else if (strcmp(line, "ARDUINO_READY") == 0) {
        snprintf(ctx->status, sizeof(ctx->status), "Arduino prêt");
    } else if (strncmp(line, "OK:", 3) == 0) {
        snprintf(ctx->status, sizeof(ctx->status), "%s", line);
    } else if (strncmp(line, "FREQ_OK:", 8) == 0) {
        snprintf(ctx->status, sizeof(ctx->status), "%s", line);
    } else if (strncmp(line, "SEUIL_OK:", 9) == 0) {
        snprintf(ctx->status, sizeof(ctx->status), "%s", line);
    } else if (strncmp(line, "ERR:", 4) == 0) {
        snprintf(ctx->status, sizeof(ctx->status), "%s", line);
    }

    pthread_mutex_unlock(&ctx->state_mutex);
}

static void process_serial_buffer(AppContext *ctx, const char *buffer, int n) {
    for (int i = 0; i < n; i++) {
        char c = buffer[i];

        if (c == '\n' || c == '\r') {
            if (ctx->rx_len > 0) {
                ctx->rx_line[ctx->rx_len] = '\0';
                process_serial_line(ctx, ctx->rx_line);
                ctx->rx_len = 0;
            }
        } else {
            if (ctx->rx_len < sizeof(ctx->rx_line) - 1) {
                ctx->rx_line[ctx->rx_len++] = c;
            }
        }
    }
}

static int enqueue_light_command(AppContext *ctx, WorkerCommand cmd) {
    pthread_mutex_lock(&ctx->light_mutex);

    int result = queue_push(&ctx->light_queue, cmd);

    if (result == 0) {
        pthread_cond_signal(&ctx->light_cond);
    }

    pthread_mutex_unlock(&ctx->light_mutex);

    if (result != 0) {
        set_status(ctx, "File lumière pleine");
    }

    return result;
}

static int enqueue_measure_command(AppContext *ctx, WorkerCommand cmd) {
    pthread_mutex_lock(&ctx->measure_mutex);

    int result = queue_push(&ctx->measure_queue, cmd);

    if (result == 0) {
        pthread_cond_signal(&ctx->measure_cond);
    }

    pthread_mutex_unlock(&ctx->measure_mutex);

    if (result != 0) {
        set_status(ctx, "File mesure pleine");
    }

    return result;
}

static void *light_thread_main(void *arg) {
    AppContext *ctx = arg;

    while (is_running(ctx)) {
        WorkerCommand cmd;
        int has_cmd = 0;

        pthread_mutex_lock(&ctx->light_mutex);

        while (ctx->light_queue.count == 0 && is_running(ctx)) {
            pthread_cond_wait(&ctx->light_cond, &ctx->light_mutex);
        }

        has_cmd = queue_pop(&ctx->light_queue, &cmd);

        pthread_mutex_unlock(&ctx->light_mutex);

        if (!has_cmd) {
            continue;
        }

        switch (cmd.type) {
            case CMD_LED_ON:
                serial_send_line(ctx, "LED_ON");
                set_status(ctx, "Commande lumière: LED_ON");
                break;

            case CMD_LED_OFF:
                serial_send_line(ctx, "LED_OFF");
                set_status(ctx, "Commande lumière: LED_OFF");
                break;

            case CMD_BLINK_ON:
                serial_send_line(ctx, "BLINK_ON");
                set_status(ctx, "Commande lumière: BLINK_ON");
                break;

            case CMD_BLINK_OFF:
                serial_send_line(ctx, "BLINK_OFF");
                set_status(ctx, "Commande lumière: BLINK_OFF");
                break;

            case CMD_SET_FREQ: {
                char line[64];
                snprintf(line, sizeof(line), "FREQ:%d", (int)cmd.value);
                serial_send_line(ctx, line);
                set_status(ctx, "Commande lumière: fréquence envoyée");
                break;
            }

            default:
                break;
        }
    }

    return NULL;
}

static void *temperature_thread_main(void *arg) {
    AppContext *ctx = arg;

    int ticks = 0;

    while (is_running(ctx)) {
        WorkerCommand cmd;

        pthread_mutex_lock(&ctx->measure_mutex);
        while (queue_pop(&ctx->measure_queue, &cmd)) {
            pthread_mutex_unlock(&ctx->measure_mutex);

            if (cmd.type == CMD_GET_TEMP) {
                serial_send_line(ctx, "GET_TEMP");
                set_status(ctx, "Commande mesure: GET_TEMP");
            } else if (cmd.type == CMD_SET_SEUIL) {
                char line[64];
                snprintf(line, sizeof(line), "SEUIL:%.1f", cmd.value);
                serial_send_line(ctx, line);
                set_status(ctx, "Commande mesure: seuil envoyé");
            }

            pthread_mutex_lock(&ctx->measure_mutex);
        }
        pthread_mutex_unlock(&ctx->measure_mutex);

        char buffer[256];
        int n = serial_read_available(ctx, buffer, sizeof(buffer));

        if (n > 0) {
            process_serial_buffer(ctx, buffer, n);
        }

        ticks++;

        if (ticks >= 60) {
            serial_send_line(ctx, "GET_TEMP");
            ticks = 0;
        }

        usleep(50000);
    }

    return NULL;
}

AppContext *app_context_new(const char *port, int baud) {
    AppContext *ctx = calloc(1, sizeof(*ctx));

    if (!ctx) {
        return NULL;
    }

    ctx->fd = -1;
    ctx->baud = baud;
    ctx->seuil = 30.0;

    snprintf(ctx->port, sizeof(ctx->port), "%s", port);
    snprintf(ctx->status, sizeof(ctx->status), "Initialisation");

    pthread_mutex_init(&ctx->state_mutex, NULL);
    pthread_mutex_init(&ctx->serial_mutex, NULL);

    pthread_mutex_init(&ctx->light_mutex, NULL);
    pthread_cond_init(&ctx->light_cond, NULL);
    queue_init(&ctx->light_queue);

    pthread_mutex_init(&ctx->measure_mutex, NULL);
    pthread_cond_init(&ctx->measure_cond, NULL);
    queue_init(&ctx->measure_queue);

    return ctx;
}

int app_context_start(AppContext *ctx) {
    if (!ctx) {
        return -1;
    }

    pthread_mutex_lock(&ctx->state_mutex);
    ctx->running = 1;
    pthread_mutex_unlock(&ctx->state_mutex);

    if (open_serial(ctx) != 0) {
        return -1;
    }

    if (pthread_create(&ctx->temperature_thread, NULL, temperature_thread_main, ctx) == 0) {
        ctx->temperature_thread_started = 1;
    } else {
        set_status(ctx, "Erreur création thread température");
        return -1;
    }

    if (pthread_create(&ctx->light_thread, NULL, light_thread_main, ctx) == 0) {
        ctx->light_thread_started = 1;
    } else {
        set_status(ctx, "Erreur création thread lumière");
        return -1;
    }

    return 0;
}

void app_context_stop(AppContext *ctx) {
    if (!ctx) {
        return;
    }

    pthread_mutex_lock(&ctx->state_mutex);
    ctx->running = 0;
    pthread_mutex_unlock(&ctx->state_mutex);

    pthread_mutex_lock(&ctx->light_mutex);
    pthread_cond_broadcast(&ctx->light_cond);
    pthread_mutex_unlock(&ctx->light_mutex);

    pthread_mutex_lock(&ctx->measure_mutex);
    pthread_cond_broadcast(&ctx->measure_cond);
    pthread_mutex_unlock(&ctx->measure_mutex);

    if (ctx->temperature_thread_started) {
        pthread_join(ctx->temperature_thread, NULL);
        ctx->temperature_thread_started = 0;
    }

    if (ctx->light_thread_started) {
        pthread_join(ctx->light_thread, NULL);
        ctx->light_thread_started = 0;
    }

    pthread_mutex_lock(&ctx->serial_mutex);

    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    pthread_mutex_unlock(&ctx->serial_mutex);

    pthread_mutex_lock(&ctx->state_mutex);
    ctx->connected = 0;
    snprintf(ctx->status, sizeof(ctx->status), "Déconnecté");
    pthread_mutex_unlock(&ctx->state_mutex);
}

void app_context_free(AppContext *ctx) {
    if (!ctx) {
        return;
    }

    app_context_stop(ctx);

    pthread_mutex_destroy(&ctx->state_mutex);
    pthread_mutex_destroy(&ctx->serial_mutex);

    pthread_mutex_destroy(&ctx->light_mutex);
    pthread_cond_destroy(&ctx->light_cond);

    pthread_mutex_destroy(&ctx->measure_mutex);
    pthread_cond_destroy(&ctx->measure_cond);

    free(ctx);
}

void app_get_snapshot(AppContext *ctx, SensorSnapshot *snapshot) {
    if (!ctx || !snapshot) {
        return;
    }

    pthread_mutex_lock(&ctx->state_mutex);

    snapshot->connected = ctx->connected;
    snapshot->baud = ctx->baud;

    snprintf(snapshot->port, sizeof(snapshot->port), "%s", ctx->port);
    snprintf(snapshot->status, sizeof(snapshot->status), "%s", ctx->status);

    snapshot->temperature = ctx->temperature;
    snapshot->humidite = ctx->humidite;
    snapshot->seuil = ctx->seuil;

    snapshot->has_temperature = ctx->has_temperature;
    snapshot->has_humidite = ctx->has_humidite;
    snapshot->has_seuil = ctx->has_seuil;

    pthread_mutex_unlock(&ctx->state_mutex);
}

int app_light_on(AppContext *ctx) {
    return enqueue_light_command(ctx, (WorkerCommand){ .type = CMD_LED_ON });
}

int app_light_off(AppContext *ctx) {
    return enqueue_light_command(ctx, (WorkerCommand){ .type = CMD_LED_OFF });
}

int app_blink_on(AppContext *ctx) {
    return enqueue_light_command(ctx, (WorkerCommand){ .type = CMD_BLINK_ON });
}

int app_blink_off(AppContext *ctx) {
    return enqueue_light_command(ctx, (WorkerCommand){ .type = CMD_BLINK_OFF });
}

int app_set_blink_frequency(AppContext *ctx, int frequency_ms) {
    return enqueue_light_command(ctx, (WorkerCommand){
        .type = CMD_SET_FREQ,
        .value = frequency_ms
    });
}

int app_request_temperature(AppContext *ctx) {
    return enqueue_measure_command(ctx, (WorkerCommand){ .type = CMD_GET_TEMP });
}

int app_set_temperature_threshold(AppContext *ctx, double seuil) {
    return enqueue_measure_command(ctx, (WorkerCommand){
        .type = CMD_SET_SEUIL,
        .value = seuil
    });
}
EOF
