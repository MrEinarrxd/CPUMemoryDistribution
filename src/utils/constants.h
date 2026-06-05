#ifndef CONSTANTS_H
#define CONSTANTS_H

#define TotalProcesos 250
#define ProcesosEnEjecucion 150
#define ProcesosEnEspera 100
#define TamColaListos 250
#define BarrasHistorial 5
#define IdProcesoLen 16
#define TotalRankingProcesos 5
#define MaxMarcos 512
#define PalabrasPorPagina 20
#define MaxCaracteresPalabra 32
#define MaxSwap (MaxMarcos * 2)
#define MaxPaginasPorProceso 32
#define NumColasEs 4
#define MultColaEs1 2
#define MultColaEs2 4
#define MultColaEs3 8
#define MultColaEs4 12
#define CiclosCpuMin 0
#define CiclosCpuMax 85000
#define CiclosPorInstanciaMin 10
#define CiclosPorInstanciaMax 70
#define CambioContextoMin 10
#define CambioContextoMax 30
#define TiempoLlegadaMin 0
#define TiempoLlegadaMax 800
#define SleepCreacionMin 1
#define SleepCreacionMax 50
#define QuantumDefault 20
#define IteracionesRebalanceo 20
#define UmbralDesbalance 75
#define PalabrasPorFrase 5
#define TamanoFraseIo 500
#ifndef pvmModeEnabled
#define pvmModeEnabled 1
#endif
#define PvmNumeroEsclavos 2
#define PvmSlaveHostsEnvVar "PVM_SLAVE_HOSTS"
#define LongitudMaximaCadena 256
#define TamanoBufferMensaje 1024
#define TamanoBufferLog 2048
#define HistorialAlgoritmoMaximo 1000
#define MarcosMin 8
#define MarcosMax 20
#define TiempoEsMin 1
#define TiempoEsMax 100
#define GrowthListSize 20
#define RetardoSimulacionMs 100

/* Alias de compatibilidad para conservar los algoritmos ya probados. */
#define totalProcesos TotalProcesos
#define procesosEnEjecucion ProcesosEnEjecucion
#define procesosEnEspera ProcesosEnEspera
#define tamColaListos TamColaListos
#define barrasHistorial BarrasHistorial
#define idProcesoLen IdProcesoLen
#define totalRankingProcesos TotalRankingProcesos
#define maxMarcos MaxMarcos
#define palabrasPorPagina PalabrasPorPagina
#define maxCaracteresPalabra MaxCaracteresPalabra
#define maxSwap MaxSwap
#define maxPaginasPorProceso MaxPaginasPorProceso
#define numColasEs NumColasEs
#define multColaEs1 MultColaEs1
#define multColaEs2 MultColaEs2
#define multColaEs3 MultColaEs3
#define multColaEs4 MultColaEs4
#define ciclosCpuMin CiclosCpuMin
#define ciclosCpuMax CiclosCpuMax
#define ciclosPorInstanciaMin CiclosPorInstanciaMin
#define ciclosPorInstanciaMax CiclosPorInstanciaMax
#define cambioContextoMin CambioContextoMin
#define cambioContextoMax CambioContextoMax
#define tiempoLlegadaMin TiempoLlegadaMin
#define tiempoLlegadaMax TiempoLlegadaMax
#define sleepCreacionMin SleepCreacionMin
#define sleepCreacionMax SleepCreacionMax
#define quantumDefault QuantumDefault
#define iteracionesRebalanceo IteracionesRebalanceo
#define umbralDesbalance UmbralDesbalance
#define palabrasPorFrase PalabrasPorFrase
#define tamanoFraseIo TamanoFraseIo
#define pvmNumEsclavos PvmNumeroEsclavos
#define pvmSlaveHostsEnvVar PvmSlaveHostsEnvVar
#define longitudMaximaCadena LongitudMaximaCadena
#define tamanoBufferMensaje TamanoBufferMensaje
#define tamanoBufferLog TamanoBufferLog
#define historialAlgoritmoMaximo HistorialAlgoritmoMaximo
#define marcosMin MarcosMin
#define marcosMax MarcosMax
#define tiempoEsMin TiempoEsMin
#define tiempoEsMax TiempoEsMax
#define growthListSize GrowthListSize
#define retardoSimulacionMs RetardoSimulacionMs

#endif
