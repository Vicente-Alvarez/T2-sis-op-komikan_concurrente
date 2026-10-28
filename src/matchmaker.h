#ifndef MATCHMAKER_H
#define MATCHMAKER_H

#include <stdbool.h>
#include "cliente.h"
#include "config.h"
#include "mesa.h"

typedef struct matchmaker matchmaker_t;

typedef struct {
    int partidas;
    int gana_puma, gana_perros, abandonos, empates, interrumpidas, errores;
    int max_mesas;
} mm_stats_t;

matchmaker_t *matchmaker_crear(int max_mesas, const config_t *cfg);
int   matchmaker_iniciar(matchmaker_t *mm);
void  matchmaker_unir(matchmaker_t *mm);
void  matchmaker_destruir(matchmaker_t *mm);

int   matchmaker_encolar(matchmaker_t *mm, cliente_t *c);

void  matchmaker_fin_llegadas(matchmaker_t *mm, int creados);

bool  matchmaker_dormir(matchmaker_t *mm, int ms);

void  matchmaker_cerrar(matchmaker_t *mm);

bool  matchmaker_esperar_fin(matchmaker_t *mm, int ms);

void  matchmaker_mesa_finalizada(matchmaker_t *mm, mesa_t *m, mesa_res_t res);

void  matchmaker_stats(matchmaker_t *mm, mm_stats_t *out);

#endif
