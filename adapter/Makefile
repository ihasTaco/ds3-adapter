# RosettaPad - Makefile
# Builds the modular DualSense to PS3 adapter

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/common.c \
       $(SRC_DIR)/ds3.c \
       $(SRC_DIR)/dualsense.c \
       $(SRC_DIR)/usb_gadget.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Target
TARGET = $(BIN_DIR)/rosettapad

# Default target
all: dirs $(TARGET)

# Create directories
dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install to system
install: $(TARGET)
	@echo "Installing to /opt/rosettapad..."
	sudo mkdir -p /opt/rosettapad
	sudo cp $(TARGET) /opt/rosettapad/
	sudo ln -sf /opt/rosettapad/rosettapad /usr/local/bin/rosettapad
	@echo "Installed!"

# Development: build and run
run: $(TARGET)
	sudo $(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Show module structure
info:
	@echo "RosettaPad Module Structure:"
	@echo "  include/"
	@echo "    common.h     - Shared state and types"
	@echo "    ds3.h        - DS3 emulation interface"
	@echo "    dualsense.h  - DualSense controller interface"
	@echo "    usb_gadget.h - USB FunctionFS interface"
	@echo "  src/"
	@echo "    main.c       - Entry point, thread orchestration"
	@echo "    common.c     - Global state definitions"
	@echo "    ds3.c        - DS3 protocol implementation"
	@echo "    dualsense.c  - DualSense BT communication"
	@echo "    usb_gadget.c - USB gadget setup and threads"

.PHONY: all dirs clean install run debug info