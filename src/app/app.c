#include "app.h"

#include "../distributed/backend.h"
#include "../simulation/engine.h"
#include "../ui/console_ui.h"
#include <stdio.h>

static int run_engine(DistributedMode mode, const char* name, int pvm_test) {
    SimulationEngine engine;
    if (simulation_engine_init(&engine, mode, name) != 0) {
        ui_shutdown();
        fprintf(stderr, "No se pudo inicializar la simulacion. Revise PVM/variables si eligio modo real.\n");
        return 1;
    }
    if (pvm_test && engine.config.max_cycles == 0) engine.config.max_cycles = 100;
    int result = simulation_engine_run(&engine);
    simulation_engine_destroy(&engine);
    return result == 0 ? 0 : 1;
}

int app_run(void) {
    ui_init();
    int option = ui_show_menu();
    int result = 1;
    switch (option) {
        case 1:
            result = run_engine(DISTRIBUTED_REAL_PVM, "Simulacion completa con PVM real", 0);
            break;
        case 2:
            result = run_engine(DISTRIBUTED_FAKE, "Simulacion con PVM simulado", 0);
            break;
        case 3:
            result = run_engine(DISTRIBUTED_REAL_PVM, "Prueba PVM real", 1);
            break;
        default:
            ui_shutdown();
            fprintf(stderr, "Opcion invalida.\n");
            return 1;
    }
    ui_shutdown();
    return result;
}
