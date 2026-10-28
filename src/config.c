#define _POSIX_C_SOURCE 200809L
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void config_por_defecto(config_t *c) {
    memset(c, 0, sizeof *c);
    c->delay_min_ms = 50;
    c->delay_max_ms = 300;
    c->seed_auto = true;
    c->seed = (unsigned int)time(NULL) ^ ((unsigned int)getpid() << 16);
    c->llegada_min_ms = 0;
    c->llegada_max_ms = 100;
    c->prob_fallo = 0.0;
    c->fallo_min_ms = 200;
    c->fallo_max_ms = 2000;
    c->max_partida_ms = 0;
    c->log_tablero_ms = 1000;
    c->usa_disposicion = false;
}

static char *recortar(char *s) {
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

static int leer_int(const char *v, int *out) {
    char *fin;
    errno = 0;
    long x = strtol(v, &fin, 10);
    if (errno || fin == v || *fin != '\0' || x < 0 || x > 1000000000L) return -1;
    *out = (int)x;
    return 0;
}

static int leer_double(const char *v, double *out) {
    char *fin;
    errno = 0;
    double x = strtod(v, &fin);
    if (errno || fin == v || *fin != '\0') return -1;
    *out = x;
    return 0;
}

static int leer_uint(const char *v, unsigned int *out) {
    char *fin;
    errno = 0;
    unsigned long x = strtoul(v, &fin, 10);
    if (errno || fin == v || *fin != '\0' || x > UINT_MAX) return -1;
    *out = (unsigned int)x;
    return 0;
}

static int aplicar_clave(config_t *c, const char *k, const char *v, int nl, const char *ruta) {
    int rc = 0;
    if      (!strcmp(k, "delay_min_ms"))   rc = leer_int(v, &c->delay_min_ms);
    else if (!strcmp(k, "delay_max_ms"))   rc = leer_int(v, &c->delay_max_ms);
    else if (!strcmp(k, "llegada_min_ms")) rc = leer_int(v, &c->llegada_min_ms);
    else if (!strcmp(k, "llegada_max_ms")) rc = leer_int(v, &c->llegada_max_ms);
    else if (!strcmp(k, "fallo_min_ms"))   rc = leer_int(v, &c->fallo_min_ms);
    else if (!strcmp(k, "fallo_max_ms"))   rc = leer_int(v, &c->fallo_max_ms);
    else if (!strcmp(k, "max_partida_ms")) rc = leer_int(v, &c->max_partida_ms);
    else if (!strcmp(k, "log_tablero_ms")) rc = leer_int(v, &c->log_tablero_ms);
    else if (!strcmp(k, "prob_fallo"))     rc = leer_double(v, &c->prob_fallo);
    else if (!strcmp(k, "seed")) {
        if (!strcmp(v, "auto")) return 0;
        rc = leer_uint(v, &c->seed);
        if (rc == 0) c->seed_auto = false;
    } else {
        fprintf(stderr, "config: %s:%d: clave desconocida '%s' (ignorada)\n", ruta, nl, k);
        return 0;
    }
    if (rc != 0) fprintf(stderr, "config: %s:%d: valor inválido para '%s': '%s'\n", ruta, nl, k, v);
    return rc;
}

int config_cargar(config_t *c, const char *ruta) {
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "config: no se pudo abrir '%s': %s\n", ruta, strerror(errno));
        return -1;
    }
    char linea[256];
    int nl = 0, rc = 0;
    bool en_tablero = false;

    while (rc == 0 && fgets(linea, sizeof linea, f)) {
        nl++;
        size_t len = strlen(linea);
        if (len == sizeof linea - 1 && linea[len - 1] != '\n' && !feof(f)) {
            fprintf(stderr, "config: %s:%d: línea demasiado larga\n", ruta, nl);
            rc = -1;
            break;
        }

        while (len > 0 && isspace((unsigned char)linea[len - 1])) linea[--len] = '\0';

        if (en_tablero) {
            char *ini = linea;
            while (isspace((unsigned char)*ini)) ini++;
            if (*ini == '\0') continue;
            if (!strcmp(ini, "fin")) { en_tablero = false; continue; }
            if (c->disp_filas >= CFG_MAX_FILAS || strlen(linea) > CFG_MAX_COLS) {
                fprintf(stderr, "config: %s:%d: fila del tablero fuera de tamaño (máx %dx%d)\n",
                        ruta, nl, CFG_MAX_FILAS, CFG_MAX_COLS);
                rc = -1;
                break;
            }
            strcpy(c->disposicion[c->disp_filas++], linea);
            continue;
        }

        char *h = strchr(linea, '#');
        if (h) *h = '\0';
        char *ln = recortar(linea);
        if (*ln == '\0') continue;

        if (!strcmp(ln, "tablero:")) {
            en_tablero = true;
            c->disp_filas = 0;
            continue;
        }
        char *eq = strchr(ln, '=');
        if (!eq) {
            fprintf(stderr, "config: %s:%d: se esperaba clave=valor\n", ruta, nl);
            rc = -1;
            break;
        }
        *eq = '\0';
        rc = aplicar_clave(c, recortar(ln), recortar(eq + 1), nl, ruta);
    }
    fclose(f);

    if (rc == 0 && en_tablero) {
        fprintf(stderr, "config: %s: bloque 'tablero:' sin 'fin'\n", ruta);
        rc = -1;
    }
    if (rc == 0) {
        c->usa_disposicion = c->disp_filas > 0;
        rc = config_validar(c);
    }
    return rc;
}

