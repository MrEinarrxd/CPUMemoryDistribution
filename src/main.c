#include "business/appController.h"
/*  
gcc -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src \
  $(find src -name '*.c' ! -name 'realPvmSlave.c') \
  -o CPUMemoryDistributionSimulator -lpvm3

gcc -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src \
  src/domain/distributed/realPvmSlave.c \
  -o simSlave -lpvm3

./CPUMemoryDistributionSimulator

export PVM_SLAVE_HOSTS=host1,host2
export PVM_SLAVE_EXEC=simSlave
./CPUMemoryDistributionSimulator
*/
int main(void) {
    AppController* controller = appControllerCreate();
    if (!controller) return 1;
    int result = appControllerRun(controller);
    appControllerDestroy(controller);
    return result == 0 ? 0 : 1;
}
