#include "pieza.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void* hilo_puma(void* arg) {
ArgPieza* datos = (ArgPieza*) arg;
Tablero* t = datos->tablero;

while (*(datos->juego_activo)) {
sem_wait(datos->sem_turno);
if (!(*(datos->juego_activo))) break;

pthread_mutex_lock(&t->lock_tablero);

if (puma_esta_acorralado(t)) {
registrar_evento(datos->id_mesa, "¡Puma acorralado! Ganan los Perros.");
*(datos->juego_activo) = false;
pthread_mutex_unlock(&t->lock_tablero);
sem_post(datos->sem_siguiente);
break;
}

int captura = -1;
int destino_elegido = -1;

for (int i = 0; i < TOTAL_CASILLAS; i++) {
if (movimiento_valido_puma(t, t->pos_puma, i, &captura)) {
destino_elegido = i;
if (captura != -1) break;
}
}

if (destino_elegido != -1) {
int origen = t->pos_puma;
t->casillas[origen].tipo = CASILLA_VACIA;
t->casillas[destino_elegido].tipo = CASILLA_PUMA;
t->pos_puma = destino_elegido;

if (captura != -1) {
t->casillas[captura].tipo = CASILLA_VACIA;
t->perros_capturados++;
registrar_evento(datos->id_mesa, "Puma se comio un Perro.");
} else {
registrar_evento(datos->id_mesa, "Puma se movio.");
}

imprimir_tablero_consola(t, datos->id_mesa);

if (t->perros_capturados >= 12) {
registrar_evento(datos->id_mesa, "¡Puma capturo a los 12 Perros! Gana el Puma.");
*(datos->juego_activo) = false;
}
}

pthread_mutex_unlock(&t->lock_tablero);
usleep(100000);

sem_post(datos->sem_siguiente);
}
return NULL;
}

void* hilo_perro(void* arg) {
ArgPieza* datos = (ArgPieza*) arg;
Tablero* t = datos->tablero;

while (*(datos->juego_activo)) {
sem_wait(datos->sem_turno);
if (!(*(datos->juego_activo))) break;

pthread_mutex_lock(&t->lock_tablero);

bool movido = false;
for (int origen = 0; origen < TOTAL_CASILLAS; origen++) {
if (t->casillas[origen].tipo == CASILLA_PERRO) {
for (int destino = 0; destino < TOTAL_CASILLAS; destino++) {
if (movimiento_valido_perro(t, origen, destino)) {
t->casillas[origen].tipo = CASILLA_VACIA;
t->casillas[destino].tipo = CASILLA_PERRO;
movido = true;
break;
}
}
}
if (movido) break;
}

if (movido) {
registrar_evento(datos->id_mesa, "Un Perro se movio.");
imprimir_tablero_consola(t, datos->id_mesa);
}

pthread_mutex_unlock(&t->lock_tablero);
usleep(100000);

sem_post(datos->sem_siguiente);
}
return NULL;
}