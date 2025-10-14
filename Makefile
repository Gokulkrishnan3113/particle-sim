# Makefile for Verlet Integration Parallel Computing Demo

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O3
LDFLAGS = -lglfw -lGLEW -lGL -lm -lpthread
OPENMP_FLAGS = -fopenmp

# Directories
SRC_DIR = .
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SOURCES = main.c parallel_verlet.c graphics.c shader.c model.c verlet.c camera.c peripheral.c
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)

# Target executable
TARGET = $(BIN_DIR)/verlet_parallel

# Build modes
.PHONY: all clean sequential openmp pthread gpu debug

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OPENMP_FLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"
	@echo "Run with: ./$(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(OPENMP_FLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Sequential build (no OpenMP)
sequential: CFLAGS += -DNO_OPENMP
sequential: OPENMP_FLAGS =
sequential: clean $(TARGET)

# OpenMP optimized build
openmp: OPENMP_FLAGS = -fopenmp
openmp: CFLAGS += -DUSE_OPENMP
openmp: clean $(TARGET)

# Pthread build
pthread: CFLAGS += -DUSE_PTHREAD
pthread: clean $(TARGET)

# GPU compute build
gpu: CFLAGS += -DUSE_GPU
gpu: clean $(TARGET)

# Debug build
debug: CFLAGS = -Wall -Wextra -std=c11 -g -O0 -DDEBUG
debug: clean $(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned build artifacts"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Display build information
info:
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS) $(OPENMP_FLAGS)"
	@echo "Linker: $(LDFLAGS)"
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"
	@echo "Target: $(TARGET)"