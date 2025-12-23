#pragma once
#include "openthread/instance.h"

typedef struct { 
    char endereco[40];
    char dataHora[64];
    float temperatura;
    float umidadeAr;
    float umidadeSolo;
    float particulas;
} sensor_data_t;


