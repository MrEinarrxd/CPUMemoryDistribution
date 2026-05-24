// === src/presentation/menu.h ===

#ifndef MENU_H
#define MENU_H

#include "../utils/constants.h"

// Console text menu system
// Replaces GUI, handles user input for algorithm selection, quantum input, X and A keys

// Show main menu
void menuShowMain(void);

// Show algorithm selection options
void menuShowAlgorithmOptions(void);

// Get algorithm choice from user, returns algorithm ID
int menuGetAlgorithmChoice(void);

// Get quantum value input from user, returns quantum value
int menuGetQuantumInput(void);

// Show prompt for quantum input
void menuShowQuantumPrompt(void);

// Show balance alert with current proportions and new quantum
void menuShowBalanceAlert(float proportionReady, float proportionWaiting, int newQuantum);

// Show top 5 aged processes
void menuShowTop5Aged(const char ids[][idProcesoLen], const int wasteValues[], int count);

// Show top 5 memory wasters
void menuShowTop5Wasters(const char ids[][idProcesoLen], const int wasteValues[], int count);

// Get privileged process ID from user input
int menuGetPrivilegedProcessId(char* outId, int maxLen);

// Show PVM distributed processing results
void menuShowPvmResults(void);

#endif
