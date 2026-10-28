#define _POSIX_C_SOURCE 200809L
#include "matchmaker.h"

#include <errno.h>
#include <semaphore.h>
#include <stdlib.h>

#include "bitacora.h"
#include "util.h"

typedef struct { cliente_t *head, *tail; int n; } cola_t;

static void cola_push(cola_t *q, cliente_t *c) {
    c->sig = NULL;
    if (q->tail) q->tail->sig = c; else q->head = c;
    q->tail = c;
    q->n++;
}

static cliente_t *cola_pop(cola_t *q) {
    cliente_t *c = q->head;
    if (!c) return NULL;
    q->head = c->sig;
    if (!q->head) q->tail = NULL;
    c->sig = NULL;
    q->n--;
    return c;
}

struct matchmaker {

    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    pthread_cond_t  cond_cierre;
    pthread_cond_t  cond_fin;
    pthread_t       hilo;
    bool            hilo_creado;

    const config_t *cfg;
    sem_t    sem_mesas;
    int      max_mesas;
    cola_t   q_puma, q_perros;
    mesa_t **slots;
    int      activas;
    mesa_t  *terminadas;

    int  encolados, total_llegados, partidas_creadas;
    bool llegadas_terminadas, cerrando, fin_pedido, terminado;
    mm_stats_t st;
};

matchmaker_t *matchmaker_crear(int max_mesas, const config_t *cfg) {
    matchmaker_t *mm = calloc(1, sizeof *mm);
    if (!mm) return NULL;
    mm->slots = calloc((size_t)max_mesas, sizeof *mm->slots);
    if (!mm->slots) { free(mm); return NULL; }
    mm->max_mesas = max_mesas;
    mm->cfg = cfg;
    if (sem_init(&mm->sem_mesas, 0, (unsigned int)max_mesas) != 0) { free(mm->slots); free(mm); return NULL; }
    pthread_mutex_init(&mm->mtx, NULL);
    pthread_cond_init(&mm->cond, NULL);
    pthread_cond_init(&mm->cond_cierre, NULL);
    pthread_cond_init(&mm->cond_fin, NULL);
    return mm;
}

void matchmaker_destruir(matchmaker_t *mm) {
    if (!mm) return;
    pthread_cond_destroy(&mm->cond_fin);
    pthread_cond_destroy(&mm->cond_cierre);
    pthread_cond_destroy(&mm->cond);
    pthread_mutex_destroy(&mm->mtx);
    sem_destroy(&mm->sem_mesas);
    free(mm->slots);
    free(mm);
}

int matchmaker_encolar(matchmaker_t *mm, cliente_t *c) {
    pthread_mutex_lock(&mm->mtx);
    if (mm->cerrando) {
        pthread_mutex_unlock(&mm->mtx);
        return -1;
    }
    cola_push(c->rol == ROL_PUMA ? &mm->q_puma : &mm->q_perros, c);
    mm->encolados++;
    int np = mm->q_puma.n, nd = mm->q_perros.n;
    pthread_cond_broadcast(&mm->cond);
    pthread_mutex_unlock(&mm->mtx);
    bitacora_evento(-1, "Cliente %d (%s) en cola de espera [pumas=%d perros=%d]", c->id, rol_str(c->rol), np, nd);
    return 0;
}

void matchmaker_fin_llegadas(matchmaker_t *mm, int creados) {
    pthread_mutex_lock(&mm->mtx);
    mm->llegadas_terminadas = true;
    mm->total_llegados = creados;
    pthread_cond_broadcast(&mm->cond);
    pthread_mutex_unlock(&mm->mtx);
}

bool matchmaker_dormir(matchmaker_t *mm, int ms) {
    struct timespec dl;
    util_abs_en(&dl, ms);
    pthread_mutex_lock(&mm->mtx);
    while (!mm->cerrando)
        if (pthread_cond_timedwait(&mm->cond_cierre, &mm->mtx, &dl) == ETIMEDOUT) break;
    bool ok = !mm->cerrando;
    pthread_mutex_unlock(&mm->mtx);
    return ok;
}

void matchmaker_cerrar(matchmaker_t *mm) {
    pthread_mutex_lock(&mm->mtx);
    mm->cerrando = true;
    pthread_cond_broadcast(&mm->cond);
    pthread_cond_broadcast(&mm->cond_cierre);
    pthread_mutex_unlock(&mm->mtx);
}

bool matchmaker_esperar_fin(matchmaker_t *mm, int ms) {
    struct timespec dl;
    util_abs_en(&dl, ms);
    pthread_mutex_lock(&mm->mtx);
    while (!mm->terminado)
        if (pthread_cond_timedwait(&mm->cond_fin, &mm->mtx, &dl) == ETIMEDOUT) break;
    bool t = mm->terminado;
    pthread_mutex_unlock(&mm->mtx);
    return t;
}

void matchmaker_stats(matchmaker_t *mm, mm_stats_t *out) {
    pthread_mutex_lock(&mm->mtx);
    *out = mm->st;
    pthread_mutex_unlock(&mm->mtx);
}

