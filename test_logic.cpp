// =============================================================================
// test_logic.cpp -- Prueba unitaria del Simulador RISC-V RV32I
// =============================================================================
//
// ESTRUCTURA:
//   FASE 1 -- Registros y memoria (Fase 1)
//   FASE 2a -- Campos de instruccion (get_opcode / rd / funct3 / rs1 / rs2 / funct7)
//   FASE 2b -- Inmediatos con sign-extension (I, S, B, U, J)
//   FASE 2c -- Fetch y dispatch (step)
//
// METODOLOGIA:
//   Cada instruccion usada como fixture se muestra con su encoding binario
//   descompuesto para que sea facil auditar el valor hexadecimal a mano.
//
// NOTA sobre sign-extension:
//   La tecnica SAR (Shift Arithmetic Right) sobre int32_t propaga el bit de
//   signo hacia la izquierda. En C++17 este comportamiento es
//   implementation-defined pero garantizado en x86/ARM/RISC-V modernos.
//   C++20 lo estandariza formalmente.
//
// Estandar: C++17
// =============================================================================

#include "Simulator.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

// =============================================================================
// Framework minimo de tests
// =============================================================================

static int g_tests_run    = 0;
static int g_tests_failed = 0;

// test() con diagnostico: muestra los valores got/expected al fallar.
template <typename T>
static void test(const string& name, T got, T expected)
{
    ++g_tests_run;
    bool ok = (got == expected);
    if (ok) {
        cout << "[PASS] " << name << "\n";
    } else {
        ++g_tests_failed;
        cout << "[FAIL] " << name << "\n"
                  << "         got      = 0x" << hex << setw(8)
                  << setfill('0') << static_cast<uint32_t>(got)
                  << "  (" << dec << static_cast<int32_t>(got) << ")\n"
                  << "         expected = 0x" << hex << setw(8)
                  << setfill('0') << static_cast<uint32_t>(expected)
                  << "  (" << dec << static_cast<int32_t>(expected) << ")\n";
    }
}

// Sobrecarga bool para tests sin valor numerico
static void test(const string& name, bool ok)
{
    ++g_tests_run;
    cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
    if (!ok) ++g_tests_failed;
}

// =============================================================================
// main
// =============================================================================

