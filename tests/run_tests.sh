#!/usr/bin/env bash
# Uso: tests/run_tests.sh [binario]      (por defecto ./torneo; sin Parte 1: ./torneo_stub)
BIN=${1:-./torneo}
D="$(cd "$(dirname "$0")" && pwd)"
PASS=0; FAIL=0
ok()   { echo "  [OK]   $1"; PASS=$((PASS+1)); }
bad()  { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }
campo(){ grep '^RESUMEN' <<<"$1" | tail -1 | sed -n "s/.* $2=\([0-9]*\).*/\1/p"; }
espera_n() { [ "$1" = "$2" ]; }

[ -x "$BIN" ] || { echo "No existe $BIN (compila con 'make' o 'make stub')"; exit 1; }
echo "== Binario: $BIN"

echo "-- 1. Argumentos y configuración inválidos (deben fallar con código != 0)"
for args in "" "4" "0 2" "4 0" "abc 2" "4 2 /no/existe.txt" "4 2 $D/invalido.txt" "4 2 $D/invalido_tablero.txt" "4 2 $D/invalido_simbolo.txt"; do
    timeout 10 $BIN $args >/dev/null 2>&1; rc=$?
    [ $rc -ne 0 ] && [ $rc -lt 124 ] && ok "rechaza: '$args' (rc=$rc)" || bad "no rechaza '$args' (rc=$rc)"
done

# Invariantes que deben cumplirse en toda corrida normal (sin SIGINT)
verifica_normal() {  # $1=nombre  $2=N  $3=M  $4=salida
    local out="$4" N=$2 M=$3
    local part=$(campo "$out" partidas) mx=$(campo "$out" max_mesas) sp=$(campo "$out" sin_pareja)
    local suma=$(( $(campo "$out" gana_puma)+$(campo "$out" gana_perros)+$(campo "$out" abandonos)+$(campo "$out" empates)+$(campo "$out" interrumpidas)+$(campo "$out" errores) ))
    [ "$(campo "$out" llegaron)" = "$N" ] && ok "$1: llegaron los $N clientes" || bad "$1: llegaron=$(campo "$out" llegaron) != $N"
    [ -n "$mx" ] && [ "$mx" -le "$M" ] && ok "$1: nunca más de M=$M mesas (max=$mx)" || bad "$1: max_mesas=$mx > M=$M"
    [ "$suma" = "$part" ] && ok "$1: cada partida tiene exactamente 1 resultado ($part)" || bad "$1: resultados($suma) != partidas($part)"
    [ $((2*part+sp)) = "$N" ] && ok "$1: todos los jugadores atendidos (2*$part+$sp=$N)" || bad "$1: 2*$part+$sp != $N"
}

echo "-- 2. Caso básico N=10 M=2"
out=$(timeout 60 $BIN 10 2 $D/rapido.txt 2>&1); rc=$?
[ $rc -eq 0 ] && ok "termina con código 0" || bad "rc=$rc"
verifica_normal "básico" 10 2 "$out"
[ "$(campo "$out" partidas)" = 5 ] && ok "básico: 5 partidas" || bad "básico: partidas=$(campo "$out" partidas)"

echo "-- 3. N impar (N=11, M=3): un jugador queda sin pareja"
out=$(timeout 60 $BIN 11 3 $D/rapido.txt 2>&1)
verifica_normal "impar" 11 3 "$out"
[ "$(campo "$out" sin_pareja)" = 1 ] && ok "impar: 1 sin pareja" || bad "impar: sin_pareja=$(campo "$out" sin_pareja)"

echo "-- 4. M=1 (serializa todo) y M > N/2"
out=$(timeout 60 $BIN 8 1 $D/rapido.txt 2>&1); verifica_normal "M=1" 8 1 "$out"
[ "$(campo "$out" max_mesas)" = 1 ] && ok "M=1: exactamente 1 mesa a la vez" || bad "M=1: max=$(campo "$out" max_mesas)"
out=$(timeout 60 $BIN 6 50 $D/rapido.txt 2>&1); verifica_normal "M=50" 6 50 "$out"

