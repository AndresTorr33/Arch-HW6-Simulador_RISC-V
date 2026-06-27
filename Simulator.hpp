#pragma once

// =============================================================================
// Simulator.hpp — Simulador RISC-V RV32I (32-bit)
// =============================================================================
// Declara la clase Simulator que encapsula el estado arquitectural completo:
//   - 32 registros de proposito general (x0–x31)
//   - Program Counter (PC)
//   - Memoria principal plana de 1 MB (byte-addressable, little-endian)
//
// Estandar: C++17
// =============================================================================

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// Clase principal
// =============================================================================

/**
 * @class Simulator
 * @brief Nucleo del simulador de CPU RISC-V RV32I.
 *
 * Proporciona:
 *   - Estado arquitectural (registros + PC)
 *   - Memoria flat de 1 MB con accesos little-endian y alineacion forzada
 *   - Cargador de archivos binarios crudos (.bin)
 *   - Ciclo Fetch + estructura de Decode (Fase 2)
 *   - Ciclo Execute pendiente (Fase 3)
 */
class Simulator {
public:
    // =========================================================================
    // Constantes publicas
    // =========================================================================

    /// Tamaño del bloque de memoria principal: 1 MB (1,048,576 bytes).
    static constexpr uint32_t MEM_SIZE = 1024u * 1024u;

    /**
     * @brief Nombres ABI de los 32 registros de proposito general.
     *
     * Indice i corresponde al registro xi:
     *   x0=zero  x1=ra   x2=sp   x3=gp   x4=tp
     *   x5=t0    x6=t1   x7=t2   x8=s0   x9=s1
     *   x10=a0   x11=a1  ...     x17=a7
     *   x18=s2   ...     x27=s11
     *   x28=t3   x29=t4  x30=t5  x31=t6
     */
    static constexpr std::array<const char*, 32> ABI_NAMES = {
        "zero", "ra",  "sp",  "gp",  "tp",  "t0",  "t1",  "t2",
        "s0",   "s1",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
        "a6",   "a7",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
        "s8",   "s9",  "s10", "s11", "t3",  "t4",  "t5",  "t6"
    };

    // =========================================================================
    // Opcodes base RV32I (bits [6:0] de la instruccion)
    // =========================================================================

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

    // =========================================================================
    // Constructor
    // =========================================================================

    /**
     * @brief Construye el simulador con estado inicial limpio.
     *
     * Postcondiciones:
     *   - Todos los registros x0–x31 = 0x00000000
     *   - PC = 0x00000000
     *   - Memoria = MEM_SIZE bytes, todos en cero
     */
    Simulator();

    // =========================================================================
    // Carga de programa
    // =========================================================================

    /**
     * @brief Carga un archivo binario crudo en memoria desde la direccion 0x0.
     *
     * Lee el archivo en modo binario y copia su contenido byte a byte en
     * memory_[0..file_size-1]. El PC no se modifica.
     *
     * @param filename  Ruta al archivo .bin a cargar.
     * @return          true si la carga fue exitosa, false en caso contrario.
     *                  En caso de error se imprime un mensaje descriptivo en stderr.
     */
    bool load_from_file(const std::string& filename);

    // =========================================================================
    // Ciclo de ejecucion
    // =========================================================================

    /**
     * @brief Lee la instruccion de 32 bits desde la memoria en la direccion pc_.
     *
     * Utiliza read_word(pc_) internamente. No modifica el PC.
     *
     * @return Instruccion de 32 bits en formato little-endian reconstruido.
     * @throws std::runtime_error si pc_ esta fuera de rango o no alineado.
     */
    uint32_t fetch();

    /**
     * @brief Ejecuta un ciclo Fetch-Decode-Execute.
     *
     * Secuencia:
     *   1. fetch()     — lee instruccion en pc_
     *   2. pc_ += 4   — avanza al siguiente word
     *   3. Extrae opcode
     *   4. switch(opcode) — despacha a la familia de instrucciones
     *
     * Las familias individuales (Execute) se implementan en la Fase 3.
     * Un opcode desconocido lanza std::runtime_error.
     */
    void step();

