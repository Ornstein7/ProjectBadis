CC ?= cc
PKG_CONFIG ?= pkg-config

APP_NAME ?= projetbadis_gui
BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/$(APP_NAME)

SRC := src/main.c src/ihm.c src/calcul.c
PORT ?= /dev/ttyACM0
BAUD ?= 9600

CPPFLAGS += -Iinclude
CFLAGS += -Wall -Wextra -std=c11
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk4 2>/dev/null)

.PHONY: all check-deps run clean

all: check-deps $(TARGET)

check-deps:
	@$(PKG_CONFIG) --exists gtk4 || { \
		echo "Erreur: GTK4 introuvable. Installez libgtk-4-dev/gtk4-devel et pkg-config."; \
		exit 1; \
	}

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): $(SRC) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(GTK_LIBS)

run: $(TARGET)
	./$(TARGET) --serial $(PORT) --baud $(BAUD)

clean:
	rm -rf $(BUILD_DIR)
