// =============================================================================
// Simulator.cpp — Implementacion del Simulador RISC-V RV32I
// =============================================================================
// Implementa todos los metodos declarados en Simulator.hpp.
//
// Decisiones de diseño:
//   - Memoria little-endian construida byte a byte (portable, sin UB).
//   - Alineacion forzada: error explícito + excepcion en accesos no alineados.
//   - Regla de Oro: set_reg() ignora silenciosamente escrituras a x0.
//
// Estandar: C++17
// =============================================================================

#include "Simulator.hpp"

#include <algorithm>    // std::fill
#include <fstream>      // std::ifstream
#include <iomanip>      // std::hex, std::setw, std::setfill
#include <iostream>     // std::cout, std::cerr
#include <sstream>      // std::ostringstream
#include <stdexcept>    // std::runtime_error, std::out_of_range

// =============================================================================
// Constructor
// =============================================================================

Simulator::Simulator()
    : pc_(0x00000000u),
      memory_(MEM_SIZE, 0u)   // Inicializar toda la memoria a 0
{
    // Inicializar los 32 registros a 0 (x0 incluido — nunca cambiara)
    std::fill(std::begin(regs_), std::end(regs_), 0u);
}

// =============================================================================
// Helper: check_access — Alineacion y límites
// =============================================================================

void Simulator::check_access(uint32_t address, uint8_t size, const char* operation) const
{
    // --- Verificacion de alineacion (solo aplica para size > 1) ---
    if (size > 1 && (address % size) != 0) {
        std::ostringstream oss;
        oss << "\n[ALIGNMENT FAULT] " << operation << "()\n"
            << "  Direccion : 0x" << std::hex << std::setw(8)
                                  << std::setfill('0') << address << "\n"
            << "  Ancho     : " << std::dec << static_cast<int>(size) << " bytes\n"
            << "  Requerido : direccion multiplo de " << static_cast<int>(size)
            << " (address % " << static_cast<int>(size) << " == 0)\n"
            << "  Obtenido  : 0x" << std::hex << address
            << " % " << std::dec << static_cast<int>(size)
            << " == " << (address % size) << "\n"
            << "  → La simulacion se detiene.";
        std::cerr << oss.str() << "\n";
        throw std::runtime_error(oss.str());
    }

    // --- Verificacion de límites de memoria ---
    // Se usa uint64_t para evitar overflow al sumar address + size
    if (static_cast<uint64_t>(address) + size > static_cast<uint64_t>(MEM_SIZE)) {
        std::ostringstream oss;
        oss << "\n[OUT OF BOUNDS] " << operation << "()\n"
            << "  Direccion : 0x" << std::hex << std::setw(8)
                                  << std::setfill('0') << address << "\n"
            << "  Ancho     : " << std::dec << static_cast<int>(size) << " bytes\n"
            << "  Límite    : 0x" << std::hex << MEM_SIZE
            << " (" << std::dec << MEM_SIZE << " bytes = 1 MB)\n"
            << "  → La simulacion se detiene.";
        std::cerr << oss.str() << "\n";
        throw std::runtime_error(oss.str());
    }
}

// =============================================================================
// Acceso a registros
// =============================================================================

uint32_t Simulator::get_reg(uint8_t idx) const
{
    if (idx >= 32) {
        throw std::out_of_range(
            "[REGISTER ERROR] get_reg(): índice fuera de rango: "
            + std::to_string(idx) + " (maximo: 31)");
    }
    return regs_[idx];
}

void Simulator::set_reg(uint8_t idx, uint32_t val)
{
    if (idx >= 32) {
        throw std::out_of_range(
            "[REGISTER ERROR] set_reg(): índice fuera de rango: "
            + std::to_string(idx) + " (maximo: 31)");
    }

    // Regla de Oro RISC-V: x0 es hardwired a 0.
    // Cualquier intento de escritura se descarta silenciosamente.
    if (idx != 0) {
        regs_[idx] = val;
    }
}

// =============================================================================
// Lectura de memoria — little-endian
// =============================================================================

uint8_t Simulator::read_byte(uint32_t address) const
{
    check_access(address, 1, "read_byte");
    return memory_[address];
}

uint16_t Simulator::read_halfword(uint32_t address) const
{
    check_access(address, 2, "read_halfword");

    //  Direccion : address+0  |  address+1
    //  Bits      : [7:0]      |  [15:8]
    //  (little-endian: el byte en la direccion mas baja es el menos significativo)
    return   static_cast<uint16_t>(memory_[address])
           | static_cast<uint16_t>(memory_[address + 1]) << 8;
}

uint32_t Simulator::read_word(uint32_t address) const
{
    check_access(address, 4, "read_word");

    //  Direccion : address+0  |  address+1  |  address+2  |  address+3
    //  Bits      : [7:0]      |  [15:8]     |  [23:16]    |  [31:24]
    //  (little-endian: LSB en la direccion mas baja)
    return   static_cast<uint32_t>(memory_[address])
           | static_cast<uint32_t>(memory_[address + 1]) <<  8
           | static_cast<uint32_t>(memory_[address + 2]) << 16
           | static_cast<uint32_t>(memory_[address + 3]) << 24;
}

