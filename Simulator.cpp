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

#include <algorithm>    // fill
#include <fstream>      // ifstream
#include <iomanip>      // hex, setw, setfill
#include <iostream>     // cout, cerr
#include <sstream>      // ostringstream
#include <stdexcept>    // runtime_error, out_of_range

// =============================================================================
// Constructor
// =============================================================================

Simulator::Simulator()
    : pc_(0x00000000u),
      memory_(MEM_SIZE, 0u)   // Inicializar toda la memoria a 0
{
    // Inicializar los 32 registros a 0 (x0 incluido — nunca cambiara)
    fill(begin(regs_), end(regs_), 0u);
}

// =============================================================================
// Helper: check_access — Alineacion y límites
// =============================================================================

void Simulator::check_access(uint32_t address, uint8_t size, const char* operation) const
{
    // --- Verificacion de alineacion (solo aplica para size > 1) ---
    if (size > 1 && (address % size) != 0) {
        ostringstream oss;
        oss << "\n[ALIGNMENT FAULT] " << operation << "()\n"
            << "  Direccion : 0x" << hex << setw(8)
                                  << setfill('0') << address << "\n"
            << "  Ancho     : " << dec << static_cast<int>(size) << " bytes\n"
            << "  Requerido : direccion multiplo de " << static_cast<int>(size)
            << " (address % " << static_cast<int>(size) << " == 0)\n"
            << "  Obtenido  : 0x" << hex << address
            << " % " << dec << static_cast<int>(size)
            << " == " << (address % size) << "\n"
            << " La simulacion se detiene.";
        cerr << oss.str() << "\n";
        throw runtime_error(oss.str());
    }

    // --- Verificacion de límites de memoria ---
    // Se usa uint64_t para evitar overflow al sumar address + size
    if (static_cast<uint64_t>(address) + size > static_cast<uint64_t>(MEM_SIZE)) {
        ostringstream oss;
        oss << "\n[OUT OF BOUNDS] " << operation << "()\n"
            << "  Direccion : 0x" << hex << setw(8)
                                  << setfill('0') << address << "\n"
            << "  Ancho     : " << dec << static_cast<int>(size) << " bytes\n"
            << "  Límite    : 0x" << hex << MEM_SIZE
            << " (" << dec << MEM_SIZE << " bytes = 1 MB)\n"
            << "  → La simulacion se detiene.";
        cerr << oss.str() << "\n";
        throw runtime_error(oss.str());
    }
}

// =============================================================================
// Acceso a registros
// =============================================================================

uint32_t Simulator::get_reg(uint8_t idx) const
{
    if (idx >= 32) {
        throw out_of_range(
            "[REGISTER ERROR] get_reg(): índice fuera de rango: "
            + to_string(idx) + " (maximo: 31)");
    }
    return regs_[idx];
}

