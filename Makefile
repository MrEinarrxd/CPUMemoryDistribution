CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -DpvmModeEnabled=1 -I./src
LDLIBS := -lpvm3
BUILD_DIR := build

ALL_SOURCES := $(shell find src -name '*.c' | sort)
SIM_SOURCES := $(filter-out src/distributed/pvmSlave.c,$(ALL_SOURCES))
SLAVE_SOURCES := \
	src/distributed/pvmSlave.c \
	src/distributed/protocol.c \
	src/distributed/distributedTasks.c

.PHONY: all clean

all: $(BUILD_DIR)/CPUMemoryDistributionSimulator $(BUILD_DIR)/simSlave

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/CPUMemoryDistributionSimulator: $(SIM_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SIM_SOURCES) -o $@ $(LDLIBS)

$(BUILD_DIR)/simSlave: $(SLAVE_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SLAVE_SOURCES) -o $@ $(LDLIBS)

clean:
	rm -f $(BUILD_DIR)/CPUMemoryDistributionSimulator $(BUILD_DIR)/simSlave
	rm -f CPUMemoryDistributionSimulator simSlave