int main()
{
    cout << "=================================================================\n"
              << " Tests del Simulador RISC-V RV32I  (Fase 1 + Fase 2)\n"
              << "=================================================================\n\n";

    // =========================================================================
    // FASE 1 -- Registros y memoria
    // =========================================================================
    cout << "--- Fase 1: Registros y Memoria ---\n";

    Simulator sim;

    // T01: Regla de Oro -- x0 siempre es 0
    sim.set_reg(0, 0xDEADBEEFu);
    test("T01  x0 siempre 0 (set_reg ignorado)", uint32_t(sim.get_reg(0)), uint32_t(0));

    // T02-T03: Registros normales
    sim.set_reg(1, 0x12345678u);
    test("T02  x1 escribe y lee correctamente",  uint32_t(sim.get_reg(1)),  uint32_t(0x12345678u));
    sim.set_reg(31, 0xCAFEBABEu);
    test("T03  x31 escribe y lee correctamente", uint32_t(sim.get_reg(31)), uint32_t(0xCAFEBABEu));

    // T04-T08: write_word / read_word y orden little-endian en bytes
    //   0xDEADBEEF -> [addr]=EF, [addr+1]=BE, [addr+2]=AD, [addr+3]=DE
    sim.write_word(0x0u, 0xDEADBEEFu);
    test("T04  write_word/read_word round-trip", uint32_t(sim.read_word(0x0u)), uint32_t(0xDEADBEEFu));
    test("T05  Byte[0] = 0xEF (LSB)",            uint32_t(sim.read_byte(0x0u)), uint32_t(0xEFu));
    test("T06  Byte[1] = 0xBE",                  uint32_t(sim.read_byte(0x1u)), uint32_t(0xBEu));
    test("T07  Byte[2] = 0xAD",                  uint32_t(sim.read_byte(0x2u)), uint32_t(0xADu));
    test("T08  Byte[3] = 0xDE (MSB)",            uint32_t(sim.read_byte(0x3u)), uint32_t(0xDEu));

    // T09-T11: write_halfword / read_halfword
    //   0xABCD -> [0x4]=CD, [0x5]=AB
    sim.write_halfword(0x4u, 0xABCDu);
    test("T09  write_halfword/read_halfword round-trip", uint32_t(sim.read_halfword(0x4u)), uint32_t(0xABCDu));
    test("T10  Halfword Byte[4] = 0xCD (LSB)",          uint32_t(sim.read_byte(0x4u)),     uint32_t(0xCDu));
    test("T11  Halfword Byte[5] = 0xAB (MSB)",          uint32_t(sim.read_byte(0x5u)),     uint32_t(0xABu));

    // T12: write_byte / read_byte
    sim.write_byte(0x8u, 0x42u);
    test("T12  write_byte/read_byte round-trip", uint32_t(sim.read_byte(0x8u)), uint32_t(0x42u));

    // T13-T14: Alineacion forzada
    {
        bool caught = false;
        try { sim.read_word(0x1u); } catch (const runtime_error&) { caught = true; }
        test("T13  read_word en dir. no alineada lanza excepcion", caught);
    }
    {
        bool caught = false;
        try { sim.read_halfword(0x3u); } catch (const runtime_error&) { caught = true; }
        test("T14  read_halfword en dir. no alineada lanza excepcion", caught);
    }

    // T15: Acceso fuera de rango
    {
        bool caught = false;
        try { sim.read_word(Simulator::MEM_SIZE - 2u); } catch (const runtime_error&) { caught = true; }
        test("T15  read_word fuera de rango lanza excepcion", caught);
    }

    // =========================================================================
    // FASE 2a -- Campos de instruccion (helpers de bits)
    // =========================================================================
    // Instrucciones de referencia calculadas a mano:
    //
    //  ADD x5, x6, x7  (R-type, funct7=0)
    //  Encoding: funct7[31:25]=0000000 rs2[24:20]=00111 rs1[19:15]=00110
    //            funct3[14:12]=000 rd[11:7]=00101 opcode[6:0]=0110011
    //  = 0000_0000_0111_0011_0000_0010_1011_0011 = 0x007302B3
    //
    //  SUB x15, x14, x13  (R-type, funct7=0x20)
    //  Encoding: funct7=0100000 rs2=01101(13) rs1=01110(14)
    //            funct3=000 rd=01111(15) opcode=0110011
    //  = 0100_0000_1101_0111_0000_0111_1011_0011 = 0x40D707B3
    //
    //  SLT x2, x3, x4  (R-type, funct3=2)
    //  Encoding: funct7=0000000 rs2=00100(4) rs1=00011(3)
    //            funct3=010 rd=00010(2) opcode=0110011
    //  = 0000_0000_0100_0001_1010_0001_0011_0011 = 0x0041A133
    //
    //  ADDI x1, x0, 1  (I-type, opcode=0x13)
    //  Encoding: imm[31:20]=000000000001 rs1=00000 funct3=000 rd=00001 opcode=0010011
    //  = 0000_0000_0001_0000_0000_0000_1001_0011 = 0x00100093
    // =========================================================================
    cout << "\n--- Fase 2a: Campos de instruccion ---\n";

    // Instruccion: ADD x5, x6, x7 = 0x007302B3
    {
        constexpr uint32_t ADD_x5_x6_x7 = 0x007302B3u;
        test("T16  ADD x5,x6,x7 -> opcode = 0x33",
             Simulator::get_opcode(ADD_x5_x6_x7), uint32_t(0x33u));
        test("T17  ADD x5,x6,x7 -> rd = 5",
             uint32_t(Simulator::get_rd(ADD_x5_x6_x7)), uint32_t(5u));
        test("T18  ADD x5,x6,x7 -> funct3 = 0",
             Simulator::get_funct3(ADD_x5_x6_x7), uint32_t(0u));
        test("T19  ADD x5,x6,x7 -> rs1 = 6",
             uint32_t(Simulator::get_rs1(ADD_x5_x6_x7)), uint32_t(6u));
        test("T20  ADD x5,x6,x7 -> rs2 = 7",
             uint32_t(Simulator::get_rs2(ADD_x5_x6_x7)), uint32_t(7u));
        test("T21  ADD x5,x6,x7 -> funct7 = 0x00",
             Simulator::get_funct7(ADD_x5_x6_x7), uint32_t(0x00u));
    }

    // Instruccion: SUB x15, x14, x13 = 0x40D707B3
    {
        constexpr uint32_t SUB_x15_x14_x13 = 0x40D707B3u;
        test("T22  SUB x15,x14,x13 -> opcode = 0x33",
             Simulator::get_opcode(SUB_x15_x14_x13), uint32_t(0x33u));
        test("T23  SUB x15,x14,x13 -> rd = 15",
             uint32_t(Simulator::get_rd(SUB_x15_x14_x13)), uint32_t(15u));
        test("T24  SUB x15,x14,x13 -> rs1 = 14",
             uint32_t(Simulator::get_rs1(SUB_x15_x14_x13)), uint32_t(14u));
        test("T25  SUB x15,x14,x13 -> rs2 = 13",
             uint32_t(Simulator::get_rs2(SUB_x15_x14_x13)), uint32_t(13u));
        test("T26  SUB x15,x14,x13 -> funct7 = 0x20 (ADD vs SUB bit)",
             Simulator::get_funct7(SUB_x15_x14_x13), uint32_t(0x20u));
    }

    // Instruccion: SLT x2, x3, x4 = 0x0041A133  (funct3 != 0)
    {
        constexpr uint32_t SLT_x2_x3_x4 = 0x0041A133u;
        test("T27  SLT x2,x3,x4 -> funct3 = 2 (0b010)",
             Simulator::get_funct3(SLT_x2_x3_x4), uint32_t(2u));
        test("T28  SLT x2,x3,x4 -> rd = 2",
             uint32_t(Simulator::get_rd(SLT_x2_x3_x4)), uint32_t(2u));
        test("T29  SLT x2,x3,x4 -> rs1 = 3",
             uint32_t(Simulator::get_rs1(SLT_x2_x3_x4)), uint32_t(3u));
        test("T30  SLT x2,x3,x4 -> rs2 = 4",
             uint32_t(Simulator::get_rs2(SLT_x2_x3_x4)), uint32_t(4u));
    }

    // Instruccion: ADDI x1, x0, 1 = 0x00100093  (opcode I-type)
    {
        constexpr uint32_t ADDI_x1_x0_1 = 0x00100093u;
        test("T31  ADDI x1,x0,1 -> opcode = 0x13 (OP_ALUI)",
             Simulator::get_opcode(ADDI_x1_x0_1), uint32_t(0x13u));
        test("T32  ADDI x1,x0,1 -> rd = 1",
             uint32_t(Simulator::get_rd(ADDI_x1_x0_1)), uint32_t(1u));
        test("T33  ADDI x1,x0,1 -> rs1 = 0",
             uint32_t(Simulator::get_rs1(ADDI_x1_x0_1)), uint32_t(0u));
    }

    // =========================================================================
    // FASE 2b -- Inmediatos con sign-extension
    // =========================================================================
    // Para cada formato se prueba:
    //   - Un inmediato POSITIVO (bit de signo = 0)
    //   - Un inmediato NEGATIVO (bit de signo = 1) -- critico para SAR
    //   - El maximo y minimo del rango cuando aplica
    //
    // Los encodings se recalculan bit a bit en los comentarios para
    // que cualquier discrepancia sea auditable sin herramienta externa.
    // =========================================================================
    cout << "\n--- Fase 2b: Inmediatos con sign-extension ---\n";

    // -------------------------------------------------------------------------
    // Formato I  (imm[11:0] = instr[31:20])
    // sign-extension: static_cast<int32_t>(instr) >> 20  (SAR 20)
    //
    //  ADDI x1, x0, +42    imm=0x02A, instr[31:20]=0000_0010_1010
    //  Encoding: 0000_0010_1010_0000_0000_0000_1001_0011 = 0x02A00093
    //
    //  ADDI x1, x0, +2047  imm=0x7FF (max positivo 12-bit)
    //  Encoding: 0111_1111_1111_0000_0000_0000_1001_0011 = 0x7FF00093
    //
    //  ADDI x1, x0, -1     imm=0xFFF (todos 1s en 12-bit)
    //  Encoding: 1111_1111_1111_0000_0000_0000_1001_0011 = 0xFFF00093
    //
    //  ADDI x1, x0, -2048  imm=0x800 (minimo negativo 12-bit)
    //  Encoding: 1000_0000_0000_0000_0000_0000_1001_0011 = 0x80000093
    // -------------------------------------------------------------------------
    cout << "  [Formato I]\n";

    test("T34  imm_I: ADDI x1,x0,+42   -> +42",
         Simulator::extract_imm_I(0x02A00093u), int32_t(42));
    test("T35  imm_I: ADDI x1,x0,+2047 -> +2047  (maximo positivo)",
         Simulator::extract_imm_I(0x7FF00093u), int32_t(2047));
    test("T36  imm_I: ADDI x1,x0,-1    -> -1     (bit signo set, todos 1s)",
         Simulator::extract_imm_I(0xFFF00093u), int32_t(-1));
    test("T37  imm_I: ADDI x1,x0,-2048 -> -2048  (minimo negativo)",
         Simulator::extract_imm_I(0x80000093u), int32_t(-2048));

    // -------------------------------------------------------------------------
    // Formato S  (imm[11:5]=instr[31:25], imm[4:0]=instr[11:7])
    // sign-extension: SAR desde bit 11 del campo ensamblado
    //
    //  SW x5, 8(x6)     imm=+8  = 0b0000_0000_1000
    //  imm[11:5]=0000000, imm[4:0]=01000
    //  Encoding:
    //   [31:25]=0000000  [24:20]=rs2=x5=00101  [19:15]=rs1=x6=00110
    //   [14:12]=funct3=010  [11:7]=01000  [6:0]=opcode=0100011
    //  = 0000_0000_0101_0011_0010_0100_0010_0011 = 0x00532423
    //
    //  SW x5, -4(x6)    imm=-4  = 0b1111_1111_1100
    //  imm[11:5]=1111111(0x7F), imm[4:0]=11100(0x1C)
    //  Encoding:
    //   [31:25]=1111111  [24:20]=00101  [19:15]=00110
    //   [14:12]=010  [11:7]=11100  [6:0]=0100011
    //  = 1111_1110_0101_0011_0010_1110_0010_0011 = 0xFE532E23
    // -------------------------------------------------------------------------
    cout << "  [Formato S]\n";

    test("T38  imm_S: SW x5,8(x6)   -> +8",
         Simulator::extract_imm_S(0x00532423u), int32_t(8));
    test("T39  imm_S: SW x5,-4(x6)  -> -4   (sign-extension con funct7 = 0x7F)",
         Simulator::extract_imm_S(0xFE532E23u), int32_t(-4));

    // -------------------------------------------------------------------------
    // Formato B  (bits dispersos; imm[0]=0 implicito)
    //   instr[31]=imm[12]  instr[30:25]=imm[10:5]
    //   instr[11:8]=imm[4:1]  instr[7]=imm[11]
    // sign-extension: SAR desde bit 12 del campo ensamblado
    //
    //  BEQ x0, x0, +8    imm=+8
    //   imm[12]=0, imm[11]=0, imm[10:5]=000000, imm[4:1]=0100
    //   [31]=0 [30:25]=000000 [24:20]=00000 [19:15]=00000
    //   [14:12]=000 [11:8]=0100 [7]=0 [6:0]=1100011
    //  = 0000_0000_0000_0000_0000_0100_0110_0011 = 0x00000463
    //
    //  BEQ x0, x0, -8    imm=-8 (13-bit two's complement: 1_1111_1111_1000)
    //   imm[12]=1, imm[11]=1, imm[10:5]=111111, imm[4:1]=1100
    //   [31]=1 [30:25]=111111 [24:20]=00000 [19:15]=00000
    //   [14:12]=000 [11:8]=1100 [7]=1 [6:0]=1100011
    //  = 1111_1110_0000_0000_0000_1100_1110_0011 = 0xFE000CE3
    // -------------------------------------------------------------------------
    cout << "  [Formato B]\n";

    test("T40  imm_B: BEQ x0,x0,+8  -> +8",
         Simulator::extract_imm_B(0x00000463u), int32_t(8));
    test("T41  imm_B: BEQ x0,x0,-8  -> -8   (imm[12]=1, extension aritmetica)",
         Simulator::extract_imm_B(0xFE000CE3u), int32_t(-8));

    // -------------------------------------------------------------------------
    // Formato U  (instr[31:12] son los 20 bits del inmediato; bits [11:0]=0)
    // NO requiere SAR adicional -- el campo ya ocupa toda la palabra.
    // La mascara 0xFFFFF000 limpia rd y opcode.
    //
    //  LUI x1, 0x12345    rd=1 (00001), opcode=0x37
    //   instr = (0x12345 << 12) | (1 << 7) | 0x37
    //         = 0x12345000 | 0x80 | 0x37 = 0x123450B7
    //   extract_imm_U = 0x12345000 = +305419008
    //
    //  LUI x1, 0xFFFFF    (todos los 20 bits en 1 -> valor como int32_t = -4096)
    //   instr = 0xFFFFF000 | 0x80 | 0x37 = 0xFFFFF0B7
    //   extract_imm_U = (int32_t)0xFFFFF000 = -4096
    // -------------------------------------------------------------------------
    cout << "  [Formato U]\n";

    test("T42  imm_U: LUI x1,0x12345   -> 0x12345000 (positivo)",
         Simulator::extract_imm_U(0x123450B7u), int32_t(0x12345000));
    test("T43  imm_U: LUI x1,0xFFFFF   -> -4096      (bit31=1, negativo como int32_t)",
         Simulator::extract_imm_U(0xFFFFF0B7u), int32_t(-4096));

    // -------------------------------------------------------------------------
    // Formato J  (bits dispersos para JAL; imm[0]=0 implicito)
    //   instr[31]=imm[20]  instr[30:21]=imm[10:1]
    //   instr[20]=imm[11]  instr[19:12]=imm[19:12]
    // sign-extension: SAR desde bit 20 del campo ensamblado
    //
    //  JAL x0, +8    imm=+8 (21-bit: 0_0000_0000_0000_0000_1000)
    //   imm[20]=0, imm[19:12]=0, imm[11]=0, imm[10:1]=0000000100
    //   [31]=0 [30:21]=0000000100 [20]=0 [19:12]=00000000
    //   [11:7]=00000 [6:0]=1101111
    //  = 0000_0000_1000_0000_0000_0000_0110_1111 = 0x0080006F
    //
    //  JAL x0, -8    imm=-8 (21-bit two's complement: 1_1111_1111_1111_1111_1000)
    //   imm[20]=1, imm[19:12]=0xFF, imm[11]=1, imm[10:1]=0b11_1111_1100(0x3FC)
    //   [31]=1 [30:21]=1111111100 [20]=1 [19:12]=11111111
    //   [11:7]=00000 [6:0]=1101111
    //   bits[23:20]=1001 (imm[3]=1 sets bit23, imm[2..1] sets bits22,21=0, bit20=1)
    //  = 1111_1111_1001_1111_1111_0000_0110_1111 = 0xFF9FF06F
    // -------------------------------------------------------------------------
    cout << "  [Formato J]\n";

    test("T44  imm_J: JAL x0,+8  -> +8",
         Simulator::extract_imm_J(0x0080006Fu), int32_t(8));
    test("T45  imm_J: JAL x0,-8  -> -8   (imm[20]=1, extension aritmetica 21 bits)",
         Simulator::extract_imm_J(0xFF9FF06Fu), int32_t(-8));

    // Caso adicional: rango maximo positivo de J (+1048574 = 2^20 - 2)
    // imm = 0x000FFFFE -> imm[20:1] = 0x7FFFF (todos 1s excepto bit20)
    // Encoding: bit[31]=0, bits[30:21]=1111111111(0x3FF), bit[20]=1,
    //           bits[19:12]=0xFF, bits[11:7]=00000, bits[6:0]=1101111
    // = 0111_1111_1111_1111_1111_0000_0110_1111 = 0x7FFFF06F
    test("T46  imm_J: JAL x0,+1048574 -> +1048574  (maximo positivo J)",
         Simulator::extract_imm_J(0x7FFFF06Fu), int32_t(1048574));

    // =========================================================================
    // FASE 2c -- Fetch y dispatch (step)
    // =========================================================================
    cout << "\n--- Fase 2c: Fetch y dispatch (step) ---\n";

    // T47: opcode ilegal (0x02 no existe en RV32I base) -> runtime_error
    {
        Simulator s;
        s.write_word(0x0u, 0x00000002u);  // opcode=0x02 (ilegal)
        bool caught = false;
        try { s.step(); } catch (const runtime_error&) { caught = true; }
        test("T47  step() con opcode ilegal (0x02) lanza runtime_error", caught);
    }

    // T48-T58: cada familia de opcodes validos no lanza excepcion
    struct OpcodeCase { const char* name; uint32_t instr; };
    static const OpcodeCase valid[] = {
        // LUI   x1, 1         opcode=0x37
        {"T48  LUI   (0x37) no lanza",  0x00001037u},
        // AUIPC x1, 1         opcode=0x17
        {"T49  AUIPC (0x17) no lanza",  0x00001097u},
        // JAL   x0, 0         opcode=0x6F
        {"T50  JAL   (0x6F) no lanza",  0x0000006Fu},
        // JALR  x0, x0, 0     opcode=0x67
        {"T51  JALR  (0x67) no lanza",  0x00000067u},
        // BEQ   x0, x0, 0     opcode=0x63
        {"T52  BEQ   (0x63) no lanza",  0x00000063u},
        // LW    x1, 0(x0)     opcode=0x03
        {"T53  LOAD  (0x03) no lanza",  0x00002083u},
        // SW    x0, 0(x0)     opcode=0x23
        {"T54  STORE (0x23) no lanza",  0x00002023u},
        // ADDI  x0, x0, 0     opcode=0x13  (NOP canonico)
        {"T55  ALU-I (0x13) no lanza",  0x00000013u},
        // ADD   x0, x0, x0    opcode=0x33
        {"T56  ALU-R (0x33) no lanza",  0x00000033u},
        // FENCE               opcode=0x0F
        {"T57  FENCE (0x0F) no lanza",  0x0000000Fu},
        // ECALL               opcode=0x73
        {"T58  ECALL (0x73) no lanza",  0x00000073u},
    };

    for (const auto& c : valid) {
        Simulator s;
        s.write_word(0x0u, c.instr);
        bool ok = true;
        try { s.step(); } catch (...) { ok = false; }
        test(c.name, ok);
    }

    // T59: fetch secuencial -- PC avanza 0x0 -> 0x4 -> 0x8
    {
        Simulator s;
        s.write_word(0x0u, 0x00000013u);  // NOP en 0x0
        s.write_word(0x4u, 0x00000013u);  // NOP en 0x4
        bool ok = true;
        try {
            s.step();   // fetch en 0x0, PC -> 0x4
            s.step();   // fetch en 0x4, PC -> 0x8
        } catch (...) { ok = false; }
        test("T59  fetch() secuencial NOP NOP sin excepcion (PC 0->4->8)", ok);
    }

    // T60: print_state no lanza excepciones
    {
        bool ok = true;
        try { sim.print_state(); } catch (...) { ok = false; }
        test("T60  print_state() ejecuta sin excepciones", ok);
    }

    // =========================================================================
    // Resumen
    // =========================================================================
    cout << "\n=================================================================\n";
    if (g_tests_failed == 0) {
        cout << " [OK] " << g_tests_run << "/" << g_tests_run
                  << " tests pasaron correctamente.\n";
    } else {
        cout << " [KO] " << g_tests_failed << " de " << g_tests_run
                  << " tests FALLARON.\n";
        return 1;
    }
    cout << "=================================================================\n";
    return 0;
}
