#include "rebalancing.h"
#include <stdlib.h>

Rebalancer* rebalancerCreate(void) {
    Rebalancer* r = (Rebalancer*)calloc(1, sizeof(Rebalancer));
    if (r) {
        r->checkInterval = iteracionesRebalanceo;
        r->iterationCount = 0;
        r->quantumAdjustments = 0;
        r->isBalanced = 1;
        r->alertPending = 0;
    }
    return r;
}
void rebalancerDestroy(Rebalancer* rebalancer) { free(rebalancer); }
void rebalancerTick(Rebalancer* rebalancer) { if (rebalancer) rebalancer->iterationCount++; }
int rebalancerShouldCheck(Rebalancer* rebalancer) {
    if (!rebalancer) return 0;
    return (rebalancer->iterationCount >= rebalancer->checkInterval);
}
int rebalancerIsImbalanced(Rebalancer* rebalancer, float proportionReady) {
    if (!rebalancer) return 0;
    float imbalance = proportionReady * 100.0f;
    return (imbalance >= umbralDesbalance || (100.0f - imbalance) >= umbralDesbalance);
}
int rebalancerSuggestQuantumDelta(Rebalancer* rebalancer, float proportionReady) {
    (void)rebalancer;
    if (proportionReady > 0.75f) return -5;
    else if (proportionReady < 0.25f) return 5;
    else return 0;
}
void rebalancerOnBalanceRestored(Rebalancer* rebalancer) {
    if (rebalancer) {
        rebalancer->iterationCount = 0;
        rebalancer->alertPending = 1;
        rebalancer->quantumAdjustments++;
    }
}
int rebalancerHasAlert(Rebalancer* rebalancer) { return rebalancer ? rebalancer->alertPending : 0; }
void rebalancerClearAlert(Rebalancer* rebalancer) { if (rebalancer) rebalancer->alertPending = 0; }
int rebalancerGetAdjustmentCount(Rebalancer* rebalancer) { return rebalancer ? rebalancer->quantumAdjustments : 0; }
