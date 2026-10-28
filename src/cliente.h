#ifndef CLIENTE_H
#define CLIENTE_H

#include <pthread.h>
#include <stdbool.h>
#include "config.h"

typedef struct mesa mesa_t;
typedef struct matchmaker matchmaker_t;

typedef enum { ROL_PUMA = 0, ROL_PERROS = 1 } rol_t;

typedef enum {
    CL_ESPERANDO = 0,
    CL_JUGANDO,
    CL_TERMINADO,
    CL_SIN_PAREJA,
    CL_CANCELADO
} cl_estado_t;

typedef enum {
    CLR_NINGUNO = 0,
    CLR_GANO,
    CLR_PERDIO,
    CLR_GANO_ABANDONO,
    CLR_ABANDONO,
    CLR_EMPATE,
    CLR_INTERRUMPIDO,
    CLR_SIN_RIVAL,
    CLR_ERROR
} cl_resultado_t;

typedef struct cliente {
    int  id;
    rol_t rol;
    unsigned int semilla;
    int  fallo_en_ms;

    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    cl_estado_t     estado;
    cl_resultado_t  resultado;
    bool            conectado;
    mesa_t         *mesa;

    struct cliente *sig;
    pthread_t       hilo;
    bool            hilo_creado;
    matchmaker_t   *mm;
    const config_t *cfg;
} cliente_t;

cliente_t *cliente_crear(int id, rol_t rol, const config_t *cfg, matchmaker_t *mm);
int        cliente_iniciar(cliente_t *c);
void       cliente_unir(cliente_t *c);
void       cliente_destruir(cliente_t *c);

void cliente_asignar_mesa(cliente_t *c, mesa_t *m);
void cliente_finalizar(cliente_t *c, cl_estado_t est, cl_resultado_t r);
bool cliente_conectado(cliente_t *c);

const char *rol_str(rol_t r);
const char *cl_resultado_str(cl_resultado_t r);

#endif
