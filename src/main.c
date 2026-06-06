<<<<<<< Updated upstream
#include "src/business/systemController.h"

int main() {
    SystemController* controller = systemControllerCreate();
    systemControllerInit(controller);
    systemControllerRun(controller);
    systemControllerDestroy(controller);
    return 0;
}
=======
#include "app/app.h"

int main(void) {
    return app_run();
}
>>>>>>> Stashed changes
