#ifndef TABLERO_H
#define TABLERO_H

#include <stdbool.h>
#include <pthread.h>

// 25 casillas grilla 5x5 + 3 casillas del trono (25, 26, 27) = 28 casillas
#define TOTAL_CASILLAS 28
#define CASILLA_VACIA 0
#define CASILLA_PUMA 1
#define CASILLA_PERRO 2

typedef struct {
int id;
int tipo; // VACIA, PUMA o PERRO
} Casilla;

typedef struct {
Casilla casillas[TOTAL_CASILLAS];
int pos_puma;
int perros_capturados;
pthread_mutex_t lock_tablero; // Exclusión mutua para la mesa
} Tablero;

void inicializar_tablero(Tablero* t);
void destruir_tablero(Tablero* t);
bool es_adyacente(int origen, int destino);
bool movimiento_valido_perro(Tablero* t, int origen, int destino);
bool movimiento_valido_puma(Tablero* t, int origen, int destino, int* casilla_capturada);
bool puma_esta_acorralado(Tablero* t);

#endif // TABLERO_H