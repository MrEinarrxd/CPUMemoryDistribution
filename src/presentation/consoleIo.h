#ifndef CpuMemoryConsoleIoH
#define CpuMemoryConsoleIoH

int consoleIoInit(void);
void consoleIoCleanup(void);
void consoleIoSetRawMode(void);
void consoleIoSetNormalMode(void);
int consoleIoKbhit(void);
char consoleIoGetChar(void);
int consoleIoReadLine(char* buffer, int maxLen);
void consoleIoPrint(const char* text);
void consoleIoPrintLine(const char* text);
void consoleIoPrintInt(const char* label, int value);
void consoleIoPrintFloat(const char* label, float value);
void consoleIoClear(void);
void consoleIoPrintSeparator(void);

#endif