    /**
     * @brief Imprime en stdout el estado completo del simulador.
     *
     * Formato de cada registro:
     *   x<nn> (<abi>) : 0x<XXXXXXXX>
     *
     * Ejemplo:
     *   x10 (a0  ) : 0x0000002A
     */
    void print_state() const;

    // =========================================================================
    // Acceso a registros
    // =========================================================================

    /**
     * @brief Lee el valor del registro x{idx}.
     * @param idx  Indice del registro (0–31).
     * @return     Valor de 32 bits almacenado en el registro.
     * @throws     std::out_of_range si idx >= 32.
     */
    uint32_t get_reg(uint8_t idx) const;

    /**
     * @brief Escribe val en el registro x{idx}.
     *
     * Regla de Oro RISC-V: si idx == 0, la escritura se ignora silenciosamente
     * y x0 permanece en 0x00000000.
     *
     * @param idx  Indice del registro destino (0–31).
     * @param val  Valor de 32 bits a escribir.
     * @throws     std::out_of_range si idx >= 32.
     */
    void set_reg(uint8_t idx, uint32_t val);

    // =========================================================================
    // Lectura de memoria — little-endian estricto
    // =========================================================================

    /**
     * @brief Lee 1 byte desde la direccion dada.
     * @param address  Direccion de memoria (sin restriccion de alineacion).
     * @return         Byte leído.
     * @throws         std::runtime_error si address >= MEM_SIZE.
     */
    uint8_t read_byte(uint32_t address) const;

    /**
     * @brief Lee 2 bytes (halfword) en formato little-endian.
     *
     * Requiere alineacion estricta: address % 2 == 0.
     * En caso de acceso no alineado, se imprime un mensaje de error y se
     * lanza std::runtime_error deteniendo la simulacion.
     *
     * @param address  Direccion base (debe ser multiplo de 2).
     * @return         Halfword de 16 bits reconstruido en orden del host.
     * @throws         std::runtime_error si no alineado o fuera de rango.
     */
    uint16_t read_halfword(uint32_t address) const;

    /**
     * @brief Lee 4 bytes (word) en formato little-endian.
     *
     * Requiere alineacion estricta: address % 4 == 0.
     * En caso de acceso no alineado, se imprime un mensaje de error y se
     * lanza std::runtime_error deteniendo la simulacion.
     *
     * @param address  Direccion base (debe ser multiplo de 4).
     * @return         Word de 32 bits reconstruido en orden del host.
     * @throws         std::runtime_error si no alineado o fuera de rango.
     */
    uint32_t read_word(uint32_t address) const;

    // =========================================================================
    // Escritura de memoria — little-endian estricto
    // =========================================================================

    /**
     * @brief Escribe 1 byte en la direccion dada.
     * @param address  Direccion de memoria.
     * @param val      Byte a escribir.
     * @throws         std::runtime_error si address >= MEM_SIZE.
     */
    void write_byte(uint32_t address, uint8_t val);

    /**
     * @brief Escribe 2 bytes (halfword) en formato little-endian.
     *
     * Requiere alineacion estricta: address % 2 == 0.
     *
     * @param address  Direccion base (debe ser multiplo de 2).
     * @param val      Halfword de 16 bits a escribir (LSB va a address).
     * @throws         std::runtime_error si no alineado o fuera de rango.
     */
    void write_halfword(uint32_t address, uint16_t val);

    /**
     * @brief Escribe 4 bytes (word) en formato little-endian.
     *
     * Requiere alineacion estricta: address % 4 == 0.
     *
     * @param address  Direccion base (debe ser multiplo de 4).
     * @param val      Word de 32 bits a escribir (LSB va a address).
     * @throws         std::runtime_error si no alineado o fuera de rango.
     */
    void write_word(uint32_t address, uint32_t val);

    // =========================================================================
    // Decode utilities -- extraccion de campos e inmediatos
    // =========================================================================
    // Funciones estaticas y puras: no acceden al estado del simulador.
    // Se exponen como publicas para permitir pruebas unitarias directas.

