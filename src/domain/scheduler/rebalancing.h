// === src/domain/scheduler/rebalancing.h ===

#ifndef REBALANCING_H
#define REBALANCING_H

#include "../../utils/constants.h"

// Dynamic quantum rebalancer
// Checks every 20 iterations if ready/waiting proportion is imbalanced
// If reaches 75-25 threshold, adjusts quantum and sends alert when balanced
typedef struct Rebalancer {
    int iterationCount;                                 // Counter from 0 to iteracionesRebalanceo
    int checkInterval;                                  // = iteracionesRebalanceo
    float lastProportionReady;                          // Last proportion of ready processes
    float lastProportionWaiting;                        // Last proportion of waiting processes
    int quantumAdjustments;                             // Total number of quantum adjustments
    int isBalanced;                                     // Flag: 1 if balanced, 0 if imbalanced
    int alertPending;                                   // Flag: 1 if alert should be shown
} Rebalancer;

// Create rebalancer instance
Rebalancer* rebalancerCreate(void);

// Destroy rebalancer
void rebalancerDestroy(Rebalancer* rebalancer);

// Increment iteration counter
void rebalancerTick(Rebalancer* rebalancer);

// Check if rebalancer should perform check this iteration
int rebalancerShouldCheck(Rebalancer* rebalancer);

// Check if current proportion is imbalanced (75-25 threshold)
int rebalancerIsImbalanced(Rebalancer* rebalancer, float proportionReady);

// Suggest quantum delta based on imbalance (-/+)
int rebalancerSuggestQuantumDelta(Rebalancer* rebalancer, float proportionReady);

// Called when balance is restored, sets alert pending
void rebalancerOnBalanceRestored(Rebalancer* rebalancer);

// Check if alert is pending
int rebalancerHasAlert(Rebalancer* rebalancer);

// Clear pending alert
void rebalancerClearAlert(Rebalancer* rebalancer);

// Get total count of quantum adjustments made
int rebalancerGetAdjustmentCount(Rebalancer* rebalancer);

#endif
