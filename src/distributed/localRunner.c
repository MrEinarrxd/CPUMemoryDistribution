#include "localRunner.h"
#include "distributedTasks.h"

#include <string.h>

void localRunnerRun(const ProcessTable* table, DistributedReport* report) {
    ProcessStatsRow statsRows[TotalProcesses];
    RrAnalysisRow rrRows[TotalProcesses];
    DistributedStatsResult statsPartials[2];
    DistributedAgingResult agingPartials[2];
    int statsCount;
    int rrCount;
    int splitStats;
    int splitRr;

    if (!table || !report) return;
    memset(report, 0, sizeof(*report));

    statsCount = processTableExportStatsRows(table, statsRows, TotalProcesses);
    rrCount = processTableExportRrRows(table, rrRows, TotalProcesses);
    splitStats = statsCount / 2;
    splitRr = rrCount / 2;

    distributedTasksCalculateStats(statsRows, splitStats, &statsPartials[0]);
    distributedTasksCalculateStats(statsRows + splitStats, statsCount - splitStats, &statsPartials[1]);
    distributedTasksCalculateAging(rrRows, splitRr, &agingPartials[0]);
    distributedTasksCalculateAging(rrRows + splitRr, rrCount - splitRr, &agingPartials[1]);

    distributedTasksIntegrateStats(statsPartials, 2, &report->stats);
    distributedTasksIntegrateAging(agingPartials, 2, &report->aging);
}
