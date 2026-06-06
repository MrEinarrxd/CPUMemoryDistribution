// === src/presentation/uiStrings.h ===

#ifndef UI_STRINGS_H
#define UI_STRINGS_H

#define uiTitleSimulator "OS Simulator"
#define uiLabelProcesses "Processes"
#define uiLabelMemory "Memory"
#define uiLabelCpu "CPU"
#define uiLabelScheduler "Scheduler"
#define uiLabelAlgorithm "Algorithm"
#define uiLabelQuantum "Quantum"
#define uiLabelUtilization "Utilization"
#define uiLabelWaitingTime "Waiting Time"
#define uiLabelTurnaroundTime "Turnaround Time"
#define uiLabelPageFaults "Page Faults"
#define uiLabelFragmentation "Fragmentation"
#define uiLabelContextSwitches "Context Switches"
#define uiLabelIoOperations "I/O Operations"
#define uiButtonStart "Start"
#define uiButtonPause "Pause"
#define uiButtonResume "Resume"
#define uiButtonStop "Stop"
#define uiButtonReset "Reset"
#define uiStatusRunning "Running"
#define uiStatusPaused "Paused"
#define uiStatusStopped "Stopped"

const char* uiStringsGetLabel(const char* key);

#endif
