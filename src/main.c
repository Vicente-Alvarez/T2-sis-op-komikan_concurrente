#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cliente.h"
#include "config.h"
#include "bitacora.h"
#include "matchmaker.h"

#define MAX_CLIENTES 1000000

static volatile sig_atomic_t g_interrumpido = 0;
static void manejador_senal(int sig) { (void)sig; g_interrumpido = 1; }

typedef struct {
    matchmaker_t *mm;
    const config_t *cfg;
    cliente_t **clientes;
    int n;
    int creados;
} llegadas_t;

static void *hilo_llegadas(void *arg) {
    llegadas_t *L = arg;
    unsigned int est = config_derivar_semilla(L->cfg->seed, 0, 0);

    rol_t *roles = malloc((size_t)L->n * sizeof *roles);
    if (roles) {
        for (int i = 0; i < L->n; i++) roles[i] = (i % 2 == 0) ? ROL_PUMA : ROL_PERROS;
        for (int i = L->n - 1; i > 0; i--) {
            int j = (int)((unsigned int)rand_r(&est) % (unsigned int)(i + 1));
            rol_t tmp = roles[i]; roles[i] = roles[j]; roles[j] = tmp;
        }
        for (int i = 0; i < L->n; i++) {
            if (!matchmaker_dormir(L->mm, config_azar(&est, L->cfg->llegada_min_ms, L->cfg->llegada_max_ms)))
                break;
            cliente_t *c = cliente_crear(i + 1, roles[i], L->cfg, L->mm);
            if (!c) break;
            L->clientes[i] = c;
            if (cliente_iniciar(c) != 0) {
                L->clientes[i] = NULL;
                cliente_destruir(c);
                bitacora_evento(-1, "No se pudo crear el hilo del cliente %d: se detienen las llegadas", i + 1);
                break;
            }
            L->creados = i + 1;
        }
        free(roles);
    }
    matchmaker_fin_llegadas(L->mm, L->creados);
    return NULL;
}

static int parsear_entero(const char *s, int minimo, int maximo, int *out) {
    char *fin;
    errno = 0;
    long v = strtol(s, &fin, 10);
    if (errno || fin == s || *fin != '\0' || v < minimo || v > maximo) return -1;
    *out = (int)v;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n  N = jugadores (clientes), M = mesas simultáneas máximas\n", argv[0]);
        return 2;
    }
    int N, M;
    if (parsear_entero(argv[1], 1, MAX_CLIENTES, &N) != 0 || parsear_entero(argv[2], 1, 100000, &M) != 0) {
        fprintf(stderr, "Error: N y M deben ser enteros positivos (N<=%d, M<=100000)\n", MAX_CLIENTES);
        return 2;
    }

    config_t cfg;
    config_por_defecto(&cfg);
    if (argc == 4 && config_cargar(&cfg, argv[3]) != 0) return 2;

    if (bitacora_iniciar(NULL) != 0) {
        fprintf(stderr, "Error: no se pudo iniciar el log\n");
        return 1;
    }
    bitacora_evento(-1, "Servidor: N=%d jugadores, M=%d mesas", N, M);
    config_imprimir(&cfg, stdout);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = manejador_senal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigset_t bloq, previa;
    sigemptyset(&bloq);
    sigaddset(&bloq, SIGINT);
    sigaddset(&bloq, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &bloq, &previa);

    int rc_salida = 0;
    matchmaker_t *mm = matchmaker_crear(M, &cfg);
    cliente_t **clientes = calloc((size_t)N, sizeof *clientes);
    if (!mm || !clientes) {
        fprintf(stderr, "Error: sin memoria\n");
        free(clientes);
        matchmaker_destruir(mm);
        bitacora_cerrar();
        return 1;
    }

    llegadas_t L = { mm, &cfg, clientes, N, 0 };
    pthread_t th_llegadas;
    if (matchmaker_iniciar(mm) != 0) {
        fprintf(stderr, "Error: no se pudo lanzar el matchmaker\n");
        rc_salida = 1;
        goto limpieza_sin_hilos;
    }
    if (pthread_create(&th_llegadas, NULL, hilo_llegadas, &L) != 0) {
        fprintf(stderr, "Error: no se pudo lanzar el generador de llegadas\n");
        matchmaker_cerrar(mm);
        matchmaker_unir(mm);
        rc_salida = 1;
        goto limpieza_sin_hilos;
    }
    pthread_sigmask(SIG_SETMASK, &previa, NULL);

    bool cerrado = false;
    while (!matchmaker_esperar_fin(mm, 50)) {
        if (g_interrumpido && !cerrado) {
            bitacora_evento(-1, "SIGINT recibido: sin nuevos jugadores, cerrando partidas activas...");
            matchmaker_cerrar(mm);
            cerrado = true;
        }
    }

    pthread_join(th_llegadas, NULL);
    matchmaker_unir(mm);

limpieza_sin_hilos:;
    int sin_pareja = 0, cancelados = 0, creados = L.creados;
    for (int i = 0; i < N; i++) {
        cliente_t *c = clientes[i];
        if (!c) continue;
        cliente_unir(c);
        if (c->estado == CL_SIN_PAREJA) sin_pareja++;
        else if (c->estado == CL_CANCELADO) cancelados++;
        cliente_destruir(c);
    }
    free(clientes);

    mm_stats_t st;
    matchmaker_stats(mm, &st);
    bitacora_evento(-1, "Servidor detenido%s. Recursos liberados.", g_interrumpido ? " (por SIGINT)" : "");
    matchmaker_destruir(mm);
    bitacora_cerrar();

    printf("RESUMEN solicitados=%d llegaron=%d partidas=%d gana_puma=%d gana_perros=%d abandonos=%d "
           "empates=%d interrumpidas=%d errores=%d sin_pareja=%d cancelados=%d max_mesas=%d M=%d\n",
           N, creados, st.partidas, st.gana_puma, st.gana_perros, st.abandonos, st.empates,
           st.interrumpidas, st.errores, sin_pareja, cancelados, st.max_mesas, M);
    fflush(stdout);
    return rc_salida;
}
