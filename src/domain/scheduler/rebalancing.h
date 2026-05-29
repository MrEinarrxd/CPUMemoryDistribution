#ifndef REBALANCING_H
#define REBALANCING_H

#include "../../utils/constants.h"

typedef struct Rebalancer {
    int iterationCount;
    int checkInterval;
    float lastProportionReady;
    float lastProportionWaiting;
    int quantumAdjustments;
    int isBalanced;
    int alertPending;
} Rebalancer;

Rebalancer* rebalancerCreate(void);
void rebalancerDestroy(Rebalancer* rebalancer);
void rebalancerTick(Rebalancer* rebalancer);
int rebalancerShouldCheck(Rebalancer* rebalancer);
int rebalancerIsImbalanced(Rebalancer* rebalancer, float proportionReady);
int rebalancerSuggestQuantumDelta(Rebalancer* rebalancer, float proportionReady);
void rebalancerOnBalanceRestored(Rebalancer* rebalancer);
int rebalancerHasAlert(Rebalancer* rebalancer);
void rebalancerClearAlert(Rebalancer* rebalancer);
int rebalancerGetAdjustmentCount(Rebalancer* rebalancer);

#endif