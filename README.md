# Komikan Concurrente — Tarea 2 Sistemas Operativos

**Integrantes:** Vicente Álvarez · Xiomara Vidalon
**Curso:** Sistemas Operativos · Sección 1 · Tarea 2

Torneo multijugador del Komikan: servidor de emparejamiento (Matchmaker), mesas con un juez y piezas
concurrentes (1 Puma + 12 Perros) sobre un tablero compartido de 28 casillas (grilla 5x5 + trono de 3).

```
./torneo N M [config.txt]     N = jugadores (clientes) · M = mesas simultáneas máximas
```

Al terminar imprime una línea `RESUMEN ...` con las estadísticas (partidas, resultados, máximo de mesas simultáneas).

## Compilar y probar

Requiere Linux (o WSL), `gcc` y `make`. Opcional: `valgrind`.

```bash
sudo apt install build-essential valgrind   # si faltan las herramientas
```

| Comando | Qué hace |
|---|---|
| `make` | Compila `./torneo` (`gcc -Wall -Wextra -std=c17 -pthread`) |
| `make test` | Corre `tests/run_tests.sh` (incluye valgrind si está instalado) |
| `make valgrind` | Revisa fugas de memoria con valgrind |
| `make debug` | Compila con AddressSanitizer + UBSan |
| `make tsan` | Compila con ThreadSanitizer |
| `make clean` | Borra `build/` y `./torneo` (usar `make clean && make` para recompilar desde cero) |

Ejemplos:

```bash
./torneo 10 2                       # 10 jugadores, 2 mesas, configuración por defecto
./torneo 10 2 config.txt            # con archivo de configuración
./torneo 20 4 tests/fallos.txt      # jugadores que se caen a mitad de partida
./torneo 1000 4 tests/lento.txt     # prueba de Ctrl+C a mitad del torneo
```

## Estructura

```
src/       Parte 2 (servidor):  main.c config.c matchmaker.c cliente.c mesa.c bitacora.c util.h
           Parte 1 (juego):     tablero.c pieza.c log.c (+ .h)
tests/     run_tests.sh y configuraciones de prueba (*.txt)
informe/   informe.pdf
config.txt Configuración de ejemplo
Makefile   Compilación y pruebas
```

## Arquitectura de sincronización

* **Hilos pthread** en un único proceso: sin memoria compartida System V ni IPC que pueda quedar huérfano.
* **Mutex global por mesa** (`lock_tablero`): todo movimiento y captura del Puma se valida y aplica en una sola
  sección crítica, sin locks por casilla, por lo que no hay deadlock en los saltos.
* **Servidor y emparejamiento**: hilo Matchmaker con dos colas FIFO (Puma / Perros) y un **semáforo POSIX `sem_t`**
  inicializado en M que limita las partidas simultáneas. Si las M mesas están ocupadas, los pares esperan pasivamente.
* **Juez por mesa**: inicializa el tablero, lanza las 13 piezas, vigila fin/abandono/SIGINT/tiempo, imprime el
  tablero, detiene las piezas, hace `join` y destruye los recursos.
* **Piezas**: cada una duerme un retardo aleatorio en `[delay_min_ms, delay_max_ms]` (con `pthread_cond_timedwait`
  sobre `cond_fin`, que libera el mutex y despierta al terminar) y luego intenta mover.
* **Resiliencia**: un jugador que falla es detectado por el juez, que declara ganador al rival y libera todo.
  Con **SIGINT** el manejador solo levanta una bandera; `main` cierra las llegadas, cancela la cola, pide el fin a
  cada mesa y espera su cierre ordenado. La parada es cooperativa (no `pthread_cancel`).
* **Orden de locks**: `matchmaker.mtx -> cliente.mtx -> mesa.mtx` y `tablero.lock -> bitacora.mtx`.
  Nunca se llama al tablero con el mutex de la mesa tomado.
* **N impar**: queda un jugador sin rival (`sin_pareja`).

Más detalle (riesgos de deadlock/livelock/starvation y evaluación) en `informe/informe.pdf`.

## Formato de `config.txt`

Clave=valor, `#` comentarios. Las claves desconocidas se ignoran con una advertencia.

| Clave | Default | Significado |
|---|---|---|
| `delay_min_ms`, `delay_max_ms` | 50, 300 | Retardo de cada ficha entre decisiones |
| `seed` | auto | Semilla global (`auto` = se genera y se imprime) |
| `llegada_min_ms`, `llegada_max_ms` | 0, 100 | Tiempo entre llegadas de clientes |
| `prob_fallo` | 0 | Probabilidad de que un jugador se caiga a media partida |
| `fallo_min_ms`, `fallo_max_ms` | 200, 2000 | Momento de la caída |
| `max_partida_ms` | **30000** | Tiempo máximo de partida; 0 = sin límite. Si se excede, empate |
| `log_tablero_ms` | 1000 | 0 = las piezas imprimen el tablero en cada jugada; >0 = lo imprime el juez cada X ms |
| `tablero:` … `fin` | estándar | Disposición inicial (ver abajo) |

### Disposición inicial (`tablero:` … `fin`)

Se leen los símbolos no blancos en el orden de lectura de la figura del enunciado. Símbolos: `P` (Puma, exactamente 1),
`D` (Perro, de 1 a 12) y `.` (casilla libre). Los espacios no cuentan.

* **28 símbolos**: cúspide del trono, trono izquierdo y derecho, y luego las 5 filas de la grilla 5x5.
* **25 símbolos**: solo la grilla; el trono queda vacío.

```
tablero:
      .
    .   .
D D D D D
D D D D D
D D P . .
. . . . .
. . . . .
fin
```

Con los retardos por defecto las partidas duran entre 1 y 5 s; por eso 30 s es un tope seguro que evita partidas infinitas.
Cada cliente, mesa y pieza deriva su semilla de `seed` (`rand_r`, sin estado global): llegadas, roles y fallos son
reproducibles; el resultado exacto de cada partida depende además del planificador del SO.

## Pruebas (`tests/run_tests.sh`)

* Argumentos y configuraciones inválidas (deben fallar con código distinto de 0).
* Invariantes: nunca más de M mesas, un único resultado por partida, `2·partidas + sin_pareja = N`.
* N impar, M=1, M mayor que N/2, disposición inicial personalizada, carga (N=400, M=8).
* Fallos de jugadores (abandono) y tiempo máximo de partida (empate).
* SIGINT a mitad del torneo: apagado en menos de 3 s, sin procesos huérfanos, `/dev/shm` sin cambios.
* Valgrind (`--leak-check=full`) en ejecución normal **y con SIGINT a mitad de partida**: 0 errores, 0 fugas.

Configuraciones: `rapido`, `fallos`, `timeout`, `lento`, `semilla_fija`, `disposicion`, `invalido*`.
