#include "business/appController.h"

<<<<<<< Updated upstream
gcc -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src \
  $(find src -name '*.c' ! -name 'pvmSlave.c') \
  -o sim_pvm -lpvm3

export PVM_SLAVE_HOSTS=slave1,slave2
./sim_pvm

*/

#include "business/systemController.h"

int main() {
    SystemController* controller = systemControllerCreate();
=======
int main(void) {
    AppController* controller = appControllerCreate();
>>>>>>> Stashed changes
    if (!controller) return 1;
    int result = appControllerRun(controller);
    appControllerDestroy(controller);
    return result == 0 ? 0 : 1;
}
