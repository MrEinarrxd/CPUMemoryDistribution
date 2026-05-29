#include "agingAnalysis.h"
#include "../process/processTable.h"
#include <stdlib.h>

AgingAnalysis* agingAnalysisCreate(void) {
    AgingAnalysis* aa = (AgingAnalysis*)calloc(1, sizeof(AgingAnalysis));
    return aa;
}
void agingAnalysisDestroy(AgingAnalysis* analysis) { free(analysis); }
void agingAnalysisCollectSample(AgingAnalysis* analysis, ProcessTable* table) {
    if (!analysis || !table) return;
    int totalWaste = 0, count = 0;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            totalWaste += p->bcp->wastedCpuCycles;
            count++;
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            totalWaste += p->bcp->wastedCpuCycles;
            count++;
        }
    }
    int sample = (count > 0) ? totalWaste / count : 0;
    if (analysis->sampleCount < historialAlgoritmoMaximo) {
        analysis->samples[analysis->sampleCount++] = sample;
    } else {
        for (int i = 1; i < historialAlgoritmoMaximo; i++)
            analysis->samples[i-1] = analysis->samples[i];
        analysis->samples[historialAlgoritmoMaximo-1] = sample;
    }
    agingAnalysisCalculateStatistics(analysis);
}
void agingAnalysisCalculateStatistics(AgingAnalysis* analysis) {
    if (!analysis || analysis->sampleCount == 0) return;
    float sum = 0;
    analysis->maxWaste = analysis->samples[0];
    analysis->minWaste = analysis->samples[0];
    for (int i = 0; i < analysis->sampleCount; i++) {
        sum += analysis->samples[i];
        if (analysis->samples[i] > analysis->maxWaste) analysis->maxWaste = analysis->samples[i];
        if (analysis->samples[i] < analysis->minWaste) analysis->minWaste = analysis->samples[i];
    }
    analysis->averageWaste = sum / analysis->sampleCount;
}
float agingAnalysisGetAverageWaste(AgingAnalysis* analysis) { return analysis ? analysis->averageWaste : 0.0f; }
float agingAnalysisGetMaxWaste(AgingAnalysis* analysis) { return analysis ? analysis->maxWaste : 0.0f; }
int agingAnalysisGetSampleCount(AgingAnalysis* analysis) { return analysis ? analysis->sampleCount : 0; }

//Parte 9 – Dominio: Distribuido (PVM)
