// =============================================================================
// Simulator.cpp — Implementacion del Simulador RISC-V RV32I
// =============================================================================
// Implementa todos los metodos declarados en Simulator.hpp.
//
// Decisiones de diseno:
//   - Memoria little-endian construida byte a byte (portable, sin UB).
//   - Alineacion forzada: error explicito + excepcion en accesos no alineados.
//   - Regla de Oro: set_reg() ignora silenciosamente escrituras a x0.
//   - Sign-extension via cast a int32_t + corrimiento aritmetico derecho.
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
// Fetch
// =============================================================================

uint32_t Simulator::fetch()
{
    // read_word ya valida alineacion (pc_ debe ser multiplo de 4) y limites.
    return read_word(pc_);
}

// =============================================================================
// Helpers de decodificacion — extraccion de campos
// =============================================================================

// opcode : bits [6:0]
uint32_t Simulator::get_opcode(uint32_t instr) {
    return instr & 0x7Fu;
}

// rd : bits [11:7]
uint8_t Simulator::get_rd(uint32_t instr) {
    return static_cast<uint8_t>((instr >> 7) & 0x1Fu);
}

// funct3 : bits [14:12]
uint32_t Simulator::get_funct3(uint32_t instr) {
    return (instr >> 12) & 0x7u;
}

// rs1 : bits [19:15]
uint8_t Simulator::get_rs1(uint32_t instr) {
    return static_cast<uint8_t>((instr >> 15) & 0x1Fu);
}

// rs2 : bits [24:20]
uint8_t Simulator::get_rs2(uint32_t instr) {
    return static_cast<uint8_t>((instr >> 20) & 0x1Fu);
}

// funct7 : bits [31:25]
uint32_t Simulator::get_funct7(uint32_t instr) {
    return (instr >> 25) & 0x7Fu;
}

// =============================================================================
// Extractores de inmediatos con sign-extension
// =============================================================================
//
// Estrategia de sign-extension:
//   Se construye el inmediato como uint32_t con el bit de signo en la posicion
//   correcta, luego se castea a int32_t y se aplica corrimiento aritmetico
//   derecho para propagar el signo a todos los bits superiores.
//   El corrimiento aritmetico en int32_t negativo esta definido como
//   implementation-defined en C++, pero en todas las plataformas modernas
//   (x86, ARM, RISC-V) es aritmetico (replica el bit de signo). C++20 lo
//   estandariza formalmente; en C++17 es seguro en la practica.

// Formato I: imm[11:0] = instr[31:20]
// El bit de signo es instr[31] = imm[11].
int32_t Simulator::extract_imm_I(uint32_t instr)
{
    // Desplazamos el campo a la posicion mas alta y luego aritmeticamente
    // a la derecha para que el signo se propague.
    //   instr[31:20] -> colocar en [31:20] via << 0 ya esta; necesitamos
    //   llevar imm[11] a bit31 del int32_t.
    return static_cast<int32_t>(instr) >> 20;  // SAR 20: imm[11] -> bit31
}

// Formato S: imm[11:5] = instr[31:25], imm[4:0] = instr[11:7]
// Bit de signo: instr[31] = imm[11].
int32_t Simulator::extract_imm_S(uint32_t instr)
{
    // Reconstruir el inmediato de 12 bits en un uint32_t:
    //   bits [11:5] provienen de instr[31:25]
    //   bits [4:0]  provienen de instr[11:7]
    const uint32_t imm_upper = (instr >> 25) & 0x7Fu;  // 7 bits -> posicion [6:0]
    const uint32_t imm_lower = (instr >>  7) & 0x1Fu;  // 5 bits -> posicion [4:0]

    // Ensamblar en un uint32_t con el campo en bits [11:0]
    const uint32_t raw = (imm_upper << 5) | imm_lower;

    // Sign-extend desde bit 11: desplazar a bit31 y volver con SAR
    return static_cast<int32_t>(raw << 20) >> 20;
}

