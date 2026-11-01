#include "tablero.h"
#include <stdio.h>
#include <stdlib.h>

void inicializar_tablero(Tablero* t) {
pthread_mutex_init(&t->lock_tablero, NULL);

for (int i = 0; i < TOTAL_CASILLAS; i++) {
t->casillas[i].id = i;
t->casillas[i].tipo = CASILLA_VACIA;
}

// Puma en el centro de la grilla (casilla 12)
t->pos_puma = 12;
t->casillas[12].tipo = CASILLA_PUMA;

// Los 12 perros inician en las dos primeras filas (casillas 0 a 11)
for (int i = 0; i < 12; i++) {
t->casillas[i].tipo = CASILLA_PERRO;
}
t->perros_capturados = 0;
}

void destruir_tablero(Tablero* t) {
pthread_mutex_destroy(&t->lock_tablero);
}

bool es_adyacente(int origen, int destino) {
if (origen < 0 || origen >= TOTAL_CASILLAS || destino < 0 || destino >= TOTAL_CASILLAS) return false;

// Conexiones de la grilla 5x5 (casillas 0 a 24)
if (origen < 25 && destino < 25) {
int row1 = origen / 5, col1 = origen % 5;
int row2 = destino / 5, col2 = destino % 5;
return (abs(row1 - row2) + abs(col1 - col2)) == 1;
}

// Conexiones del Trono (casillas 25, 26, 27) con la base de la grilla (0, 1, 2) y entre sí:
// 25 (izq trono) conecta con 0, 1 y 27 (cúspide)
if ((origen == 25 && (destino == 0 || destino == 1 || destino == 27)) ||
(destino == 25 && (origen == 0 || origen == 1 || origen == 27))) return true;

// 26 (der trono) conecta con 1, 2 y 27 (cúspide)
if ((origen == 26 && (destino == 1 || destino == 2 || destino == 27)) ||
(destino == 26 && (origen == 1 || origen == 2 || origen == 27))) return true;

// 27 (cúspide) conecta con 25 y 26
if ((origen == 27 && (destino == 25 || destino == 26)) ||
(destino == 27 && (origen == 25 || origen == 26))) return true;

return false;
}

bool movimiento_valido_perro(Tablero* t, int origen, int destino) {
if (t->casillas[origen].tipo != CASILLA_PERRO) return false;
if (t->casillas[destino].tipo != CASILLA_VACIA) return false;
return es_adyacente(origen, destino);
}

bool movimiento_valido_puma(Tablero* t, int origen, int destino, int* casilla_capturada) {
*casilla_capturada = -1;
if (t->casillas[origen].tipo != CASILLA_PUMA) return false;
if (t->casillas[destino].tipo != CASILLA_VACIA) return false;

// Paso simple a casilla libre
if (es_adyacente(origen, destino)) return true;

// Salto de captura en la grilla 5x5
if (origen < 25 && destino < 25) {
int row1 = origen / 5, col1 = origen % 5;
int row2 = destino / 5, col2 = destino % 5;

if (abs(row1 - row2) == 2 && col1 == col2) {
int intermedio = ((row1 + row2) / 2) * 5 + col1;
if (t->casillas[intermedio].tipo == CASILLA_PERRO) {
*casilla_capturada = intermedio;
return true;
}
} else if (abs(col1 - col2) == 2 && row1 == row2) {
int intermedio = row1 * 5 + ((col1 + col2) / 2);
if (t->casillas[intermedio].tipo == CASILLA_PERRO) {
*casilla_capturada = intermedio;
return true;
}
}
}
return false;
}

bool puma_esta_acorralado(Tablero* t) {
int dummy;
for (int i = 0; i < TOTAL_CASILLAS; i++) {
if (movimiento_valido_puma(t, t->pos_puma, i, &dummy)) return false;
}
return true;
}