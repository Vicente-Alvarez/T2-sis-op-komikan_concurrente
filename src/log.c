#include "log.h"
#include <stdio.h>

void imprimir_tablero_consola(Tablero* t, int id_mesa) {
printf("\n MESA %d: ESTADO DEL TABLERO \n", id_mesa);

// Grilla principal 5x5 (0 a 24)
for (int i = 0; i < 25; i++) {
char simbolo = '.';
if (t->casillas[i].tipo == CASILLA_PUMA) simbolo = 'P';
else if (t->casillas[i].tipo == CASILLA_PERRO) simbolo = 'd';

printf("%c ", simbolo);
if ((i + 1) % 5 == 0) printf("\n");
}

// Representación del Trono (Casillas 25, 26 y 27)
char t25 = (t->casillas[25].tipo == CASILLA_PUMA) ? 'P' : (t->casillas[25].tipo == CASILLA_PERRO) ? 'd' : '.';
char t26 = (t->casillas[26].tipo == CASILLA_PUMA) ? 'P' : (t->casillas[26].tipo == CASILLA_PERRO) ? 'd' : '.';
char t27 = (t->casillas[27].tipo == CASILLA_PUMA) ? 'P' : (t->casillas[27].tipo == CASILLA_PERRO) ? 'd' : '.';

printf("Trono: [Izq: %c] [Der: %c] [Cuspite: %c]\n", t25, t26, t27);
printf("Perros capturados: %d/12\n", t->perros_capturados);
printf("n\n");
}

void registrar_evento(int id_mesa, const char* mensaje) {
printf("[Mesa %d] %s\n", id_mesa, mensaje);
}