// Formato B: bits dispersos, offset de 2 bytes (imm[0] = 0 implicito)
//   imm[12]   = instr[31]
//   imm[10:5] = instr[30:25]
//   imm[4:1]  = instr[11:8]
//   imm[11]   = instr[7]
// Bit de signo: instr[31] = imm[12].
int32_t Simulator::extract_imm_B(uint32_t instr)
{
    const uint32_t bit12  = (instr >> 31) & 0x1u;   // imm[12]
    const uint32_t bit11  = (instr >>  7) & 0x1u;   // imm[11]
    const uint32_t bits10_5 = (instr >> 25) & 0x3Fu; // imm[10:5]
    const uint32_t bits4_1  = (instr >>  8) & 0xFu;  // imm[4:1]

    // Ensamblar: imm[0]=0 siempre (offset a instrucciones de 2 bytes)
    const uint32_t raw = (bit12   << 12)
                       | (bit11   << 11)
                       | (bits10_5 << 5)
                       | (bits4_1  << 1);

    // Sign-extend desde bit 12
    return static_cast<int32_t>(raw << 19) >> 19;
}

// Formato U: los 20 bits superiores son el inmediato, bits [11:0] = 0.
// Se usa directamente como valor de 32 bits; no necesita sign-extension
// adicional porque ya ocupa los bits [31:12].
int32_t Simulator::extract_imm_U(uint32_t instr)
{
    // Limpiar los 12 bits inferiores (opcode + rd)
    return static_cast<int32_t>(instr & 0xFFFFF000u);
}

// Formato J: bits dispersos para JAL
//   imm[20]    = instr[31]
//   imm[10:1]  = instr[30:21]
//   imm[11]    = instr[20]
//   imm[19:12] = instr[19:12]
//   imm[0]     = 0 (siempre)
// Bit de signo: instr[31] = imm[20].
int32_t Simulator::extract_imm_J(uint32_t instr)
{
    const uint32_t bit20    = (instr >> 31) & 0x1u;   // imm[20]
    const uint32_t bits19_12 = (instr >> 12) & 0xFFu; // imm[19:12]
    const uint32_t bit11    = (instr >> 20) & 0x1u;   // imm[11]
    const uint32_t bits10_1 = (instr >> 21) & 0x3FFu; // imm[10:1]

    // Ensamblar: imm[0]=0 siempre
    const uint32_t raw = (bit20     << 20)
                       | (bits19_12 << 12)
                       | (bit11     << 11)
                       | (bits10_1  <<  1);

    // Sign-extend desde bit 20
    return static_cast<int32_t>(raw << 11) >> 11;
}

// =============================================================================
// Ciclo de ejecucion — Fetch + Decode
// =============================================================================

