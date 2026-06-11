#include "ihm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *port = "/dev/ttyACM0";
    int baud = 9600;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--serial") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
            baud = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--serial /dev/ttyACM0] [--baud 9600]\n", argv[0]);
            return 0;
        }
    }

    ihm_run(argc, argv, port, baud);
    return 0;
}