int config_validar(const config_t *c) {
    int err = 0;
    if (c->delay_min_ms > c->delay_max_ms)     { fprintf(stderr, "config: delay_min_ms > delay_max_ms\n"); err = -1; }
    if (c->llegada_min_ms > c->llegada_max_ms) { fprintf(stderr, "config: llegada_min_ms > llegada_max_ms\n"); err = -1; }
    if (c->fallo_min_ms > c->fallo_max_ms)     { fprintf(stderr, "config: fallo_min_ms > fallo_max_ms\n"); err = -1; }
    if (c->prob_fallo < 0.0 || c->prob_fallo > 1.0) { fprintf(stderr, "config: prob_fallo fuera de [0,1]\n"); err = -1; }
    if (c->usa_disposicion) {
        int p = 0, d = 0, celdas = 0;
        for (int i = 0; i < c->disp_filas; i++)
            for (const char *s = c->disposicion[i]; *s; s++) {
                if (*s == ' ') continue;
                celdas++;
                if (*s == 'P') p++;
                else if (*s == 'D') d++;
                else if (*s != '.') { fprintf(stderr, "config: caracter '%c' inválido en el tablero (use P, D, .)\n", *s); err = -1; }
            }
        if (celdas == 0)      { fprintf(stderr, "config: el bloque tablero está vacío\n"); err = -1; }
        if (p != 1)           { fprintf(stderr, "config: la disposición debe tener exactamente 1 'P' (hay %d)\n", p); err = -1; }
        if (d < 1 || d > 12)  { fprintf(stderr, "config: la disposición debe tener entre 1 y 12 'D' (hay %d)\n", d); err = -1; }
    }
    return err;
}

void config_imprimir(const config_t *c, FILE *out) {
    fprintf(out, "config: delay=[%d,%d]ms seed=%u%s llegada=[%d,%d]ms prob_fallo=%.2f fallo=[%d,%d]ms max_partida=%dms disposicion=%s\n",
            c->delay_min_ms, c->delay_max_ms, c->seed, c->seed_auto ? " (auto)" : "",
            c->llegada_min_ms, c->llegada_max_ms, c->prob_fallo, c->fallo_min_ms, c->fallo_max_ms,
            c->max_partida_ms, c->usa_disposicion ? "personalizada" : "por defecto");
}

unsigned int config_derivar_semilla(unsigned int base, unsigned int tipo, unsigned int id) {
    uint64_t x = ((uint64_t)base << 32) ^ ((uint64_t)tipo << 24) ^ (uint64_t)id;
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return (unsigned int)(x ^ (x >> 32));
}

int config_azar(unsigned int *estado, int min, int max) {
    if (max <= min) return min;
    return min + (int)((unsigned int)rand_r(estado) % (unsigned int)(max - min + 1));
}
