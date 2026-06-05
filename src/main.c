#include "business/appController.h"

int main(void) {
    AppController* controller = appControllerCreate();
    if (!controller) return 1;
    int result = appControllerRun(controller);
    appControllerDestroy(controller);
    return result == 0 ? 0 : 1;
}
