#ifndef TABLERO_H
#define TABLERO_H

#include <stdbool.h>
#include <pthread.h>
#include "config.h"

// Tablero de 28 casillas: 25 de la grilla 5x5 + 3 del trono (L, R y T)
#define GRILLA_FILAS 5
#define GRILLA_COLS 5
#define NUM_CASILLAS_GRILLA 25
#define CASILLA_TRONO_IZQ 25
#define CASILLA_TRONO_DER 26
#define CASILLA_CUSPIDE 27
#define TOTAL_CASILLAS 28

#define MAX_PERROS 12
#define CAPTURAS_PARA_GANAR 6 // Victoria del puma por comerse 6 perros
#define MAX_VECINOS 5
#define MAX_MOVIMIENTOS 16 // Maximo de jugadas posibles por pieza

#define CASILLA_VACIA 0
#define CASILLA_PUMA 1
#define CASILLA_PERRO 2

typedef struct {
int id;
int tipo; // Vacia, puma o perro
} Casilla;

typedef enum {
JUEGO_EN_CURSO = 0,
JUEGO_GANA_PUMA, // Puma comio lo necesario
JUEGO_GANA_PERROS, // Puma acorralado
JUEGO_DETENIDO // Detenido por el juez o tiempo
} estado_juego_t;

// Estructura para mover al puma (paso simple o salto comiendo)
typedef struct {
int destino;
int captura; // Casilla del perro comido o -1
} Movimiento;

typedef struct {
Casilla casillas[TOTAL_CASILLAS];
int pos_puma;
int pos_perro[MAX_PERROS]; // Donde esta cada perro (-1 si murio)
int perros_iniciales;
int perros_capturados;
int umbral_victoria; // Meta de perros a capturar
unsigned long movimientos; // Contador de jugadas para el log
estado_juego_t estado; // Estado actual con lock

pthread_mutex_t lock_tablero; // El mutex clave de la mesa
pthread_cond_t cond_fin; // Alerta a los hilos cuando acaba
bool listo; // Marca si el tablero esta ok
} Tablero;

// Funciones para iniciar y limpiar el tablero
int inicializar_tablero(Tablero* t, const config_t* cfg);
void destruir_tablero(Tablero* t);
int tablero_validar_disposicion(const config_t* cfg);

// Validaciones de reglas (se llaman con lock)
bool es_adyacente(int origen, int destino);
bool movimiento_valido_perro(const Tablero* t, int origen, int destino);
bool movimiento_valido_puma(const Tablero* t, int origen, int destino, int* casilla_capturada);
bool puma_esta_acorralado(const Tablero* t);

int listar_movimientos_puma(const Tablero* t, Movimiento out[MAX_MOVIMIENTOS]);
int listar_movimientos_perro(const Tablero* t, int id_perro, int out_destinos[MAX_VECINOS]);

// Movimientos atomicos bajo lock
bool aplicar_movimiento_puma(Tablero* t, int destino, int* captura_out);
bool aplicar_movimiento_perro(Tablero* t, int id_perro, int destino);

// Consultas y control del estado del juego
bool tablero_terminado(Tablero* t);
estado_juego_t tablero_estado(Tablero* t);
void tablero_terminar(Tablero* t, estado_juego_t estado);
bool tablero_dormir(Tablero* t, int ms);

#endif