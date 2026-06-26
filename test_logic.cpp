// =============================================================================
// test_logic.cpp — Prueba de logica en memoria (sin archivo binario externo)
// =============================================================================
// Verifica las operaciones clave del simulador:
//   1. x0 siempre es 0 (Regla de Oro)
//   2. write_word / read_word en little-endian
//   3. write_halfword / read_halfword en little-endian
//   4. Inspeccion de bytes individuales (verifica el orden real en memoria)
//   5. Deteccion de acceso no alineado
//
// Estandar: C++17
// =============================================================================

#include "Simulator.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

// Helper para reportar resultado de cada prueba
static void test(const std::string& name, bool ok) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
    if (!ok) std::exit(1);
}

int main() {
    std::cout << "=== Tests de logica del Simulador RISC-V ===\n\n";

    Simulator sim;

    // ---------------------------------------------------------
    // TEST 1: Regla de Oro — x0 siempre vale 0
    // ---------------------------------------------------------
    sim.set_reg(0, 0xDEADBEEF);   // intentar escribir en x0
    test("x0 siempre es 0 (set_reg ignorado)", sim.get_reg(0) == 0);

    // ---------------------------------------------------------
    // TEST 2: Registros normales (x1-x31) pueden escribirse
    // ---------------------------------------------------------
    sim.set_reg(1, 0x12345678);
    test("x1 escribe y lee correctamente", sim.get_reg(1) == 0x12345678u);

    sim.set_reg(31, 0xCAFEBABE);
    test("x31 escribe y lee correctamente", sim.get_reg(31) == 0xCAFEBABEu);

    // ---------------------------------------------------------
    // TEST 3: write_word + read_word (little-endian round-trip)
    // ---------------------------------------------------------
    sim.write_word(0x0, 0xDEADBEEF);
    uint32_t w = sim.read_word(0x0);
    test("write_word/read_word round-trip 0xDEADBEEF", w == 0xDEADBEEFu);

    // ---------------------------------------------------------
    // TEST 4: Verificar orden de bytes en memoria (little-endian)
    //   0xDEADBEEF en 0x0 → [0]=0xEF [1]=0xBE [2]=0xAD [3]=0xDE
    // ---------------------------------------------------------
    test("Byte[0] = 0xEF (LSB)", sim.read_byte(0x0) == 0xEFu);
    test("Byte[1] = 0xBE",       sim.read_byte(0x1) == 0xBEu);
    test("Byte[2] = 0xAD",       sim.read_byte(0x2) == 0xADu);
    test("Byte[3] = 0xDE (MSB)", sim.read_byte(0x3) == 0xDEu);

    // ---------------------------------------------------------
    // TEST 5: write_halfword + read_halfword (little-endian)
    //   0xABCD en 0x4 → [4]=0xCD [5]=0xAB
    // ---------------------------------------------------------
    sim.write_halfword(0x4, 0xABCD);
    test("write_halfword/read_halfword round-trip 0xABCD",
         sim.read_halfword(0x4) == 0xABCDu);
    test("Halfword: Byte[4] = 0xCD (LSB)", sim.read_byte(0x4) == 0xCDu);
    test("Halfword: Byte[5] = 0xAB (MSB)", sim.read_byte(0x5) == 0xABu);

    // ---------------------------------------------------------
    // TEST 6: write_byte + read_byte
    // ---------------------------------------------------------
    sim.write_byte(0x8, 0x42);
    test("write_byte/read_byte round-trip 0x42", sim.read_byte(0x8) == 0x42u);

    // ---------------------------------------------------------
    // TEST 7: Alineacion forzada — read_word en direccion impar
    // ---------------------------------------------------------
    bool alignment_caught = false;
    try {
        sim.read_word(0x1); // direccion no alineada a 4 bytes
    } catch (const std::runtime_error&) {
        alignment_caught = true;
    }
    test("read_word en dir. no alineada lanza excepcion", alignment_caught);

    // ---------------------------------------------------------
    // TEST 8: Alineacion forzada — read_halfword en direccion impar
    // ---------------------------------------------------------
    bool hw_alignment_caught = false;
    try {
        sim.read_halfword(0x3); // impar
    } catch (const std::runtime_error&) {
        hw_alignment_caught = true;
    }
    test("read_halfword en dir. no alineada lanza excepcion", hw_alignment_caught);

    // ---------------------------------------------------------
    // TEST 9: Acceso fuera de rango
    // ---------------------------------------------------------
    bool oob_caught = false;
    try {
        sim.read_word(Simulator::MEM_SIZE - 2); // ultimo word invalido
    } catch (const std::runtime_error&) {
        oob_caught = true;
    }
    test("read_word fuera de rango lanza excepcion", oob_caught);

    // ---------------------------------------------------------
    // TEST 10: print_state no lanza excepciones
    // ---------------------------------------------------------
    bool print_ok = true;
    try {
        sim.print_state();
    } catch (...) {
        print_ok = false;
    }
    test("print_state ejecuta sin excepciones", print_ok);

    std::cout << "\n✓ Todos los tests pasaron correctamente.\n";
    return 0;
}