    /**
     * @brief Extrae el opcode: bits [6:0].
     * @param instr Instruccion de 32 bits.
     * @return Opcode de 7 bits.
     */
    static uint32_t get_opcode(uint32_t instr);

    /**
     * @brief Extrae el registro destino rd: bits [11:7].
     * @param instr Instruccion de 32 bits.
     * @return Indice de registro (0-31).
     */
    static uint8_t get_rd(uint32_t instr);

    /**
     * @brief Extrae funct3: bits [14:12].
     * @param instr Instruccion de 32 bits.
     * @return Campo funct3 de 3 bits.
     */
    static uint32_t get_funct3(uint32_t instr);

    /**
     * @brief Extrae el registro fuente rs1: bits [19:15].
     * @param instr Instruccion de 32 bits.
     * @return Indice de registro (0-31).
     */
    static uint8_t get_rs1(uint32_t instr);

    /**
     * @brief Extrae el registro fuente rs2: bits [24:20].
     * @param instr Instruccion de 32 bits.
     * @return Indice de registro (0-31).
     */
    static uint8_t get_rs2(uint32_t instr);

    /**
     * @brief Extrae funct7: bits [31:25].
     * @param instr Instruccion de 32 bits.
     * @return Campo funct7 de 7 bits.
     */
    static uint32_t get_funct7(uint32_t instr);

    /**
     * @brief Inmediato formato I: instr[31:20], sign-extended a 32 bits.
     *
     * Usos: ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI, JALR, Loads.
     *
     *  31      20 19   15 14 12 11    7 6       0
     *  [ imm[11:0] ][ rs1 ][fn3][ rd  ][ opcode ]
     */
    static int32_t extract_imm_I(uint32_t instr);

    /**
     * @brief Inmediato formato S: instr[31:25|11:7], sign-extended a 32 bits.
     *
     * Usos: SB, SH, SW.
     *
     *  31    25 24  20 19  15 14 12 11    7 6       0
     *  [imm[11:5]][ rs2 ][ rs1 ][fn3][imm[4:0]][ opcode ]
     */
    static int32_t extract_imm_S(uint32_t instr);

    /**
     * @brief Inmediato formato B: branches, sign-extended a 32 bits.
     *
     * Usos: BEQ, BNE, BLT, BGE, BLTU, BGEU.
     *
     * Bits dispersos: instr[31]=imm[12], instr[30:25]=imm[10:5],
     *                 instr[11:8]=imm[4:1],  instr[7]=imm[11].
     */
    static int32_t extract_imm_B(uint32_t instr);

    /**
     * @brief Inmediato formato U: instr[31:12] en bits [31:12], bits [11:0]=0.
     *
     * Usos: LUI, AUIPC.
     */
    static int32_t extract_imm_U(uint32_t instr);

    /**
     * @brief Inmediato formato J: saltos JAL, sign-extended a 32 bits.
     *
     * Bits dispersos: instr[31]=imm[20], instr[30:21]=imm[10:1],
     *                 instr[20]=imm[11],  instr[19:12]=imm[19:12].
     */
    static int32_t extract_imm_J(uint32_t instr);

private:
    // =========================================================================
    // Helper de acceso a memoria
    // =========================================================================

    /**
     * @brief Valida alineacion y limites antes de cualquier acceso a memoria.
     *
     * Imprime un mensaje de error descriptivo en stderr y lanza
     * std::runtime_error si:
     *   a) size > 1 y (address % size) != 0  -> Error de alineacion
     *   b) address + size > MEM_SIZE          -> Acceso fuera de rango
     *
     * @param address    Direccion de inicio del acceso.
     * @param size       Ancho del acceso en bytes (1, 2 o 4).
     * @param operation  Nombre del metodo (para el mensaje de error).
     */
    void check_access(uint32_t address, uint8_t size, const char* operation) const;

    // =========================================================================
    // Estado arquitectural
    // =========================================================================

    uint32_t             regs_[32]; ///< Registros de proposito general x0–x31.
    uint32_t             pc_;       ///< Program Counter.
    std::vector<uint8_t> memory_;   ///< Memoria principal (MEM_SIZE bytes, byte-addressable).
};
