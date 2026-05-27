#include "src/business/systemController.h"

int main() {
    SystemController* controller = systemControllerCreate();
    systemControllerInit(controller);
    systemControllerRun(controller);
    systemControllerDestroy(controller);
    return 0;
}