void Simulator::set_reg(uint8_t idx, uint32_t val)
{
    if (idx >= 32) {
        throw out_of_range(
            "[REGISTER ERROR] set_reg(): índice fuera de rango: "
            + to_string(idx) + " (maximo: 31)");
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

bool Simulator::load_from_file(const string& filename)
{
    // Abrir en modo binario y posicionar el cursor al final para medir el tamaño
    ifstream file(filename, ios::binary | ios::ate);

    if (!file.is_open()) {
        cerr << "[LOAD ERROR] No se pudo abrir el archivo: \""
                  << filename << "\"\n"
                  << "  Verifica que la ruta sea correcta y que tengas permisos de lectura.\n";
        return false;
    }

    const streamsize file_size = file.tellg();

    if (file_size <= 0) {
        cerr << "[LOAD ERROR] El archivo esta vacío o su tamaño no pudo determinarse.\n"
                  << "  Archivo: \"" << filename << "\"\n";
        return false;
    }

    if (static_cast<uint64_t>(file_size) > static_cast<uint64_t>(MEM_SIZE)) {
        cerr << "[LOAD ERROR] El archivo (" << file_size << " bytes) excede la memoria "
                  << "disponible (" << MEM_SIZE << " bytes = 1 MB).\n"
                  << "  Archivo: \"" << filename << "\"\n";
        return false;
    }

    // Volver al inicio y leer el contenido en memoria desde la direccion 0x0
    file.seekg(0, ios::beg);

    if (!file.read(reinterpret_cast<char*>(memory_.data()),
                   static_cast<streamsize>(file_size))) {
        cerr << "[LOAD ERROR] Fallo durante la lectura del archivo: \""
                  << filename << "\"\n";
        return false;
    }

    // Reporte de exito
    cout << "[LOAD OK] \"" << filename << "\"\n"
              << "  Tamanho   : " << dec << file_size << " bytes\n"
              << "  Rango    : 0x00000000 .. 0x"
              << hex << setw(8) << setfill('0') << (file_size - 1)
              << dec << "\n";

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
        // LUI — Load Upper Immediate
        // rd = imm_U  (imm ya tiene ceros en bits [11:0])
        // -----------------------------------------------------------------
        case OP_LUI: {
            const int32_t imm = extract_imm_U(instr);
            set_reg(rd, static_cast<uint32_t>(imm));
            break;
        }

        // -----------------------------------------------------------------
        // AUIPC — Add Upper Immediate to PC
        // rd = (PC de esta instruccion) + imm_U
        // -----------------------------------------------------------------
        case OP_AUIPC: {
            const int32_t  imm     = extract_imm_U(instr);
            const uint32_t this_pc = pc_ - 4u;
            set_reg(rd, this_pc + static_cast<uint32_t>(imm));
            break;
        }

        // -----------------------------------------------------------------
        // JAL — Jump And Link
        // rd  = PC+4 ; PC = (PC de esta instruccion) + imm_J
        // -----------------------------------------------------------------
        case OP_JAL: {
            const int32_t  imm      = extract_imm_J(instr);
            const uint32_t ret_addr = pc_;         // pc_ ya es PC+4
            const uint32_t this_pc  = pc_ - 4u;
            set_reg(rd, ret_addr);
            pc_ = this_pc + static_cast<uint32_t>(imm);
            break;
        }

        // -----------------------------------------------------------------
        // JALR — Jump And Link Register
        // rd  = PC+4 ; PC = (rs1 + imm_I) & ~1
        // -----------------------------------------------------------------
        case OP_JALR: {
            const int32_t  imm      = extract_imm_I(instr);
            const uint32_t ret_addr = pc_;         // pc_ ya es PC+4
            const uint32_t target   = (regs_[rs1] + static_cast<uint32_t>(imm)) & ~1u;
            set_reg(rd, ret_addr);
            pc_ = target;
            break;
        }

        // -----------------------------------------------------------------
        // Branches — BEQ, BNE, BLT, BGE, BLTU, BGEU
        // Si condicion se cumple: PC = (PC de esta instruccion) + imm_B
        // -----------------------------------------------------------------
        case OP_BRANCH: {
            const int32_t  imm     = extract_imm_B(instr);
            const uint32_t this_pc = pc_ - 4u;
            const int32_t  vs1 = static_cast<int32_t>(regs_[rs1]);
            const int32_t  vs2 = static_cast<int32_t>(regs_[rs2]);
            const uint32_t vu1 = regs_[rs1];
            const uint32_t vu2 = regs_[rs2];

            bool taken = false;
            switch (funct3) {
                case 0x0: taken = (vu1 == vu2); break;  // BEQ
                case 0x1: taken = (vu1 != vu2); break;  // BNE
                case 0x4: taken = (vs1 <  vs2); break;  // BLT  (signed)
                case 0x5: taken = (vs1 >= vs2); break;  // BGE  (signed)
                case 0x6: taken = (vu1 <  vu2); break;  // BLTU (unsigned)
                case 0x7: taken = (vu1 >= vu2); break;  // BGEU (unsigned)
                default: {
                    ostringstream oss;
                    oss << "\n[ILLEGAL INSTRUCTION] BRANCH funct3 desconocido\n"
                        << "  PC     : 0x" << hex << setw(8)
                        << setfill('0') << this_pc << "\n"
                        << "  funct3 : 0x" << funct3 << "\n"
                        << "  -> La simulacion se detiene.";
                    cerr << oss.str() << "\n";
                    throw runtime_error(oss.str());
                }
            }
            if (taken) {
                pc_ = this_pc + static_cast<uint32_t>(imm);
            }
            break;
        }

        // -----------------------------------------------------------------
        // Loads — LB, LH, LW, LBU, LHU
        // addr = rs1 + imm_I ;  LB/LH aplican sign-extension
        // -----------------------------------------------------------------
        case OP_LOAD: {
            const int32_t  imm  = extract_imm_I(instr);
            const uint32_t addr = regs_[rs1] + static_cast<uint32_t>(imm);
            uint32_t result = 0u;
            switch (funct3) {
                case 0x0: {  // LB — sign-extended byte
                    result = static_cast<uint32_t>(
                                 static_cast<int32_t>(
                                     static_cast<int8_t>(read_byte(addr))));
                    break;
                }
                case 0x1: {  // LH — sign-extended halfword
                    result = static_cast<uint32_t>(
                                 static_cast<int32_t>(
                                     static_cast<int16_t>(read_halfword(addr))));
                    break;
                }
                case 0x2:    // LW
                    result = read_word(addr);
                    break;
                case 0x4:    // LBU — zero-extended byte
                    result = static_cast<uint32_t>(read_byte(addr));
                    break;
                case 0x5:    // LHU — zero-extended halfword
                    result = static_cast<uint32_t>(read_halfword(addr));
                    break;
                default: {
                    ostringstream oss;
                    oss << "\n[ILLEGAL INSTRUCTION] LOAD funct3 desconocido\n"
                        << "  PC     : 0x" << hex << setw(8)
                        << setfill('0') << (pc_ - 4u) << "\n"
                        << "  funct3 : 0x" << funct3 << "\n"
                        << "  -> La simulacion se detiene.";
                    cerr << oss.str() << "\n";
                    throw runtime_error(oss.str());
                }
            }
            set_reg(rd, result);
            break;
        }

        // -----------------------------------------------------------------
        // Stores — SB, SH, SW
        // addr = rs1 + imm_S ; mem[addr] = rs2[ancho]
        // -----------------------------------------------------------------
        case OP_STORE: {
            const int32_t  imm  = extract_imm_S(instr);
            const uint32_t addr = regs_[rs1] + static_cast<uint32_t>(imm);
            const uint32_t src  = regs_[rs2];
            switch (funct3) {
                case 0x0:
                    write_byte(addr, static_cast<uint8_t>(src & 0xFFu));
                    break;
                case 0x1:
                    write_halfword(addr, static_cast<uint16_t>(src & 0xFFFFu));
                    break;
                case 0x2:
                    write_word(addr, src);
                    break;
                default: {
                    ostringstream oss;
                    oss << "\n[ILLEGAL INSTRUCTION] STORE funct3 desconocido\n"
                        << "  PC     : 0x" << hex << setw(8)
                        << setfill('0') << (pc_ - 4u) << "\n"
                        << "  funct3 : 0x" << funct3 << "\n"
                        << "  -> La simulacion se detiene.";
                    cerr << oss.str() << "\n";
                    throw runtime_error(oss.str());
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // ALU con Inmediato — ADDI, SLTI, SLTIU, XORI, ORI, ANDI,
        //                      SLLI, SRLI, SRAI
        // -----------------------------------------------------------------
        case OP_ALUI: {
            const int32_t  imm = extract_imm_I(instr);
            const int32_t  vs1 = static_cast<int32_t>(regs_[rs1]);
            const uint32_t vu1 = regs_[rs1];
            uint32_t result = 0u;
            switch (funct3) {
                case 0x0:   // ADDI
                    result = static_cast<uint32_t>(vs1 + imm);
                    break;
                case 0x2:   // SLTI — signed
                    result = (vs1 < imm) ? 1u : 0u;
                    break;
                case 0x3:   // SLTIU — unsigned (inmediato sign-extended, comp. unsigned)
                    result = (vu1 < static_cast<uint32_t>(imm)) ? 1u : 0u;
                    break;
                case 0x4:   // XORI
                    result = vu1 ^ static_cast<uint32_t>(imm);
                    break;
                case 0x6:   // ORI
                    result = vu1 | static_cast<uint32_t>(imm);
                    break;
                case 0x7:   // ANDI
                    result = vu1 & static_cast<uint32_t>(imm);
                    break;
                case 0x1:   // SLLI
                    result = vu1 << (static_cast<uint32_t>(imm) & 0x1Fu);
                    break;
                case 0x5: { // SRLI (funct7[5]=0) / SRAI (funct7[5]=1)
                    const uint32_t shamt = static_cast<uint32_t>(imm) & 0x1Fu;
                    result = (funct7 & 0x20u)
                             ? static_cast<uint32_t>(vs1 >> shamt)   // SRAI
                             : vu1 >> shamt;                          // SRLI
                    break;
                }
                default: {
                    ostringstream oss;
                    oss << "\n[ILLEGAL INSTRUCTION] ALU-I funct3 desconocido\n"
                        << "  PC     : 0x" << hex << setw(8)
                        << setfill('0') << (pc_ - 4u) << "\n"
                        << "  funct3 : 0x" << funct3 << "\n"
                        << "  -> La simulacion se detiene.";
                    cerr << oss.str() << "\n";
                    throw runtime_error(oss.str());
                }
            }
            set_reg(rd, result);
            break;
        }

        // -----------------------------------------------------------------
        // ALU Registro-Registro — ADD, SUB, SLL, SLT, SLTU, XOR,
        //                          SRL, SRA, OR, AND
        // Diferenciados por funct3 + bit 30 de funct7 (flag 'alt').
        // -----------------------------------------------------------------
        case OP_ALU: {
            const int32_t  vs1 = static_cast<int32_t>(regs_[rs1]);
            const int32_t  vs2 = static_cast<int32_t>(regs_[rs2]);
            const uint32_t vu1 = regs_[rs1];
            const uint32_t vu2 = regs_[rs2];
            const bool     alt = (funct7 & 0x20u) != 0u;  // bit30: SUB, SRA

            uint32_t result = 0u;
            switch (funct3) {
                case 0x0:   // ADD (alt=0) / SUB (alt=1)
                    result = alt
                             ? static_cast<uint32_t>(vs1 - vs2)
                             : static_cast<uint32_t>(vs1 + vs2);
                    break;
                case 0x1:   // SLL — Shift Left Logical
                    result = vu1 << (vu2 & 0x1Fu);
                    break;
                case 0x2:   // SLT — signed less-than
                    result = (vs1 < vs2) ? 1u : 0u;
                    break;
                case 0x3:   // SLTU — unsigned less-than
                    result = (vu1 < vu2) ? 1u : 0u;
                    break;
                case 0x4:   // XOR
                    result = vu1 ^ vu2;
                    break;
                case 0x5:   // SRL (alt=0) / SRA (alt=1)
                    result = alt
                             ? static_cast<uint32_t>(vs1 >> (vu2 & 0x1Fu))
                             : vu1 >> (vu2 & 0x1Fu);
                    break;
                case 0x6:   // OR
                    result = vu1 | vu2;
                    break;
                case 0x7:   // AND
                    result = vu1 & vu2;
                    break;
                default: {
                    ostringstream oss;
                    oss << "\n[ILLEGAL INSTRUCTION] ALU-R funct3 desconocido\n"
                        << "  PC     : 0x" << hex << setw(8)
                        << setfill('0') << (pc_ - 4u) << "\n"
                        << "  funct3 : 0x" << funct3 << "\n"
                        << "  -> La simulacion se detiene.";
                    cerr << oss.str() << "\n";
                    throw runtime_error(oss.str());
                }
            }
            set_reg(rd, result);
            break;
        }

        // -----------------------------------------------------------------
        // FENCE — barrera de ordenacion de memoria
        // En un simulador de ejecucion en orden (in-order, single-thread)
        // se trata como NOP; no hay reordenamiento que serializar.
        // -----------------------------------------------------------------
        case OP_FENCE:
            break;

        // -----------------------------------------------------------------
        // SYSTEM — ECALL, EBREAK
        // Fase 3: tratados como NOP (sin soporte de sistema operativo).
        // Se pueden extender para emular syscalls en fases posteriores.
        // -----------------------------------------------------------------
        case OP_SYSTEM:
            break;

        // -----------------------------------------------------------------
        // Opcode desconocido o instruccion ilegal
        // -----------------------------------------------------------------
        default: {
            ostringstream oss;
            oss << "\n[ILLEGAL INSTRUCTION] opcode desconocido\n"
                << "  PC (antes del fetch) : 0x"
                << hex << setw(8) << setfill('0') << (pc_ - 4) << "\n"
                << "  Instruccion          : 0x"
                << setw(8) << setfill('0') << instr << "\n"
                << "  Opcode               : 0x"
                << setw(2) << setfill('0') << opcode << "\n"
                << "  -> La simulacion se detiene.";
            cerr << oss.str() << "\n";
            throw runtime_error(oss.str());
        }
    }
}

// =============================================================================
// Impresion de estado
// =============================================================================

void Simulator::print_state() const
{
    // Guardar configuracion original de cout y restaurar al final
    const auto fmt_flags = cout.flags();
    const auto fill_char = cout.fill();

    cout << "=============================================================\n"
              << "  SIMULADOR RISC-V RV32I - Estado Actual\n"
              << "=============================================================\n"
              << "  PC  : 0x"
              << hex << setw(8) << setfill('0') << pc_
              << "\n"
              << "-------------------------------------------------------------\n"
              << "  Registros de Proposito General:\n"
              << "-------------------------------------------------------------\n";

    for (int i = 0; i < 32; ++i) {
        // Formato: "  x<nn> (<abi_name>) : 0x<XXXXXXXX>"
        // El nombre ABI se justifica a 4 chars para alinear columnas
        cout << "  x"
                  << dec << setw(2) << setfill('0') << i
                  << " ("
                  << left << setw(4) << setfill(' ') << ABI_NAMES[i]
                  << ") : 0x"
                  << right
                  << hex << setw(8) << setfill('0') << regs_[i]
                  << "\n";
    }

    cout << "=============================================================\n";

    // Restaurar formato original
    cout.flags(fmt_flags);
    cout.fill(fill_char);
}
