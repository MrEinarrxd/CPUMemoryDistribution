// === src/utils/constants.h ===

#ifndef CONSTANTS_H
#define CONSTANTS_H

#define totalProcesos 250
#define procesosEnEjecucion 150
#define procesosEnEspera 100
#define tamColaListos 250
#define barrasHistorial 5
#define idProcesoLen 16
#define totalRankingProcesos 5
#define maxMarcos 512
#define palabrasPorPagina 20
#define maxCaracteresPalabra 32
#define maxSwap (maxMarcos * 2)
#define maxPaginasPorProceso 32
#define numColasEs 4
#define multColaEs1 2
#define multColaEs2 4
#define multColaEs3 8
#define multColaEs4 12
#define ciclosCpuMin 0
#define ciclosCpuMax 85000
#define ciclosPorInstanciaMin 10
#define ciclosPorInstanciaMax 70
#define cambioContextoMin 10
#define cambioContextoMax 30
#define tiempoLlegadaMin 0
#define tiempoLlegadaMax 800
#define sleepCreacionMin 1
#define sleepCreacionMax 50
#define quantumDefault 20
#define iteracionesRebalanceo 20
#define umbralDesbalance 75
#define palabrasPorFrase 5
#define tamanoFraseIo 500
#define pvmNumEsclavos 2
#define longitudMaximaCadena 256
#define tamanoBufferMensaje 1024
#define tamanoBufferLog 2048
#define historialAlgoritmoMaximo 1000
#define marcosMin 8
#define marcosMax 20
#define tiempoEsMin 1
#define tiempoEsMax 100
#define growthListSize 20

#endif
