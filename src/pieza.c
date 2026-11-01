#define _POSIX_C_SOURCE 200809L
#include "pieza.h"

#include <pthread.h>
#include <stdlib.h>

#include "log.h"

// Calcula el tiempo aleatorio de espera entre jugadas
static int retardo_ms(const ArgPieza* a, unsigned int* semilla) {
return config_azar(semilla, a->cfg->delay_min_ms, a->cfg->delay_max_ms);
}

// Imprime el mensaje final cuando acaba el juego
static void informar_fin(const ArgPieza* a, estado_juego_t estado) {
if (estado == JUEGO_GANA_PUMA)
registrar_evento(a->id_mesa, "Puma capturo %d perros: GANA EL PUMA", a->tablero->perros_capturados);
else if (estado == JUEGO_GANA_PERROS)
registrar_evento(a->id_mesa, "Puma acorralado sin movimientos: GANAN LOS PERROS");
}

// Hilo del Puma
void* hilo_puma(void* arg) {
ArgPieza* a = (ArgPieza*)arg;
Tablero* t = a->tablero;
unsigned int semilla = a->semilla;

while (tablero_dormir(t, retardo_ms(a, &semilla))) {
pthread_mutex_lock(&t->lock_tablero);
Movimiento opciones[MAX_MOVIMIENTOS];
int n = listar_movimientos_puma(t, opciones);

if (n > 0 && t->estado == JUEGO_EN_CURSO) {
// Prioriza saltar sobre un perro antes de un paso normal
int caps[MAX_MOVIMIENTOS], nc = 0;
for (int i = 0; i < n; i++) if (opciones[i].captura >= 0) caps[nc++] = i;
int elegida = nc > 0 ? caps[(unsigned)rand_r(&semilla) % (unsigned)nc]
: (int)((unsigned)rand_r(&semilla) % (unsigned)n);
int origen = t->pos_puma, captura = -1;
if (aplicar_movimiento_puma(t, opciones[elegida].destino, &captura)) {
if (captura >= 0)
registrar_evento(a->id_mesa, "Puma salta %d -> %d y captura el perro de la casilla %d (%d/%d)",
origen, opciones[elegida].destino, captura,
t->perros_capturados, t->umbral_victoria);
else
registrar_evento(a->id_mesa, "Puma se mueve %d -> %d", origen, opciones[elegida].destino);
if (a->cfg->log_tablero_ms == 0) imprimir_tablero_consola(t, a->id_mesa);
if (t->estado != JUEGO_EN_CURSO) informar_fin(a, t->estado);
}
}
pthread_mutex_unlock(&t->lock_tablero);
}
return NULL;
}

// Hilo de cada Perro
void* hilo_perro(void* arg) {
ArgPieza* a = (ArgPieza*)arg;
Tablero* t = a->tablero;
unsigned int semilla = a->semilla;
const int id_perro = a->id_pieza - 1;

if (id_perro >= t->perros_iniciales) return NULL;

while (tablero_dormir(t, retardo_ms(a, &semilla))) {
pthread_mutex_lock(&t->lock_tablero);
if (t->pos_perro[id_perro] < 0) { // Si ya fue capturado, muere el hilo
pthread_mutex_unlock(&t->lock_tablero);
break;
}
int destinos[MAX_VECINOS];
int n = listar_movimientos_perro(t, id_perro, destinos);
if (n > 0 && t->estado == JUEGO_EN_CURSO) {
int destino = destinos[(unsigned)rand_r(&semilla) % (unsigned)n];
int origen = t->pos_perro[id_perro];
if (aplicar_movimiento_perro(t, id_perro, destino)) {
registrar_evento(a->id_mesa, "Perro %d se mueve %d -> %d", id_perro + 1, origen, destino);
if (a->cfg->log_tablero_ms == 0) imprimir_tablero_consola(t, a->id_mesa);
if (t->estado != JUEGO_EN_CURSO) informar_fin(a, t->estado);
}
}
pthread_mutex_unlock(&t->lock_tablero);
}
return NULL;
}