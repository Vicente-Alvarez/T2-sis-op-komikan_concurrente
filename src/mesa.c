#define _POSIX_C_SOURCE 200809L
#include "mesa.h"

#include <errno.h>
#include <semaphore.h>
#include <stdlib.h>

#include "bitacora.h"
#include "log.h"
#include "matchmaker.h"
#include "pieza.h"
#include "tablero.h"
#include "util.h"

#define JUEZ_POLL_MS 20
#define NUM_PIEZAS (1 + MAX_PERROS)

typedef struct {
    Tablero tablero;
    bool tablero_listo;
    ArgPieza args[NUM_PIEZAS];
    pthread_t hilos[NUM_PIEZAS];
    int lanzados;
} partida_t;

const char *mesa_res_str(mesa_res_t r) {
    switch (r) {
    case MR_GANA_PUMA:        return "gana el PUMA";
    case MR_GANA_PERROS:      return "ganan los PERROS (puma acorralado)";
    case MR_ABANDONO_PUMA:    return "ganan los PERROS por abandono del Puma";
    case MR_ABANDONO_PERROS:  return "gana el PUMA por abandono de los Perros";
    case MR_EMPATE_TIEMPO:    return "EMPATE por tiempo máximo";
    case MR_INTERRUMPIDA:     return "partida INTERRUMPIDA por el servidor";
    case MR_ERROR:            return "partida ANULADA (error / ambos abandonaron)";
    }
    return "?";
}

mesa_t *mesa_crear(int id, int partida, cliente_t *puma, cliente_t *perros,
                   const config_t *cfg, matchmaker_t *mm) {
    mesa_t *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->id = id;
    m->partida = partida;
    m->puma = puma;
    m->perros = perros;
    m->cfg = cfg;
    m->mm = mm;
    if (pthread_mutex_init(&m->mtx, NULL) != 0) { free(m); return NULL; }
    if (pthread_cond_init(&m->cond, NULL) != 0) {
        pthread_mutex_destroy(&m->mtx);
        free(m);
        return NULL;
    }
    return m;
}

void mesa_solicitar_fin(mesa_t *m) {
    pthread_mutex_lock(&m->mtx);
    m->fin_pedido = true;
    pthread_cond_broadcast(&m->cond);
    pthread_mutex_unlock(&m->mtx);
}

void mesa_notificar_abandono(mesa_t *m, cliente_t *c) {
    pthread_mutex_lock(&m->mtx);
    if (!m->abandono) m->abandono = c;
    pthread_cond_broadcast(&m->cond);
    pthread_mutex_unlock(&m->mtx);
}

static void partida_detener(partida_t *p) {
    if (p->tablero_listo) tablero_terminar(&p->tablero, JUEGO_DETENIDO);
    for (int i = 0; i < p->lanzados; i++) pthread_join(p->hilos[i], NULL);
    p->lanzados = 0;
}

static void partida_destruir(partida_t *p) {
    if (!p) return;
    if (p->tablero_listo) destruir_tablero(&p->tablero);
    free(p);
}

static partida_t *partida_iniciar(const mesa_t *m) {
    partida_t *p = calloc(1, sizeof *p);
    if (!p) return NULL;

    if (inicializar_tablero(&p->tablero, m->cfg) != 0) {
        free(p);
        return NULL;
    }
    p->tablero_listo = true;

    const unsigned int semilla = config_derivar_semilla(m->cfg->seed, 2, (unsigned int)m->partida);
    const int total = 1 + p->tablero.perros_iniciales;

    for (int i = 0; i < total; i++) {
        p->args[i].id_mesa = m->id;
        p->args[i].id_pieza = i;
        p->args[i].tablero = &p->tablero;
        p->args[i].cfg = m->cfg;
        p->args[i].semilla = config_derivar_semilla(semilla, 3, (unsigned int)i);
        if (pthread_create(&p->hilos[i], NULL, i == 0 ? hilo_puma : hilo_perro, &p->args[i]) != 0) {
            partida_detener(p);
            partida_destruir(p);
            return NULL;
        }
        p->lanzados++;
    }
    return p;
}

static mesa_res_t res_de_estado(estado_juego_t e, bool *real) {
    *real = true;
    if (e == JUEGO_GANA_PUMA)   return MR_GANA_PUMA;
    if (e == JUEGO_GANA_PERROS) return MR_GANA_PERROS;
    *real = false;
    return MR_ERROR;
}

