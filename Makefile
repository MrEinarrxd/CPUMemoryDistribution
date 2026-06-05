CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src
LDLIBS := -lpvm3

SIM_SOURCES := \
	src/data/wordLoader.c \
	src/data/logger.c \
	src/data/processLog.c \
	src/data/bcpLog.c \
	src/data/phraseLoader.c \
	src/domain/distributed/distributedTasks.c \
	src/domain/distributed/fakePvmMaster.c \
	src/domain/distributed/messageProtocol.c \
	src/domain/distributed/realPvmMaster.c \
	src/domain/core/ioQueue.c \
	src/domain/core/ioDispatcher.c \
	src/domain/core/agingAnalysis.c \
	src/domain/core/preemption.c \
	src/domain/core/readyQueue.c \
	src/domain/core/performanceBar.c \
	src/domain/core/scheduler.c \
	src/domain/core/processGenerator.c \
	src/domain/core/statsCollector.c \
	src/domain/core/process.c \
	src/domain/core/rrScheduler.c \
	src/domain/core/ioCompletionHandler.c \
	src/domain/core/algorithmSwitcher.c \
	src/domain/core/rebalancing.c \
	src/domain/core/bcp.c \
	src/domain/core/processTable.c \
	src/domain/core/fcfsScheduler.c \
	src/domain/memory/bitmapManager.c \
	src/domain/memory/memoria.c \
	src/domain/memory/swapManager.c \
	src/domain/memory/fifoReplacement.c \
	src/domain/memory/memoryResize.c \
	src/domain/memory/pagingManager.c \
	src/utils/timeHelper.c \
	src/utils/uniqueId.c \
	src/utils/random.c \
	src/utils/errorHandler.c \
	src/presentation/uiTexts.c \
	src/presentation/consoleIo.c \
	src/main.c \
	src/business/guiController.c \
	src/business/appController.c \
	src/business/memoryController.c \
	src/business/simulationController.c \
	src/business/pvmController.c

SLAVE_SOURCES := \
	src/domain/distributed/realPvmSlave.c \
	src/domain/distributed/messageProtocol.c \
	src/domain/distributed/distributedTasks.c \
	src/utils/errorHandler.c

.PHONY: all clean

all: CPUMemoryDistributionSimulator simSlave

CPUMemoryDistributionSimulator: $(SIM_SOURCES)
	$(CC) $(CFLAGS) $(SIM_SOURCES) -o $@ $(LDLIBS)

simSlave: $(SLAVE_SOURCES)
	$(CC) $(CFLAGS) $(SLAVE_SOURCES) -o $@ $(LDLIBS)

clean:
	rm -f CPUMemoryDistributionSimulator simSlave
