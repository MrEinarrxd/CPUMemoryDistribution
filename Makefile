CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src
LDLIBS := -lpvm3

SIM_SOURCES := $(shell find src -name '*.c' ! -name 'realPvmSlave.c')
SLAVE_SOURCES := src/domain/distributed/realPvmSlave.c src/domain/distributed/distributedTasks.c

.PHONY: all clean

all: CPUMemoryDistributionSimulator simSlave

CPUMemoryDistributionSimulator: $(SIM_SOURCES)
	$(CC) $(CFLAGS) $(SIM_SOURCES) -o $@ $(LDLIBS)

simSlave: $(SLAVE_SOURCES)
	$(CC) $(CFLAGS) $(SLAVE_SOURCES) -o $@ $(LDLIBS)

clean:
	rm -f CPUMemoryDistributionSimulator simSlave
