#include "log.h"
#include <stdio.h>

// imprime el estado visual del tablero 5x5 mas el trono
void imprimir_tablero_consola(Tablero* t, int id_mesa) {
    printf("\n=== MESA %d: ESTADO DEL TABLERO ===\n", id_mesa);

    for (int i = 0; i < 25; i++) {
        char simbolo = '.';
        if (t->casillas[i].tipo == CASILLA_PUMA) simbolo = 'P';
        else if (t->casillas[i].tipo == CASILLA_PERRO) simbolo = 'd';

        printf("%c ", simbolo);
        if ((i + 1) % 5 == 0) printf("\n");
    }

    // mostramos la casilla 25 que es el trono
    char trono_simbolo = '.';
    if (t->casillas[25].tipo == CASILLA_PUMA) trono_simbolo = 'P';
    else if (t->casillas[25].tipo == CASILLA_PERRO) trono_simbolo = 'd';

    printf("Trono [25]: %c\n", trono_simbolo);
    printf("Perros capturados: %d/12\n", t->perros_capturados);
    printf("===================================\n\n");
}

// muestra mensajes y eventos ocurridos en una mesa
void registrar_evento(int id_mesa, const char* mensaje) {
    printf("[Mesa %d] %s\n", id_mesa, mensaje);
}