#include "consoleIo.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios originalTermios;
static int terminalInitialized = 0;

static void restoreTerminal(void) {
    if (terminalInitialized) {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
    }
}

int consoleIoInit(void) {
    if (!isatty(STDIN_FILENO)) return -1;
    if (!terminalInitialized) {
        if (tcgetattr(STDIN_FILENO, &originalTermios) != 0) return -1;
        terminalInitialized = 1;
        atexit(restoreTerminal);
    }
    consoleIoSetRawMode();
    return 0;
}

void consoleIoCleanup(void) {
    restoreTerminal();
}

void consoleIoSetRawMode(void) {
    struct termios raw;
    if (!terminalInitialized) return;
    raw = originalTermios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void consoleIoSetNormalMode(void) {
    restoreTerminal();
}

int consoleIoKbhit(void) {
    struct timeval timeout = {0, 0};
    fd_set fds;
    if (!terminalInitialized) return 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout) > 0;
}

char consoleIoGetChar(void) {
    char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) <= 0) return 0;
    return ch;
}

int consoleIoReadLine(char* buffer, int maxLen) {
    if (!buffer || maxLen <= 0) return 0;
    consoleIoSetNormalMode();
    if (!fgets(buffer, maxLen, stdin)) {
        buffer[0] = '\0';
        consoleIoSetRawMode();
        return 0;
    }
    consoleIoSetRawMode();
    return 1;
}

void consoleIoPrint(const char* text) {
    if (text) printf("%s", text);
    fflush(stdout);
}

void consoleIoPrintLine(const char* text) {
    if (text) printf("%s\n", text);
    else printf("\n");
    fflush(stdout);
}

void consoleIoPrintInt(const char* label, int value) {
    printf("%s %d\n", label ? label : "", value);
    fflush(stdout);
}

void consoleIoPrintFloat(const char* label, float value) {
    printf("%s %.2f\n", label ? label : "", value);
    fflush(stdout);
}

void consoleIoClear(void) {
    printf("\033[2J\033[1;1H");
    fflush(stdout);
}

void consoleIoPrintSeparator(void) {
    printf("========================================\n");
    fflush(stdout);
}
