#include "timeHelper.h"
#include <sys/time.h>
#include <unistd.h>

long timeHelperGetCurrentTime(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

long timeHelperGetElapsedTime(long startTime) {
    return timeHelperGetCurrentTime() - startTime;
}

void timeHelperDelay(int milliseconds) {
    if (milliseconds <= 0) return;
    usleep(milliseconds * 1000);
}

void timeHelperReset(void) {}
