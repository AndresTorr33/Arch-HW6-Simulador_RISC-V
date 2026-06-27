#pragma once

// Simulator.hpp — Simulador RISC-V RV32I (32-bit)

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace std;

/**
 * @class Simulator
 * @brief Nucleo del simulador de CPU RISC-V RV32I.
 */
class Simulator {
public:


    /// Tamaño del bloque de memoria principal: 1 MB (1,048,576 bytes).
    static constexpr uint32_t MEM_SIZE = 1024u * 1024u;

    /// Nombres ABI de los 32 registros de proposito general.
    static constexpr array<const char*, 32> ABI_NAMES = {
        "zero", "ra",  "sp",  "gp",  "tp",  "t0",  "t1",  "t2",
        "s0",   "s1",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
        "a6",   "a7",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
        "s8",   "s9",  "s10", "s11", "t3",  "t4",  "t5",  "t6"
    };



    static constexpr uint32_t OP_LUI    = 0x37; ///< Load Upper Immediate
    static constexpr uint32_t OP_AUIPC  = 0x17; ///< Add Upper Immediate to PC
    static constexpr uint32_t OP_JAL    = 0x6F; ///< Jump And Link
    static constexpr uint32_t OP_JALR   = 0x67; ///< Jump And Link Register
    static constexpr uint32_t OP_BRANCH = 0x63; ///< Branches (BEQ, BNE, BLT, BGE, BLTU, BGEU)
    static constexpr uint32_t OP_LOAD   = 0x03; ///< Loads  (LB, LH, LW, LBU, LHU)
    static constexpr uint32_t OP_STORE  = 0x23; ///< Stores (SB, SH, SW)
    static constexpr uint32_t OP_ALUI   = 0x13; ///< ALU Inmediata (ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI)
    static constexpr uint32_t OP_ALU    = 0x33; ///< ALU Registro  (ADD, SUB, SLL, SLT, XOR, SRL, SRA, OR, AND)
    static constexpr uint32_t OP_FENCE  = 0x0F; ///< FENCE (barrera de memoria)
    static constexpr uint32_t OP_SYSTEM = 0x73; ///< ECALL / EBREAK / CSR

    /// Construye el simulador con estado inicial limpio (PC=0, mem=0, regs=0)
    Simulator();

    /// Carga un archivo binario crudo en memoria desde la direccion 0x0.
    bool load_from_file(const string& filename);

    /// Lee la instruccion de 32 bits en pc_ y reconstruye a little-endian
    uint32_t fetch();

    /// Ejecuta un ciclo Fetch-Decode-Execute
    void step();

    /// Imprime en stdout el estado completo (PC, registros)
    void print_state() const;

    /// Lee el valor del registro x{idx}
    uint32_t get_reg(uint8_t idx) const;

    /// Escribe val en el registro x{idx}. Escrituras a x0 son ignoradas.
    void set_reg(uint8_t idx, uint32_t val);

    /// Lee 1 byte
    uint8_t read_byte(uint32_t address) const;

    /// Lee 2 bytes (halfword) en formato little-endian. Requiere address % 2 == 0.
    uint16_t read_halfword(uint32_t address) const;

    /// Lee 4 bytes (word) en formato little-endian. Requiere address % 4 == 0.
    uint32_t read_word(uint32_t address) const;

    /// Escribe 1 byte
    void write_byte(uint32_t address, uint8_t val);

    /// Escribe 2 bytes (halfword) en formato little-endian. Requiere address % 2 == 0.
    void write_halfword(uint32_t address, uint16_t val);

    /// Escribe 4 bytes (word) en formato little-endian. Requiere address % 4 == 0.
    void write_word(uint32_t address, uint32_t val);

    // Decode utilities estaticos

    static uint32_t get_opcode(uint32_t instr); ///< Extrae opcode (bits [6:0])
    static uint8_t get_rd(uint32_t instr);      ///< Extrae rd (bits [11:7])
    static uint32_t get_funct3(uint32_t instr); ///< Extrae funct3 (bits [14:12])
    static uint8_t get_rs1(uint32_t instr);     ///< Extrae rs1 (bits [19:15])
    static uint8_t get_rs2(uint32_t instr);     ///< Extrae rs2 (bits [24:20])
    static uint32_t get_funct7(uint32_t instr); ///< Extrae funct7 (bits [31:25])

    static int32_t extract_imm_I(uint32_t instr);
    static int32_t extract_imm_S(uint32_t instr);
    static int32_t extract_imm_B(uint32_t instr);
    static int32_t extract_imm_U(uint32_t instr);
    static int32_t extract_imm_J(uint32_t instr);
    
    uint32_t get_pc() const { return pc_; }

private:

    /// Valida alineacion y limites antes del acceso a memoria
    void check_access(uint32_t address, uint8_t size, const char* operation) const;

    uint32_t             regs_[32]; ///< Registros de proposito general x0–x31.
    uint32_t             pc_;       ///< Program Counter.
    vector<uint8_t> memory_;   ///< Memoria principal (MEM_SIZE bytes, byte-addressable).
};
