#include "clock.h"

#include <time.h>

void clock_sleep_ms(int milliseconds) {
    if (milliseconds <= 0) return;
    struct timespec req;
    req.tv_sec = milliseconds / 1000;
    req.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    nanosleep(&req, NULL);
}
