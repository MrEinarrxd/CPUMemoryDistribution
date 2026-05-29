#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

void consoleIoInit(void);
void consoleIoCleanup(void);
void consoleIoSetRawMode(void);
void consoleIoSetNormalMode(void);
int consoleIoReadLine(char* buffer, int maxLen);
int consoleIoKbhit(void);
char consoleIoGetChar(void);
void consoleIoPrint(const char* text);
void consoleIoPrintLine(const char* text);
void consoleIoPrintInt(const char* label, int value);
void consoleIoPrintFloat(const char* label, float value);
void consoleIoClear(void);
void consoleIoPrintSeparator(void);

#endif
