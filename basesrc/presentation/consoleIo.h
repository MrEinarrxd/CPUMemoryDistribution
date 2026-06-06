// === src/presentation/consoleIo.h ===

#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

// Low-level console input/output
// Replaces GUI controller, handles non-blocking keypress detection for X and A

// Initialize console I/O for non-blocking input
void consoleIoInit(void);

// Cleanup console I/O resources
void consoleIoCleanup(void);

// Check if key was pressed (non-blocking), returns 1 if key available, 0 otherwise
int consoleIoKbhit(void);

// Get single character from console, non-blocking
char consoleIoGetChar(void);

// Print text to console without newline
void consoleIoPrint(const char* text);

// Print text to console with newline
void consoleIoPrintLine(const char* text);

// Print labeled integer value
void consoleIoPrintInt(const char* label, int value);

// Print labeled float value
void consoleIoPrintFloat(const char* label, float value);

// Clear console screen
void consoleIoClear(void);

// Print separator line
void consoleIoPrintSeparator(void);

#endif
