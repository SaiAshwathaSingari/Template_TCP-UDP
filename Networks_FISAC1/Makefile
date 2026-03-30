# Makefile for Collaborative Code Editor
# Builds the WebSocket server with embedded SQLite

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread -DSQLITE_THREADSAFE=1
LDFLAGS = -lpthread -ldl -lm

SRC_DIR = server
BUILD_DIR = build

SRCS = $(SRC_DIR)/server.c \
       $(SRC_DIR)/websocket.c \
       $(SRC_DIR)/database.c \
       $(SRC_DIR)/sqlite3.c

TARGET = $(BUILD_DIR)/collab_server

.PHONY: all clean run

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRCS) $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -o $(TARGET) $(SRCS) $(LDFLAGS)
	@echo ""
	@echo "Build successful!"
	@echo "Run with: ./$(TARGET)"
	@echo "Then open http://localhost:9090 in your browser"

clean:
	rm -rf $(BUILD_DIR) $(SRC_DIR)/collab_editor.db

run: $(TARGET)
	cd $(SRC_DIR) && ../$(TARGET)
