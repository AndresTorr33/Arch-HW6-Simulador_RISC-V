// =============================================================================
// main.cpp — Punto de entrada interactivo del Simulador RISC-V RV32I
// =============================================================================
// Uso:
//   ./riscv_sim.exe <archivo.bin>
//
// Carga el archivo binario crudo y abre una terminal interactiva para
// ejecutar paso a paso (step), ver registros (regs) o memoria (mem).
//
// Estandar: C++17
// =============================================================================

#include "Simulator.hpp"

#include <cstdlib>     // EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>    // cerr, cout
#include <stdexcept>   // runtime_error, exception
#include <string>      // string
#include <sstream>     // stringstream
#include <iomanip>     // hex, dec

int main(int argc, char* argv[])
{
    // -------------------------------------------------------------------------
    // Validar argumentos de linea de comandos
    // -------------------------------------------------------------------------
    if (argc < 2) {
        cerr << "Error: se requiere un archivo binario como argumento.\n"
                  << "Uso:   " << argv[0] << " <archivo.bin>\n"
                  << "Ejemplo: " << argv[0] << " programa.bin\n";
        return EXIT_FAILURE;
    }

    // -------------------------------------------------------------------------
    // Crear el simulador y cargar el archivo
    // -------------------------------------------------------------------------
    Simulator sim;

    if (!sim.load_from_file(argv[1])) {
        // load_from_file ya imprimio el mensaje de error
        return EXIT_FAILURE;
    }

    cout << "[LOAD OK] \"" << argv[1] << "\" listo para ejecucion.\n";
    cout << "Comandos: 'step' (avanzar), 'run' (ejecutar hasta el final), 'regs' (registros), 'mem <hex>' (memoria), 'exit' (salir)\n\n";

    // -------------------------------------------------------------------------
    // Bucle interactivo REPL (Read-Eval-Print Loop)
    // -------------------------------------------------------------------------
    string line;
    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break; // Maneja EOF (Ctrl+D / Ctrl+Z)

        stringstream ss(line);
        string command;
        ss >> command;

        if (command == "exit") {
            cout << "Saliendo del simulador...\n";
            break;
        } 
        else if (command == "step") {
            try {
                sim.step();
                cout << "Instruccion ejecutada.\n";
            } 
            catch (const exception& e) {
                cerr << "[RUNTIME ERROR] " << e.what() << "\n";
                cerr << "La simulacion se ha detenido debido a una excepcion.\n";
                break; // Rompe el bucle si hay un error fatal (ej. mem fault)
            }
        }
        else if (command == "run") {
            cout << "Ejecutando programa continuamente (limite de 10,000 inst)...\n";
            uint64_t instrucciones_ejecutadas = 0;
            const uint64_t LIMITE = 10000;
            
            try {
                // Ejecuta en bucle con límite
                while (instrucciones_ejecutadas < LIMITE) {
                    sim.step();
                    instrucciones_ejecutadas++;
                }
                
                cout << "\n[TIMEOUT] Simulacion detenida. Se alcanzo el limite de " << LIMITE << " instrucciones.\n";
                cout << "El PC se quedo atascado en la direccion: 0x" << hex << sim.get_pc() << dec << "\n";
                
            } 
            catch (const exception& e) {
                cout << "\nSimulacion finalizada correctamente tras " << instrucciones_ejecutadas << " instrucciones.\n";
                cout << "Razon/Estado: " << e.what() << "\n";
            }
        }
        else if (command == "regs") {
            sim.print_state();
        } 
        else if (command == "mem") {
            uint32_t addr;
            // Lee la direccion en formato hexadecimal (sin necesidad del 0x)
            if (ss >> hex >> addr) {
                try {
                    // Muestra 4 bytes (1 word) leidos desde la memoria
                    uint32_t val = sim.read_word(addr);
                    cout << "Memoria [0x" << hex << addr << "]: 0x" 
                              << setfill('0') << setw(8) << val << dec << "\n";
                } catch (const exception& e) {
                    cerr << "[MEM ERROR] " << e.what() << "\n";
                }
            } else {
                cout << "Uso: mem <direccion_hex> (ej. mem 64 para la direccion 0x64)\n";
            }
        }
        else if (!command.empty()) {
            cout << "Comando desconocido.\n";
        }
    }

    return EXIT_SUCCESS;
}