// =============================================================================
// Escritura de memoria — little-endian
// =============================================================================

void Simulator::write_byte(uint32_t address, uint8_t val)
{
    check_access(address, 1, "write_byte");
    memory_[address] = val;
}

void Simulator::write_halfword(uint32_t address, uint16_t val)
{
    check_access(address, 2, "write_halfword");

    // Descomponer el halfword en bytes, LSB primero (little-endian)
    memory_[address]     = static_cast<uint8_t>( val       & 0xFF); // bits [7:0]
    memory_[address + 1] = static_cast<uint8_t>((val >> 8) & 0xFF); // bits [15:8]
}

void Simulator::write_word(uint32_t address, uint32_t val)
{
    check_access(address, 4, "write_word");

    // Descomponer el word en bytes, LSB primero (little-endian)
    memory_[address]     = static_cast<uint8_t>( val        & 0xFF); // bits [7:0]
    memory_[address + 1] = static_cast<uint8_t>((val >>  8) & 0xFF); // bits [15:8]
    memory_[address + 2] = static_cast<uint8_t>((val >> 16) & 0xFF); // bits [23:16]
    memory_[address + 3] = static_cast<uint8_t>((val >> 24) & 0xFF); // bits [31:24]
}

// =============================================================================
// Cargador de archivos binarios
// =============================================================================

bool Simulator::load_from_file(const std::string& filename)
{
    // Abrir en modo binario y posicionar el cursor al final para medir el tamaño
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cerr << "[LOAD ERROR] No se pudo abrir el archivo: \""
                  << filename << "\"\n"
                  << "  Verifica que la ruta sea correcta y que tengas permisos de lectura.\n";
        return false;
    }

    const std::streamsize file_size = file.tellg();

    if (file_size <= 0) {
        std::cerr << "[LOAD ERROR] El archivo esta vacío o su tamaño no pudo determinarse.\n"
                  << "  Archivo: \"" << filename << "\"\n";
        return false;
    }

    if (static_cast<uint64_t>(file_size) > static_cast<uint64_t>(MEM_SIZE)) {
        std::cerr << "[LOAD ERROR] El archivo (" << file_size << " bytes) excede la memoria "
                  << "disponible (" << MEM_SIZE << " bytes = 1 MB).\n"
                  << "  Archivo: \"" << filename << "\"\n";
        return false;
    }

    // Volver al inicio y leer el contenido en memoria desde la direccion 0x0
    file.seekg(0, std::ios::beg);

    if (!file.read(reinterpret_cast<char*>(memory_.data()),
                   static_cast<std::streamsize>(file_size))) {
        std::cerr << "[LOAD ERROR] Fallo durante la lectura del archivo: \""
                  << filename << "\"\n";
        return false;
    }

    // Reporte de exito
    std::cout << "[LOAD OK] \"" << filename << "\"\n"
              << "  Tamaño   : " << std::dec << file_size << " bytes\n"
              << "  Rango    : 0x00000000 .. 0x"
              << std::hex << std::setw(8) << std::setfill('0') << (file_size - 1)
              << std::dec << "\n";

    return true;
}

// =============================================================================
// Ciclo de ejecucion (stub)
// =============================================================================

void Simulator::step()
{
    // [STUB] La siguiente fase implementara aquí el ciclo Fetch-Decode-Execute:
    //   1. FETCH   : instruction = read_word(pc_)
    //   2. DECODE  : extraer opcode, rd, rs1, rs2, funct3, funct7, imm
    //   3. EXECUTE : ejecutar la instruccion segun opcode
    //   4. Actualizar pc_ (pc_ += 4, o salto si es instruccion de rama)
    std::cout << "[STEP STUB] Ciclo Fetch-Decode-Execute no implementado aun.\n"
              << "  PC actual : 0x"
              << std::hex << std::setw(8) << std::setfill('0') << pc_
              << std::dec << "\n";
}

// =============================================================================
// Impresion de estado
// =============================================================================

void Simulator::print_state() const
{
    // Guardar configuracion original de cout y restaurar al final
    const auto fmt_flags = std::cout.flags();
    const auto fill_char = std::cout.fill();

    std::cout << "=============================================================\n"
              << "  SIMULADOR RISC-V RV32I — Estado Actual\n"
              << "=============================================================\n"
              << "  PC  : 0x"
              << std::hex << std::setw(8) << std::setfill('0') << pc_
              << "\n"
              << "-------------------------------------------------------------\n"
              << "  Registros de Proposito General:\n"
              << "-------------------------------------------------------------\n";

    for (int i = 0; i < 32; ++i) {
        // Formato: "  x<nn> (<abi_name>) : 0x<XXXXXXXX>"
        // El nombre ABI se justifica a 4 chars para alinear columnas
        std::cout << "  x"
                  << std::dec << std::setw(2) << std::setfill('0') << i
                  << " ("
                  << std::left << std::setw(4) << std::setfill(' ') << ABI_NAMES[i]
                  << ") : 0x"
                  << std::right
                  << std::hex << std::setw(8) << std::setfill('0') << regs_[i]
                  << "\n";
    }

    std::cout << "=============================================================\n";

    // Restaurar formato original
    std::cout.flags(fmt_flags);
    std::cout.fill(fill_char);
}
