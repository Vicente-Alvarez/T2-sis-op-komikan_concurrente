#define _POSIX_C_SOURCE 200809L
#include "cliente.h"

#include <errno.h>
#include <stdlib.h>

#include "bitacora.h"
#include "matchmaker.h"
#include "mesa.h"
#include "util.h"

const char *rol_str(rol_t r) { return r == ROL_PUMA ? "Puma" : "Perros"; }

const char *cl_resultado_str(cl_resultado_t r) {
    switch (r) {
    case CLR_NINGUNO:        return "sin resultado";
    case CLR_GANO:           return "GANO";
    case CLR_PERDIO:         return "PERDIO";
    case CLR_GANO_ABANDONO:  return "GANO por abandono del rival";
    case CLR_ABANDONO:       return "ABANDONO (se cayo)";
    case CLR_EMPATE:         return "EMPATE por tiempo";
    case CLR_INTERRUMPIDO:   return "INTERRUMPIDO por el servidor";
    case CLR_SIN_RIVAL:      return "SIN RIVAL";
    case CLR_ERROR:          return "ERROR del servidor";
    }
    return "?";
}

cliente_t *cliente_crear(int id, rol_t rol, const config_t *cfg, matchmaker_t *mm) {
    cliente_t *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->id = id;
    c->rol = rol;
    c->cfg = cfg;
    c->mm = mm;
    c->estado = CL_ESPERANDO;
    c->resultado = CLR_NINGUNO;
    c->conectado = true;
    c->fallo_en_ms = -1;
    c->semilla = config_derivar_semilla(cfg->seed, 1, (unsigned int)id);

    unsigned int st = c->semilla;
    double u = (double)rand_r(&st) / ((double)RAND_MAX + 1.0);
    if (cfg->prob_fallo > 0.0 && u < cfg->prob_fallo)
        c->fallo_en_ms = config_azar(&st, cfg->fallo_min_ms, cfg->fallo_max_ms);

    if (pthread_mutex_init(&c->mtx, NULL) != 0) { free(c); return NULL; }
    if (pthread_cond_init(&c->cond, NULL) != 0) {
        pthread_mutex_destroy(&c->mtx);
        free(c);
        return NULL;
    }
    return c;
}

void cliente_asignar_mesa(cliente_t *c, mesa_t *m) {
    pthread_mutex_lock(&c->mtx);
    c->mesa = m;
    c->estado = CL_JUGANDO;
    pthread_cond_broadcast(&c->cond);
    pthread_mutex_unlock(&c->mtx);
}

void cliente_finalizar(cliente_t *c, cl_estado_t est, cl_resultado_t r) {
    pthread_mutex_lock(&c->mtx);
    if (c->estado == CL_ESPERANDO || c->estado == CL_JUGANDO) {
        c->estado = est;
        c->resultado = r;
    }
    c->mesa = NULL;
    pthread_cond_broadcast(&c->cond);
    pthread_mutex_unlock(&c->mtx);
}

bool cliente_conectado(cliente_t *c) {
    pthread_mutex_lock(&c->mtx);
    bool r = c->conectado;
    pthread_mutex_unlock(&c->mtx);
    return r;
}

static void *cliente_hilo(void *arg) {
    cliente_t *c = arg;
    bitacora_evento(-1, "Cliente %d (%s) llega al servidor", c->id, rol_str(c->rol));

    if (matchmaker_encolar(c->mm, c) != 0) {
        cliente_finalizar(c, CL_CANCELADO, CLR_SIN_RIVAL);
        bitacora_evento(-1, "Cliente %d rechazado: servidor cerrando", c->id);
        return NULL;
    }

    pthread_mutex_lock(&c->mtx);
    while (c->estado == CL_ESPERANDO) pthread_cond_wait(&c->cond, &c->mtx);

    if (c->estado == CL_JUGANDO) {
        struct timespec dl;
        if (c->fallo_en_ms >= 0) util_abs_en(&dl, c->fallo_en_ms);

        while (c->estado == CL_JUGANDO) {
            if (c->fallo_en_ms >= 0) {
                int rc = pthread_cond_timedwait(&c->cond, &c->mtx, &dl);
                if (rc == ETIMEDOUT && c->estado == CL_JUGANDO) {

                    c->conectado = false;
                    c->fallo_en_ms = -1;
                    bitacora_evento(-1, "Cliente %d (%s) FALLA/ABORTA durante la partida", c->id, rol_str(c->rol));
                    mesa_notificar_abandono(c->mesa, c);
                }
            } else {
                pthread_cond_wait(&c->cond, &c->mtx);
            }
        }
    }
    cl_resultado_t r = c->resultado;
    pthread_mutex_unlock(&c->mtx);

    bitacora_evento(-1, "Cliente %d (%s) termina: %s", c->id, rol_str(c->rol), cl_resultado_str(r));
    return NULL;
}

int cliente_iniciar(cliente_t *c) {
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 256 * 1024);
    int rc = pthread_create(&c->hilo, &at, cliente_hilo, c);
    pthread_attr_destroy(&at);
    if (rc == 0) c->hilo_creado = true;
    return rc;
}

void cliente_unir(cliente_t *c) {
    if (c->hilo_creado) {
        pthread_join(c->hilo, NULL);
        c->hilo_creado = false;
    }
}

void cliente_destruir(cliente_t *c) {
    if (!c) return;
    pthread_cond_destroy(&c->cond);
    pthread_mutex_destroy(&c->mtx);
    free(c);
}
