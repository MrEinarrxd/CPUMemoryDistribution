#include "business/systemController.h"

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