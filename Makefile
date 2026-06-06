CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -DpvmModeEnabled=1 -I./src
LDLIBS := -lpvm3

ALL_SOURCES := $(shell find src -name '*.c' | sort)
SIM_SOURCES := $(filter-out src/distributed/pvm_slave.c,$(ALL_SOURCES))
SLAVE_SOURCES := \
	src/distributed/pvm_slave.c \
	src/distributed/protocol.c \
	src/distributed/distributed_tasks.c

.PHONY: all clean

all: CPUMemoryDistributionSimulator simSlave

CPUMemoryDistributionSimulator: $(SIM_SOURCES)
	$(CC) $(CFLAGS) $(SIM_SOURCES) -o $@ $(LDLIBS)

simSlave: $(SLAVE_SOURCES)
	$(CC) $(CFLAGS) $(SLAVE_SOURCES) -o $@ $(LDLIBS)

clean:
	rm -f CPUMemoryDistributionSimulator simSlave