static mesa_res_t juez_esperar_fin(mesa_t *m, partida_t *p) {
    const bool hay_limite = m->cfg->max_partida_ms > 0;
    const long limite = util_ms_monotonico() + m->cfg->max_partida_ms;
    const int periodo = m->cfg->log_tablero_ms;
    long prox_print = util_ms_monotonico() + periodo;

    for (;;) {
        pthread_mutex_lock(&m->mtx);
        if (!m->fin_pedido && !m->abandono) {
            struct timespec dl;
            util_abs_en(&dl, JUEZ_POLL_MS);
            pthread_cond_timedwait(&m->cond, &m->mtx, &dl);
        }
        bool fin = m->fin_pedido;
        cliente_t *ab = m->abandono;
        pthread_mutex_unlock(&m->mtx);

        if (periodo > 0 && util_ms_monotonico() >= prox_print) {
            pthread_mutex_lock(&p->tablero.lock_tablero);
            imprimir_tablero_consola(&p->tablero, m->id);
            pthread_mutex_unlock(&p->tablero.lock_tablero);
            prox_print += periodo;
        }

        bool real;
        mesa_res_t r = res_de_estado(tablero_estado(&p->tablero), &real);
        if (real) return r;

        if (ab) {
            cliente_t *otro = (ab == m->puma) ? m->perros : m->puma;
            if (!cliente_conectado(otro)) return MR_ERROR;
            return (ab == m->puma) ? MR_ABANDONO_PUMA : MR_ABANDONO_PERROS;
        }
        if (fin) return MR_INTERRUMPIDA;
        if (hay_limite && util_ms_monotonico() >= limite) return MR_EMPATE_TIEMPO;
    }
}

static void asignar_resultados(mesa_t *m, mesa_res_t res) {
    cl_resultado_t rp, rd;
    switch (res) {
    case MR_GANA_PUMA:       rp = CLR_GANO;          rd = CLR_PERDIO;  break;
    case MR_GANA_PERROS:     rp = CLR_PERDIO;        rd = CLR_GANO;    break;
    case MR_ABANDONO_PUMA:   rp = CLR_ABANDONO;      rd = CLR_GANO_ABANDONO; break;
    case MR_ABANDONO_PERROS: rp = CLR_GANO_ABANDONO; rd = CLR_ABANDONO; break;
    case MR_EMPATE_TIEMPO:   rp = CLR_EMPATE;        rd = CLR_EMPATE;  break;
    case MR_INTERRUMPIDA:    rp = CLR_INTERRUMPIDO;  rd = CLR_INTERRUMPIDO; break;
    case MR_ERROR:
    default:
        rp = cliente_conectado(m->puma)   ? CLR_ERROR : CLR_ABANDONO;
        rd = cliente_conectado(m->perros) ? CLR_ERROR : CLR_ABANDONO;
        break;
    }
    cliente_finalizar(m->puma,   CL_TERMINADO, rp);
    cliente_finalizar(m->perros, CL_TERMINADO, rd);
}

static void *juez_hilo(void *arg) {
    mesa_t *m = arg;
    const long t0 = util_ms_monotonico();
    mesa_res_t res = MR_ERROR;

    bitacora_evento(m->id, "JUEZ: inicia partida #%d  Puma=cliente %d  Perros=cliente %d",
                    m->partida, m->puma->id, m->perros->id);

    partida_t *p = partida_iniciar(m);
    if (!p) {
        bitacora_evento(m->id, "JUEZ: no se pudo crear el tablero o lanzar las piezas");
    } else {
        res = juez_esperar_fin(m, p);
        partida_detener(p);

        bool real;
        mesa_res_t final = res_de_estado(tablero_estado(&p->tablero), &real);
        if (real) res = final;
        bitacora_evento(m->id, "JUEZ: fin de la partida -> %s", mesa_res_str(res));

        imprimir_tablero_consola(&p->tablero, m->id);
        partida_destruir(p);
    }

    asignar_resultados(m, res);
    bitacora_evento(m->id, "JUEZ: partida #%d terminada en %ld ms. Mesa liberada.", m->partida,
                    util_ms_monotonico() - t0);

    matchmaker_mesa_finalizada(m->mm, m, res);
    return NULL;
}

int mesa_iniciar(mesa_t *m) {
    int rc = pthread_create(&m->hilo, NULL, juez_hilo, m);
    if (rc == 0) m->hilo_creado = true;
    return rc;
}

void mesa_unir(mesa_t *m) {
    if (m->hilo_creado) {
        pthread_join(m->hilo, NULL);
        m->hilo_creado = false;
    }
}

void mesa_destruir(mesa_t *m) {
    if (!m) return;
    pthread_cond_destroy(&m->cond);
    pthread_mutex_destroy(&m->mtx);
    free(m);
}
