// =============================================================================
// main.cpp — Punto de entrada temporal del Simulador RISC-V RV32I
// =============================================================================
// Uso:
//   ./simulator <archivo.bin>
//
// Carga el archivo binario crudo en el simulador y muestra el estado
// inicial (PC + registros) para verificar la carga.
//
// Estandar: C++17
// =============================================================================

#include "Simulator.hpp"

#include <cstdlib>     // EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>    // std::cerr, std::cout
#include <stdexcept>   // std::runtime_error, std::exception

int main(int argc, char* argv[])
{
    // -------------------------------------------------------------------------
    // Validar argumentos de línea de comandos
    // -------------------------------------------------------------------------
    if (argc < 2) {
        std::cerr << "Error: se requiere un archivo binario como argumento.\n"
                  << "Uso:   " << argv[0] << " <archivo.bin>\n"
                  << "Ejemplo: " << argv[0] << " programa.bin\n";
        return EXIT_FAILURE;
    }

    // -------------------------------------------------------------------------
    // Crear el simulador (estado limpio: registros=0, PC=0x0, memoria=0)
    // -------------------------------------------------------------------------
    Simulator sim;

    // -------------------------------------------------------------------------
    // Cargar el binario en memoria desde la direccion 0x00000000
    // -------------------------------------------------------------------------
    if (!sim.load_from_file(argv[1])) {
        // load_from_file ya imprimio el mensaje de error
        return EXIT_FAILURE;
    }

    // -------------------------------------------------------------------------
    // Mostrar el estado del simulador tras la carga
    // -------------------------------------------------------------------------
    try {
        sim.print_state();
    }
    catch (const std::runtime_error& e) {
        // Error de alineacion o acceso fuera de rango durante print_state
        // (no debería ocurrir en esta fase, pero se captura por seguridad)
        std::cerr << "\n[RUNTIME ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
