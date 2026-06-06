// === src/presentation/menu.h ===

#ifndef MENU_H
#define MENU_H

#include "../utils/constants.h"

struct PerformanceBar;
struct StatsCollector;

// Sistema de menú de texto en consola
// Reemplaza GUI, maneja entrada de usuario para selección de algoritmo, quantum, teclas X y A

// Muestra menú principal
void menuShowMain(void);

// Muestra opciones de selección de algoritmo
void menuShowAlgorithmOptions(void);

// Obtiene elección de algoritmo del usuario, retorna ID del algoritmo
int menuGetAlgorithmChoice(void);

// Obtiene valor de quantum ingresado por el usuario, retorna quantum
int menuGetQuantumInput(void);

// Muestra prompt para ingreso de quantum
void menuShowQuantumPrompt(void);

// Muestra alerta de balance con proporciones actuales y nuevo quantum
void menuShowBalanceAlert(float proportionReady, float proportionWaiting, int newQuantum);

// Muestra los 5 procesos más envejecidos
void menuShowTop5Aged(const char ids[][idProcesoLen], const int wasteValues[], int count);

// Muestra las 5 peores fuentes de desperdicio de memoria
void menuShowTop5Wasters(const char ids[][idProcesoLen], const int wasteValues[], int count);

// Obtiene ID de proceso privilegiado de la entrada del usuario
int menuGetPrivilegedProcessId(char* outId, int maxLen);

// Muestra resultados de procesamiento distribuido PVM
void menuShowPvmResults(void);

void menuShowPerformanceBars(const struct PerformanceBar* bar);
// Muestra las 5 barras de aprovechamiento y desperdicio de CPU

void menuShowMemoryStats(const struct StatsCollector* collector);
// Muestra estadísticas de memoria: desperdicio interno, externo, fragmentación

#endif
