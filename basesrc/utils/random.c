// Implementación de funciones de aleatoriedad para el simulador
// Cumple C99. Usa solo las constantes definidas en constants.h

#include "random.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Inicializa la semilla del generador. Si seed==0 usa time(NULL).
void initRandom(unsigned int seed) {
    unsigned int s = seed;
    if (s == 0) {
        s = (unsigned int)time(NULL);
    }
    srand(s);
}

// Retorna un entero aleatorio en [min, max] inclusive. Si min>max intercambia.
int randomInt(int min, int max) {
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }
    int range = max - min + 1;
    if (range <= 0) { // protección, aunque no debería ocurrir
        return min;
    }
    return min + (rand() % range);
}

// Retorna un float aleatorio en [min, max). Si min>max intercambia.
float randomFloat(float min, float max) {
    if (min > max) {
        float tmp = min;
        min = max;
        max = tmp;
    }
    float r = (float)rand() / ((float)RAND_MAX + 1.0f); // en [0,1)
    return min + r * (max - min);
}

// Genera tiempos de llegada únicos entre tiempoLlegadaMin y tiempoLlegadaMax.
// Usa una lista temporal con todos los valores posibles, los mezcla y toma
// los primeros processCount. Si el rango es insuficiente o falla malloc,
// cae en un respaldo que genera valores aleatorios sin garantía de unicidad.
void generateUniqueArrivalTimes(int* arrivalTimes, int processCount) {
    if (arrivalTimes == NULL || processCount <= 0) return;

    int min = tiempoLlegadaMin;
    int max = tiempoLlegadaMax;
    int rangeSize = max - min + 1;

    if (rangeSize >= processCount) {
        // Intentar crear el array de todos los posibles tiempos
        int *pool = (int*)malloc(sizeof(int) * (size_t)rangeSize);
        if (pool == NULL) {
            // Respaldo: generar aleatorios (no garantizados únicos)
            for (int i = 0; i < processCount; ++i) {
                arrivalTimes[i] = randomInt(min, max);
            }
            return;
        }

        // Llenar pool con todos los valores posibles
        for (int i = 0; i < rangeSize; ++i) {
            pool[i] = min + i;
        }

        // Fisher-Yates shuffle
        for (int i = rangeSize - 1; i > 0; --i) {
            int j = randomInt(0, i);
            int tmp = pool[i];
            pool[i] = pool[j];
            pool[j] = tmp;
        }

        // Copiar los primeros processCount valores mezclados
        for (int i = 0; i < processCount; ++i) {
            arrivalTimes[i] = pool[i];
        }

        free(pool);
        return;
    }

    // Si no hay suficientes valores únicos en el rango, generar aleatorios
    for (int i = 0; i < processCount; ++i) {
        arrivalTimes[i] = randomInt(min, max);
    }
}

// Retorna ciclos CPU aleatorios usando las constantes.
int randomCpuCycles(void) {
    return randomInt(ciclosCpuMin, ciclosCpuMax);
}

// Retorna ciclos por instancia aleatorios usando las constantes.
int randomCpuInstanceCycles(void) {
    return randomInt(ciclosPorInstanciaMin, ciclosPorInstanciaMax);
}

// Retorna tiempo de cambio de contexto aleatorio usando las constantes.
int randomContextSwitchTime(void) {
    return randomInt(cambioContextoMin, cambioContextoMax);
}

// Lista circular de crecimiento de memoria.
// Contiene growthListSize elementos: 15 ceros y 5 valores aleatorios 1-50.
// Los 5 valores aleatorios se generan solo una vez y la lista se recorre
// en orden circular en llamadas sucesivas.
int randomMemoryGrowth(void) {
    static int list[growthListSize];
    static int initialized = 0;
    static int idx = 0;

    if (!initialized) {
        initialized = 1;
        // Primero llenar con 15 ceros y 5 valores aleatorios (1-50)
        int zeros = growthListSize - 5;
        int pos = 0;
        for (int i = 0; i < zeros; ++i) {
            list[pos++] = 0;
        }
        for (int i = 0; i < 5; ++i) {
            list[pos++] = randomInt(1, 50);
        }

        // Mezclar la lista para distribuir los valores no-cero
        for (int i = growthListSize - 1; i > 0; --i) {
            int j = randomInt(0, i);
            int tmp = list[i];
            list[i] = list[j];
            list[j] = tmp;
        }
        idx = 0;
    }

    int value = list[idx];
    idx = (idx + 1) % growthListSize;
    return value;
}
