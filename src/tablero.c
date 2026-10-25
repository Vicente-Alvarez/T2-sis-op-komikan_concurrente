#include "tablero.h"
#include <stdio.h>
#include <stdlib.h>

// inicializamos las casillas, el puma y el mutex
void inicializar_tablero(Tablero* t) {
// preparamos el mutex que protege la mesa
pthread_mutex_init(&t->lock_tablero, NULL);

// limpiamos todas las casillas
for (int i = 0; i < TOTAL_CASILLAS; i++) {
t->casillas[i].id = i;
t->casillas[i].tipo = CASILLA_VACIA;
}

// colocamos al puma en el centro de la grilla 5x5
t->pos_puma = 12;
t->casillas[12].tipo = CASILLA_PUMA;

// ponemos los 12 perros en las primeras posiciones
for (int i = 0; i < 12; i++) {
t->casillas[i].tipo = CASILLA_PERRO;
}
t->perros_capturados = 0;
}

// liberamos el mutex al cerrar el tablero
void destruir_tablero(Tablero* t) {
pthread_mutex_destroy(&t->lock_tablero);
}

// revisa si dos casillas son vecinas directas
bool es_adyacente(int origen, int destino) {
if (origen < 0 || origen >= TOTAL_CASILLAS || destino < 0 || destino >= TOTAL_CASILLAS) return false;
int row1 = origen / 5, col1 = origen % 5;
int row2 = destino / 5, col2 = destino % 5;

// la casilla 25 es el trono y conecta con la cuspide (casilla 2)
if (origen == 25) return (destino == 2);
if (destino == 25) return (origen == 2);

return (abs(row1 - row2) + abs(col1 - col2)) == 1;
}

// un perro solo se mueve si la casilla esta vacia y es adyacente
bool movimiento_valido_perro(Tablero* t, int origen, int destino) {
if (t->casillas[origen].tipo != CASILLA_PERRO) return false;
if (t->casillas[destino].tipo != CASILLA_VACIA) return false;
return es_adyacente(origen, destino);
}

// valida si el puma puede mover a una casilla o si captura a un perro saltando
bool movimiento_valido_puma(Tablero* t, int origen, int destino, int* casilla_capturada) {
*casilla_capturada = -1;
if (t->casillas[origen].tipo != CASILLA_PUMA) return false;
if (t->casillas[destino].tipo != CASILLA_VACIA) return false;

// si es un paso simple a la casilla de al lado
if (es_adyacente(origen, destino)) return true;

// si intenta capturar saltando 2 casillas en linea recta
int row1 = origen / 5, col1 = origen % 5;
int row2 = destino / 5, col2 = destino % 5;

// salto vertical
if (abs(row1 - row2) == 2 && col1 == col2) {
int intermedio = ((row1 + row2) / 2) * 5 + col1;
if (t->casillas[intermedio].tipo == CASILLA_PERRO) {
*casilla_capturada = intermedio;
return true;
}
} 
// salto horizontal
else if (abs(col1 - col2) == 2 && row1 == row2) {
int intermedio = row1 * 5 + ((col1 + col2) / 2);
if (t->casillas[intermedio].tipo == CASILLA_PERRO) {
*casilla_capturada = intermedio;
return true;
}
}
return false;
}

// comprueba si el puma se quedo sin movimientos posibles
bool puma_esta_acorralado(Tablero* t) {
int dummy;
for (int i = 0; i < TOTAL_CASILLAS; i++) {
if (movimiento_valido_puma(t, t->pos_puma, i, &dummy)) return false;
}
return true;
}