echo "-- 4b. Disposición inicial personalizada (puma en el trono)"
out=$(timeout 60 $BIN 4 2 $D/disposicion.txt 2>&1); verifica_normal "disposición" 4 2 "$out"

echo "-- 5. Carga: N=400 M=8 (muchos hilos)"
out=$(timeout 120 $BIN 400 8 $D/rapido.txt 2>&1); verifica_normal "carga" 400 8 "$out"

echo "-- 6. Fallos de jugadores (abandono)"
out=$(timeout 60 $BIN 40 4 $D/fallos.txt 2>&1); rc=$?
[ $rc -eq 0 ] && ok "termina con código 0" || bad "rc=$rc"
verifica_normal "fallos" 40 4 "$out"
echo "     abandonos=$(campo "$out" abandonos) errores=$(campo "$out" errores) (con la Parte 1 real, esperable > 0)"

echo "-- 7. Tiempo máximo de partida"
out=$(timeout 60 $BIN 20 4 $D/timeout.txt 2>&1); verifica_normal "timeout" 20 4 "$out"
echo "     empates=$(campo "$out" empates)"

echo "-- 8. SIGINT a mitad del torneo (N=1000 M=4)"
antes=$(ls /dev/shm 2>/dev/null | wc -l)
tmp=$(mktemp)
$BIN 1000 4 $D/lento.txt > "$tmp" 2>&1 &
pid=$!
sleep 1.5
kill -INT $pid
t0=$(date +%s.%N)
wait $pid; rc=$?
t1=$(date +%s.%N)
out=$(cat "$tmp"); rm -f "$tmp"
[ $rc -eq 0 ] && ok "SIGINT: apagado ordenado (rc=0)" || bad "SIGINT: rc=$rc"
awk "BEGIN{exit !($t1-$t0 < 3.0)}" && ok "SIGINT: termina en < 3 s" || bad "SIGINT: tardó demasiado"
grep -q '^RESUMEN' <<<"$out" && ok "SIGINT: imprime resumen final" || bad "SIGINT: sin resumen"
[ "$(campo "$out" llegaron)" -lt 1000 ] && ok "SIGINT: dejaron de llegar jugadores ($(campo "$out" llegaron)/1000)" || bad "SIGINT: llegaron todos"
[ "$(campo "$out" max_mesas)" -le 4 ] && ok "SIGINT: max_mesas<=4" || bad "SIGINT: max_mesas excedido"
ps -p $pid >/dev/null 2>&1 && bad "SIGINT: proceso sigue vivo" || ok "SIGINT: sin procesos huérfanos"
despues=$(ls /dev/shm 2>/dev/null | wc -l)
[ "$antes" = "$despues" ] && ok "SIGINT: /dev/shm sin cambios ($antes -> $despues)" || bad "SIGINT: /dev/shm cambió"

echo "-- 9. Valgrind (si está instalado): ejecución normal y SIGINT a mitad de partida"
if command -v valgrind >/dev/null; then
    VG="valgrind -q --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99"
    $VG $BIN 6 2 $D/rapido.txt >/dev/null 2>&1; rc=$?
    [ $rc -eq 0 ] && ok "valgrind limpio (ejecución normal)" || bad "valgrind normal rc=$rc"
    tmp=$(mktemp)
    $VG $BIN 40 3 $D/lento.txt >"$tmp" 2>"$tmp.err" &
    pid=$!
    sleep 4
    kill -INT $pid
    wait $pid; rc=$?
    [ $rc -eq 0 ] && ok "valgrind limpio con SIGINT a mitad de partida" || { bad "valgrind SIGINT rc=$rc"; head -20 "$tmp.err"; }
    grep -q interrumpidas=[1-9] "$tmp" && ok "valgrind SIGINT: había partidas en curso al interrumpir" || bad "valgrind SIGINT: no había partidas en curso"
    rm -f "$tmp" "$tmp.err"
else echo "  (valgrind no instalado: omitido)"; fi

echo
echo "RESULTADO: $PASS OK, $FAIL FAIL"
[ $FAIL -eq 0 ]
