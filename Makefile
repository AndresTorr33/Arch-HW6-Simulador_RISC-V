# =============================================================================
# Makefile — Simulador RISC-V RV32I
# =============================================================================
# Comandos:
#   make          → compila el simulador (genera ./simulator o simulator.exe)
#   make run      → compila y ejecuta con un binario de prueba (TEST_BIN)
#   make clean    → elimina objetos y el ejecutable
#
# Uso:
#   make
#   ./simulator programa.bin
# =============================================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET   := simulator
SRCS     := Simulator.cpp main.cpp
OBJS     := $(SRCS:.cpp=.o)

# Binario de prueba para 'make run' (sobreescribible desde la CLI)
TEST_BIN ?= test.bin

# =============================================================================
# Reglas
# =============================================================================

.PHONY: all run clean

## Compilar el simulador
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "[BUILD OK] Ejecutable generado: $(TARGET)"

## Compilar cada objeto (depende del header)
%.o: %.cpp Simulator.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

## Compilar y ejecutar con TEST_BIN
run: all
	./$(TARGET) $(TEST_BIN)

## Compilar y ejecutar tests de logica (sin archivo .bin externo)
test: Simulator.o test_logic.cpp Simulator.hpp
	$(CXX) $(CXXFLAGS) -o test_logic Simulator.o test_logic.cpp
	./test_logic

## Limpiar artefactos de compilacion
clean:
	$(RM) $(OBJS) $(TARGET) $(TARGET).exe test_logic test_logic.exe
	@echo "[CLEAN] Artefactos eliminados."
