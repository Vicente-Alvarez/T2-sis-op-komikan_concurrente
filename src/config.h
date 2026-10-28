#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdio.h>

#define CFG_MAX_FILAS 8
#define CFG_MAX_COLS  16

typedef struct config {

    int  delay_min_ms;
    int  delay_max_ms;
    bool usa_disposicion;
    int  disp_filas;
    char disposicion[CFG_MAX_FILAS][CFG_MAX_COLS + 1];

    unsigned int seed;
    bool seed_auto;

    int    llegada_min_ms;
    int    llegada_max_ms;
    double prob_fallo;
    int    fallo_min_ms;
    int    fallo_max_ms;
    int    max_partida_ms;
    int    log_tablero_ms;
} config_t;

void config_por_defecto(config_t *cfg);

int  config_cargar(config_t *cfg, const char *ruta);
int  config_validar(const config_t *cfg);
void config_imprimir(const config_t *cfg, FILE *out);

unsigned int config_derivar_semilla(unsigned int base, unsigned int tipo, unsigned int id);

int  config_azar(unsigned int *estado, int min, int max);

#endif