void Simulator::step()
{
    // --- 1. FETCH: leer la instruccion de 32 bits en pc_ ---
    const uint32_t instr = fetch();

    // --- 2. Avanzar el PC a la siguiente instruccion ---
    // En instrucciones de salto/branch, el Execute (Fase 3) sobreescribira pc_.
    pc_ += 4;

    // --- 3. DECODE: extraer opcode (bits [6:0]) ---
    const uint32_t opcode = get_opcode(instr);

    // Campos adicionales disponibles para el Execute (Fase 3).
    // Se extraen aqui para que cada case los tenga listos.
    const uint8_t  rd     = get_rd(instr);
    const uint8_t  rs1    = get_rs1(instr);
    const uint8_t  rs2    = get_rs2(instr);
    const uint32_t funct3 = get_funct3(instr);
    const uint32_t funct7 = get_funct7(instr);

    // --- 4. DISPATCH: switch por familia de instrucciones ---
    switch (opcode) {

        // -----------------------------------------------------------------
        // Tipo U — Load Upper Immediate
        // LUI rd, imm  ->  rd = imm << 12
        // -----------------------------------------------------------------
        case OP_LUI: {
            // TODO (Fase 3): set_reg(rd, static_cast<uint32_t>(extract_imm_U(instr)));
            (void)rd;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo U — Add Upper Immediate to PC
        // AUIPC rd, imm  ->  rd = pc_ + (imm << 12)
        // -----------------------------------------------------------------
        case OP_AUIPC: {
            // TODO (Fase 3): set_reg(rd, (pc_ - 4) + static_cast<uint32_t>(extract_imm_U(instr)));
            (void)rd;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo J — Jump And Link
        // JAL rd, imm  ->  rd = pc_ ; pc_ = (pc_-4) + imm
        // -----------------------------------------------------------------
        case OP_JAL: {
            // TODO (Fase 3): implementar salto incondicional
            (void)rd;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo I — Jump And Link Register
        // JALR rd, rs1, imm  ->  rd = pc_ ; pc_ = (rs1 + imm) & ~1
        // -----------------------------------------------------------------
        case OP_JALR: {
            // TODO (Fase 3): implementar salto indirecto
            (void)rd; (void)rs1; (void)funct3;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo B — Branches condicionales
        // BEQ | BNE | BLT | BGE | BLTU | BGEU
        // Diferenciados por funct3.
        // -----------------------------------------------------------------
        case OP_BRANCH: {
            // TODO (Fase 3): evaluar condicion y actualizar pc_ si se cumple
            (void)rs1; (void)rs2; (void)funct3;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo I — Loads
        // LB | LH | LW | LBU | LHU
        // Diferenciados por funct3.
        // -----------------------------------------------------------------
        case OP_LOAD: {
            // TODO (Fase 3): leer de memoria y escribir en rd
            (void)rd; (void)rs1; (void)funct3;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo S — Stores
        // SB | SH | SW
        // Diferenciados por funct3.
        // -----------------------------------------------------------------
        case OP_STORE: {
            // TODO (Fase 3): escribir rs2 en memoria[rs1 + imm_S]
            (void)rs1; (void)rs2; (void)funct3;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo I — ALU con inmediato
        // ADDI | SLTI | SLTIU | XORI | ORI | ANDI | SLLI | SRLI | SRAI
        // Diferenciados por funct3 (y funct7 para shifts).
        // -----------------------------------------------------------------
        case OP_ALUI: {
            // TODO (Fase 3): ejecutar operacion ALU con inmediato I
            (void)rd; (void)rs1; (void)funct3; (void)funct7;
            break;
        }

        // -----------------------------------------------------------------
        // Tipo R — ALU registro a registro
        // ADD | SUB | SLL | SLT | SLTU | XOR | SRL | SRA | OR | AND
        // Diferenciados por funct3 + funct7.
        // -----------------------------------------------------------------
        case OP_ALU: {
            // TODO (Fase 3): ejecutar operacion ALU entre registros
            (void)rd; (void)rs1; (void)rs2; (void)funct3; (void)funct7;
            break;
        }

        // -----------------------------------------------------------------
        // FENCE — barrera de ordenacion de memoria
        // En un simulador de ejecucion en orden se puede tratar como NOP.
        // -----------------------------------------------------------------
        case OP_FENCE: {
            // TODO (Fase 3): NOP o modelar barrera si se necesita
            break;
        }

        // -----------------------------------------------------------------
        // SYSTEM — ECALL, EBREAK, instrucciones CSR
        // Diferenciados por funct3 y el campo imm[11:0].
        // -----------------------------------------------------------------
        case OP_SYSTEM: {
            // TODO (Fase 3): manejar llamadas al sistema y CSR
            (void)rd; (void)rs1; (void)funct3;
            break;
        }

        // -----------------------------------------------------------------
        // Opcode desconocido o instruccion ilegal
        // -----------------------------------------------------------------
        default: {
            std::ostringstream oss;
            oss << "\n[ILLEGAL INSTRUCTION] opcode desconocido\n"
                << "  PC (antes del fetch) : 0x"
                << std::hex << std::setw(8) << std::setfill('0') << (pc_ - 4) << "\n"
                << "  Instruccion          : 0x"
                << std::setw(8) << std::setfill('0') << instr << "\n"
                << "  Opcode               : 0x"
                << std::setw(2) << std::setfill('0') << opcode << "\n"
                << "  → La simulacion se detiene.";
            std::cerr << oss.str() << "\n";
            throw std::runtime_error(oss.str());
        }
    }
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
