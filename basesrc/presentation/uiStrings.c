// src/presentation/uiStrings.c
#include "uiStrings.h"
#include <string.h>

static const struct {
    const char* key;
    const char* value;
} stringTable[] = {
    {"titleSimulator", "OS Simulator"},
    {"labelProcesses", "Processes"},
    {"labelMemory", "Memory"},
    {"labelCpu", "CPU"},
    {"labelScheduler", "Scheduler"},
    {"labelAlgorithm", "Algorithm"},
    {"labelQuantum", "Quantum"},
    {"labelUtilization", "Utilization"},
    {"labelWaitingTime", "Waiting Time"},
    {"labelTurnaroundTime", "Turnaround Time"},
    {"labelPageFaults", "Page Faults"},
    {"labelFragmentation", "Fragmentation"},
    {"labelContextSwitches", "Context Switches"},
    {"labelIoOperations", "I/O Operations"},
    {"buttonStart", "Start"},
    {"buttonPause", "Pause"},
    {"buttonResume", "Resume"},
    {"buttonStop", "Stop"},
    {"buttonReset", "Reset"},
    {"statusRunning", "Running"},
    {"statusPaused", "Paused"},
    {"statusStopped", "Stopped"},
    {NULL, NULL}
};

const char* uiStringsGetLabel(const char* key) {
    if (!key) return "";
    for (int i = 0; stringTable[i].key != NULL; i++) {
        if (strcmp(key, stringTable[i].key) == 0)
            return stringTable[i].value;
    }
    return key;
}
