#include "business/systemController.h"

//gcc -std=c11 -Wall -Wextra \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I./src \
  $(find src -name '*.c' ! -path 'src/domain/distributed/*') \
  -o sim_nopvm

//./sim_nopvm



int main() {
    SystemController* controller = systemControllerCreate();
    if (!controller) return 1;
    if (systemControllerInit(controller) != 0) {
        systemControllerDestroy(controller);
        return 1;
    }
    systemControllerRun(controller);
    systemControllerDestroy(controller);
    return 0;
}
