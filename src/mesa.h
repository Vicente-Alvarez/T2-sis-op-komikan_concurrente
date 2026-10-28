#ifndef MESA_H
#define MESA_H

#include <pthread.h>
#include <stdbool.h>
#include "cliente.h"
#include "config.h"

typedef struct mesa mesa_t;
typedef struct matchmaker matchmaker_t;

typedef enum {
    MR_GANA_PUMA,
    MR_GANA_PERROS,
    MR_ABANDONO_PUMA,
    MR_ABANDONO_PERROS,
    MR_EMPATE_TIEMPO,
    MR_INTERRUMPIDA,
    MR_ERROR
} mesa_res_t;

struct mesa {
    int  id;
    int  partida;
    cliente_t *puma;
    cliente_t *perros;
    const config_t *cfg;
    matchmaker_t   *mm;
    pthread_t hilo;
    bool hilo_creado;

    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    bool       fin_pedido;
    cliente_t *abandono;

    struct mesa *sig;
};

mesa_t *mesa_crear(int id, int partida, cliente_t *puma, cliente_t *perros,
                   const config_t *cfg, matchmaker_t *mm);
int     mesa_iniciar(mesa_t *m);
void    mesa_unir(mesa_t *m);
void    mesa_destruir(mesa_t *m);

void    mesa_solicitar_fin(mesa_t *m);
void    mesa_notificar_abandono(mesa_t *m, cliente_t *c);

const char *mesa_res_str(mesa_res_t r);

#endif
