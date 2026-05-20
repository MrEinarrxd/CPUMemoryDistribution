// === src/domain/stats/agingAnalysis.h ===

#ifndef AGING_ANALYSIS_H
#define AGING_ANALYSIS_H

#include "../../utils/constants.h"

struct ProcessTable;

typedef struct AgingAnalysis {
    int samples[maxAlgorithmHistory];
    int sampleCount;
    float averageWaste;
    float maxWaste;
    float minWaste;
} AgingAnalysis;

AgingAnalysis* agingAnalysisCreate(void);
void agingAnalysisDestroy(AgingAnalysis* analysis);
void agingAnalysisCollectSample(AgingAnalysis* analysis, struct ProcessTable* table);
void agingAnalysisCalculateStatistics(AgingAnalysis* analysis);
float agingAnalysisGetAverageWaste(AgingAnalysis* analysis);
float agingAnalysisGetMaxWaste(AgingAnalysis* analysis);
int agingAnalysisGetSampleCount(AgingAnalysis* analysis);

#endif