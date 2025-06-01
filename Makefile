# Compiler and flags
CC ?= clang-18 
CFLAGS = -std=gnu23 -Wall -Wextra -O2 `pkg-config --cflags SDL2_ttf sdl2`
LD = `pkg-config --libs SDL2_ttf sdl2`
# Source and build directories
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
TARGET := $(BIN_DIR)/terminal.out
EXTRA := 
# Output binary
  
ifneq ($(EMSCRIPTEN),)
	TARGET := $(BIN_DIR)/index.html
        EXTRA  := --embed-file assets --shell-file template.html
	CFLAGS = -std=gnu23 -Wall -Wextra -O2 -s USE_SDL=2 -s USE_SDL_TTF=2
endif

# Find all source files and corresponding object files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Default build rule
all: $(TARGET)

# Rule to compile the target
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)/assets/
	@cp -r assets/* $(BIN_DIR)/assets/
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LD) $(EXTRA)

# Rule to compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Use emmake by just calling emmake make
.PHONY: all clean

