#include "localRunner.h"
#include "distributedTasks.h"

#include <string.h>

void localRunnerRun(const ProcessTable* table, DistributedReport* report) {
    ProcessStatsRow statsRows[TotalProcesses];
    RrAnalysisRow rrRows[TotalProcesses];
    DistributedStatsResult statsPartials[PvmWorkerCount];
    DistributedAgingResult agingPartials[PvmWorkerCount];
    int statsCount;
    int rrCount;

    if (!table || !report) return;
    memset(report, 0, sizeof(*report));

    statsCount = processTableExportStatsRows(table, statsRows, TotalProcesses);
    rrCount = processTableExportRrRows(table, rrRows, TotalProcesses);

    for (int worker = 0; worker < PvmWorkerCount; ++worker) {
        int statsStart = (statsCount * worker) / PvmWorkerCount;
        int statsEnd = (statsCount * (worker + 1)) / PvmWorkerCount;
        int rrStart = (rrCount * worker) / PvmWorkerCount;
        int rrEnd = (rrCount * (worker + 1)) / PvmWorkerCount;
        distributedTasksCalculateStats(statsRows + statsStart, statsEnd - statsStart,
                                       &statsPartials[worker]);
        distributedTasksCalculateAging(rrRows + rrStart, rrEnd - rrStart,
                                       &agingPartials[worker]);
    }

    distributedTasksIntegrateStats(statsPartials, PvmWorkerCount, &report->stats);
    distributedTasksIntegrateAging(agingPartials, PvmWorkerCount, &report->aging);
}
