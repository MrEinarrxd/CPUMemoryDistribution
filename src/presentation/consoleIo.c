#include "consoleIo.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>

static struct termios orig_termios;
static int terminal_initialized = 0;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static void signal_handler(int sig) {
    (void)sig;
    restore_terminal();
    exit(1);
}

void consoleIoInit(void) {
    if (!terminal_initialized) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        terminal_initialized = 1;
    }
    consoleIoSetRawMode();
    atexit(restore_terminal);
    signal(SIGINT, signal_handler);
}

void consoleIoCleanup(void) {
    restore_terminal();
}

void consoleIoSetRawMode(void) {
    if (!terminal_initialized) return;
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

void consoleIoSetNormalMode(void) {
    restore_terminal();
}

int consoleIoReadLine(char* buffer, int maxLen) {
    if (!buffer || maxLen <= 0) return 0;
    consoleIoSetNormalMode();
    fflush(stdout);
    if (!fgets(buffer, maxLen, stdin)) {
        buffer[0] = '\0';
        consoleIoSetRawMode();
        return 0;
    }
    consoleIoSetRawMode();
    return 1;
}

int consoleIoKbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

char consoleIoGetChar(void) {
    char c = getchar();
    return c;
}

void consoleIoPrint(const char* text) { if (text) printf("%s", text); fflush(stdout); }
void consoleIoPrintLine(const char* text) { if (text) printf("%s\n", text); else printf("\n"); fflush(stdout); }
void consoleIoPrintInt(const char* label, int value) { printf("%s %d\n", label ? label : "", value); fflush(stdout); }
void consoleIoPrintFloat(const char* label, float value) { printf("%s %.2f\n", label ? label : "", value); fflush(stdout); }
void consoleIoClear(void) { printf("\033[2J\033[1;1H"); fflush(stdout); }
void consoleIoPrintSeparator(void) { printf("========================================\n"); fflush(stdout); }
