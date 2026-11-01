CC      = gcc
WARN    = -Wall -Wextra -std=c17
OPT     = -O2 -g
SAN     =
LDLIBS  = -pthread

SRC_P2  = src/main.c src/config.c src/matchmaker.c src/cliente.c src/mesa.c src/bitacora.c
SRC_P1  = src/tablero.c src/pieza.c src/log.c

BUILD   = build
OBJ     = $(patsubst src/%.c,$(BUILD)/%.o,$(SRC_P2) $(SRC_P1))
DEPS    = $(OBJ:.o=.d)

.PHONY: all debug tsan test valgrind clean help

all: torneo

torneo: $(OBJ)
	$(CC) $(WARN) $(OPT) $(SAN) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(WARN) $(OPT) $(SAN) -MMD -MP -Isrc -pthread -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

debug:
	$(MAKE) clean
	$(MAKE) all OPT="-O0 -g" SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"

tsan:
	$(MAKE) clean
	$(MAKE) all OPT="-O1 -g" SAN="-fsanitize=thread"

BIN ?= ./torneo
test: $(BIN)
	bash tests/run_tests.sh $(BIN)

valgrind: $(BIN)
	valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 \
	  $(BIN) 6 2 tests/rapido.txt > /dev/null

clean:
	rm -rf $(BUILD) torneo

help:
	@echo "make            compila ./torneo"
	@echo "make test       pruebas del servidor (tests/run_tests.sh)"
	@echo "make debug|tsan compila con ASan+UBSan / ThreadSanitizer"
	@echo "make valgrind   revisa fugas con valgrind"

-include $(DEPS)
