// === src/utils/timeHelper.h ===

#ifndef TIME_HELPER_H
#define TIME_HELPER_H

long timeHelperGetCurrentTime(void);

long timeHelperGetElapsedTime(long startTime);

void timeHelperDelay(int milliseconds);

void timeHelperReset(void);

#endif