void matchmaker_mesa_finalizada(matchmaker_t *mm, mesa_t *m, mesa_res_t res) {
    pthread_mutex_lock(&mm->mtx);
    mm->st.partidas++;
    switch (res) {
    case MR_GANA_PUMA:        mm->st.gana_puma++;    break;
    case MR_GANA_PERROS:      mm->st.gana_perros++;  break;
    case MR_ABANDONO_PUMA:
    case MR_ABANDONO_PERROS:  mm->st.abandonos++;    break;
    case MR_EMPATE_TIEMPO:    mm->st.empates++;      break;
    case MR_INTERRUMPIDA:     mm->st.interrumpidas++; break;
    case MR_ERROR:            mm->st.errores++;      break;
    }
    mm->slots[m->id] = NULL;
    mm->activas--;
    m->sig = mm->terminadas;
    mm->terminadas = m;
    sem_post(&mm->sem_mesas);
    pthread_cond_broadcast(&mm->cond);
    pthread_mutex_unlock(&mm->mtx);
}

static void reciclar_terminadas(matchmaker_t *mm) {
    while (mm->terminadas) {
        mesa_t *m = mm->terminadas;
        mm->terminadas = m->sig;
        mesa_unir(m);
        mesa_destruir(m);
    }
}

static void cancelar_cola(cola_t *q, cl_resultado_t r) {
    cliente_t *c;
    while ((c = cola_pop(q)) != NULL) cliente_finalizar(c, CL_CANCELADO, r);
}

static void crear_partida(matchmaker_t *mm) {
    int slot = -1;
    for (int i = 0; i < mm->max_mesas; i++)
        if (!mm->slots[i]) { slot = i; break; }
    if (slot < 0) { sem_post(&mm->sem_mesas); return; }

    cliente_t *p = cola_pop(&mm->q_puma);
    cliente_t *d = cola_pop(&mm->q_perros);
    mesa_t *m = mesa_crear(slot, ++mm->partidas_creadas, p, d, mm->cfg, mm);
    if (!m) {
        cliente_finalizar(p, CL_CANCELADO, CLR_ERROR);
        cliente_finalizar(d, CL_CANCELADO, CLR_ERROR);
        sem_post(&mm->sem_mesas);
        return;
    }
    mm->slots[slot] = m;
    mm->activas++;
    if (mm->activas > mm->st.max_mesas) mm->st.max_mesas = mm->activas;

    cliente_asignar_mesa(p, m);
    cliente_asignar_mesa(d, m);
    bitacora_evento(-1, "Matchmaker: cliente %d (Puma) vs cliente %d (Perros) -> mesa %d  [activas=%d/%d]",
               p->id, d->id, slot, mm->activas, mm->max_mesas);

    if (mesa_iniciar(m) != 0) {
        mm->slots[slot] = NULL;
        mm->activas--;
        cliente_finalizar(p, CL_CANCELADO, CLR_ERROR);
        cliente_finalizar(d, CL_CANCELADO, CLR_ERROR);
        mesa_destruir(m);
        sem_post(&mm->sem_mesas);
    }
}

static bool tomar_ficha_mesa(matchmaker_t *mm) {
    pthread_mutex_unlock(&mm->mtx);
    bool ok = false;
    for (;;) {
        struct timespec dl;
        util_abs_en(&dl, 20);
        if (sem_timedwait(&mm->sem_mesas, &dl) == 0) { ok = true; break; }
        if (errno != ETIMEDOUT && errno != EINTR) break;
        pthread_mutex_lock(&mm->mtx);
        bool cerrando = mm->cerrando;
        pthread_mutex_unlock(&mm->mtx);
        if (cerrando) break;
    }
    pthread_mutex_lock(&mm->mtx);
    return ok;
}

static void *matchmaker_hilo(void *arg) {
    matchmaker_t *mm = arg;
    pthread_mutex_lock(&mm->mtx);

    for (;;) {
        reciclar_terminadas(mm);

        if (mm->cerrando) {

            cancelar_cola(&mm->q_puma, CLR_INTERRUMPIDO);
            cancelar_cola(&mm->q_perros, CLR_INTERRUMPIDO);
            if (!mm->fin_pedido) {
                mm->fin_pedido = true;
                for (int i = 0; i < mm->max_mesas; i++)
                    if (mm->slots[i]) mesa_solicitar_fin(mm->slots[i]);
            }
            if (mm->activas == 0) break;
        } else if (mm->q_puma.n > 0 && mm->q_perros.n > 0) {

            if (tomar_ficha_mesa(mm)) {
                if (mm->cerrando) sem_post(&mm->sem_mesas);
                else crear_partida(mm);
            }
            continue;
        } else {

            if (mm->llegadas_terminadas && mm->encolados >= mm->total_llegados && mm->activas == 0) {
                cliente_t *c;
                while ((c = cola_pop(&mm->q_puma)) != NULL)   cliente_finalizar(c, CL_SIN_PAREJA, CLR_SIN_RIVAL);
                while ((c = cola_pop(&mm->q_perros)) != NULL) cliente_finalizar(c, CL_SIN_PAREJA, CLR_SIN_RIVAL);
                break;
            }
        }
        pthread_cond_wait(&mm->cond, &mm->mtx);
    }

    mm->terminado = true;
    pthread_cond_broadcast(&mm->cond_fin);
    pthread_mutex_unlock(&mm->mtx);
    return NULL;
}

int matchmaker_iniciar(matchmaker_t *mm) {
    int rc = pthread_create(&mm->hilo, NULL, matchmaker_hilo, mm);
    if (rc == 0) mm->hilo_creado = true;
    return rc;
}

void matchmaker_unir(matchmaker_t *mm) {
    if (mm->hilo_creado) {
        pthread_join(mm->hilo, NULL);
        mm->hilo_creado = false;
    }
}
