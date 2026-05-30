/*

gcc -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src \
  $(find src -name '*.c' ! -name 'pvmSlave.c') \
  -o simPvm -lpvm3

export PVM_SLAVE_HOSTS=192.168.100.98,192.168.100.97
export PVM_SLAVE_EXEC=/home/master/Documents/GitHub/CPUMemoryDistribution/pvmSlave
./simPvm

*/

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
