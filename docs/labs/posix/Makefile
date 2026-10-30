CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O0 -g

BUILD_DIR := build
PROGRAM_NAMES = 01_rpc_socket 02_capability_table 03_notification_shared_memory
PROGRAMS = $(addprefix $(BUILD_DIR)/,$(PROGRAM_NAMES))

all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/01_rpc_socket: 01_rpc_socket.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/02_capability_table: 02_capability_table.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/03_notification_shared_memory: 03_notification_shared_memory.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ -pthread

run: all
	@./$(BUILD_DIR)/01_rpc_socket
	@./$(BUILD_DIR)/02_capability_table
	@./$(BUILD_DIR)/03_notification_shared_memory

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
