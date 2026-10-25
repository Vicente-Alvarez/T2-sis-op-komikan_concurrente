#ifndef TABLERO_H
#define TABLERO_H

#include <stdbool.h>
#include <pthread.h>

// 25 casillas del tablero mas la 25 del trono
#define TOTAL_CASILLAS 26 
#define CASILLA_VACIA 0
#define CASILLA_PUMA 1
#define CASILLA_PERRO 2

// estructura de cada casilla del tablero
typedef struct {
int id;
int tipo; // vacia, puma o perro
} Casilla;

// estado general del tablero por mesa
typedef struct {
Casilla casillas[TOTAL_CASILLAS];
int pos_puma;
int perros_capturados;
pthread_mutex_t lock_tablero; // mutex global para que no se pisen al mover
} Tablero;

// funciones del tablero
void inicializar_tablero(Tablero* t);
void destruir_tablero(Tablero* t);
bool es_adyacente(int origen, int destino);
bool movimiento_valido_perro(Tablero* t, int origen, int destino);
bool movimiento_valido_puma(Tablero* t, int origen, int destino, int* casilla_capturada);
bool puma_esta_acorralado(Tablero* t);

#endif // TABLERO_H