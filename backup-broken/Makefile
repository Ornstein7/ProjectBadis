CC = gcc
TARGET = build/projetbadis_gui

SRC = src/main.c src/ihm.c src/calcul.c

CFLAGS = -Wall -Wextra -std=c11 -Iinclude -pthread $(shell pkg-config --cflags gtk4)
LDFLAGS = -pthread $(shell pkg-config --libs gtk4)

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf build/*
