#ifndef CpuMemoryConstantsH
#define CpuMemoryConstantsH

enum {
    TotalProcesses = 250,
    ActiveProcessCount = 150,
    NewRequestCount = 100,
    ReadyQueueCapacity = TotalProcesses,
    IoDeviceCount = 4,
    HistoryBars = 5,
    TopRankingCount = 5,
    ProcessIdLen = 16,
    WordLen = 32,
    PhraseLen = 256,
    MaxRepositoryWords = 5000,
    MaxRepositoryPhrases = 1000,
    ArrivalMin = 0,
    ArrivalMax = 800,
    CpuCyclesMin = 0,
    CpuCyclesMax = 85000,
    CpuInstanceMin = 10,
    CpuInstanceMax = 70,
    ContextSwitchMin = 10,
    ContextSwitchMax = 30,
    CreationSleepMin = 1,
    CreationSleepMax = 50,
    MemoryGrowthListSize = 20,
    MemoryGrowthNonZero = 5,
    MemoryGrowthMin = 1,
    MemoryGrowthMax = 50,
    WordsPerPage = 20,
    MinPageFrames = 8,
    MaxPageFrames = 20,
    MaxPagesPerProcess = 32,
    PhysicalFrameCount = 160,
    SwapPageCapacity = TotalProcesses * MaxPagesPerProcess,
    DefaultQuantum = 20,
    MinQuantum = 10,
    MaxQuantum = 120,
    RrRebalanceInterval = 20,
    PvmAnalysisInterval = 5000,
    QueueImbalanceThreshold = 75,
    AutoSwitchInterval = 40,
    AutoSwitchCooldownIterations = 5000,
    MemoryResizeInterval = 200,
    MaxCpuIterations = 400000
};

static const char LogDirectory[] = "logs";
static const char ProcessTableLog[] = "logs/process_table.log";
static const char BcpLog[] = "logs/bcp.log";
static const char WordBookPath[] = "logs/libro1.odt";
static const char PhraseBookPath[] = "logs/frases.odt";
static const char DefaultPvmSlaveExec[] = "/tmp/simSlave";

#endif
