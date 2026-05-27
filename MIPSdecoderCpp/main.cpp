#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdio>
#include <cstdlib>

using namespace std;

class MipsSimulator {
private:
    struct Register32 {
        bool bits[32] = {false}; // bits[0] is MSB, bits[31] is LSB
    };

    Register32 reg[32];
    Register32 HI;
    Register32 LO;
    int data_mem[1024];
    int pc_code;

    // --- 內部輔助轉換工具 ---
    int toint(const bool* r, int length) {
        int ret = 0;
        for (int i = 0; i < length; ++i) {
            ret = (ret << 1) | r[i];
        }
        return ret;
    }

    int toint_signed(const bool* r, int length) {
        int ret = 0;
        if (r[0]) {
            for (int i = 0; i < length; ++i) {
                ret = (ret << 1) | !r[i];
            }
            ret = -(ret + 1);
        } else {
            for (int i = 0; i < length; ++i) {
                ret = (ret << 1) | r[i];
            }
        }
        return ret;
    }

    int toint_signed_from_int(const bool* r, int length) {
        return toint_signed(r, length);
    }

    void ret0(bool* r) {
        for (int i = 0; i < 32; i++) r[i] = false;
    }

    void ret1(bool* r) {
        for (int i = 0; i < 31; i++) r[i] = false;
        r[31] = true;
    }

    void shift_right_32(bool* r) {
        for (int i = 31; i > 0; --i) {
            r[i] = r[i - 1];
        }
        r[0] = false;
    }

    void shift_left_32(bool* r, bool fill_bit) {
        for (int i = 0; i < 31; ++i) {
            r[i] = r[i + 1];
        }
        r[31] = fill_bit;
    }

    // --- ALU 核心 ---
    bool ALU1bit(bool a, bool b, bool cin, bool invertB, int operation, bool *cout_val) {
        bool real_b = invertB ? !b : b;
        bool sum = a ^ real_b ^ cin;
        *cout_val = (a & real_b) | (cin & (a ^ real_b));

        switch (operation) {
            case 0: return a & real_b;  // AND
            case 1: return a | real_b;  // OR
            case 2: return sum;         // ADD 或 SUB
            default: return false;
        }
    }

    void ALU(const bool* ctrl, const bool* inputA, const bool* inputB, bool* output) {
        int controlSignal = toint(ctrl, 3);
        bool invertB = false;
        int operation = 2;

        switch (controlSignal) {
            case 0: invertB = false; operation = 0; break; // AND
            case 1: invertB = false; operation = 1; break; // OR
            case 2: invertB = false; operation = 2; break; // ADD
            case 6: invertB = true;  operation = 2; break; // SUB
            default: invertB = false; operation = 2; break;
        }

        bool current_cin = invertB;
        for (int i = 31; i >= 0; --i) {
            bool cout_val = false;
            output[i] = ALU1bit(inputA[i], inputB[i], current_cin, invertB, operation, &cout_val);
            current_cin = cout_val;
        }
    }

    void write_register(int reg_idx, const bool* data) {
        if (reg_idx == 0) return; // R0 唯讀保護
        for (int i = 0; i < 32; i++) {
            reg[reg_idx].bits[i] = data[i];
        }
    }

    // --- MIPS 指令實作 ---
    void add(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int rd_idx = toint(rd, 5);

        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i];
            inB[i] = reg[rt_idx].bits[i];
        }

        bool alu_ctrl[3] = {false, true, false};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
        printf("add R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
    }

    // R-format 33: addu
    void addu(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int rd_idx = toint(rd, 5);

        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i];
            inB[i] = reg[rt_idx].bits[i];
        }

        bool alu_ctrl[3] = {false, true, false}; // 同樣使用 ALU ADD 核心
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
        // addu 在硬體上不處理溢位(Overflow)，在模擬器中與 add 同邏輯，但輸出無號或視作一般加法
        printf("addu R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
    }

    void sub(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int rd_idx = toint(rd, 5);

        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i];
            inB[i] = reg[rt_idx].bits[i];
        }

        bool alu_ctrl[3] = {true, true, false};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
        printf("sub R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
    }

    void and_mips(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int rd_idx = toint(rd, 5);

        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i];
            inB[i] = reg[rt_idx].bits[i];
        }

        bool alu_ctrl[3] = {false, false, false};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
        printf("and R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
    }

    void or_mips(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int rd_idx = toint(rd, 5);

        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i];
            inB[i] = reg[rt_idx].bits[i];
        }

        bool alu_ctrl[3] = {false, false, true};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
        printf("or R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
    }

    void mult(const bool* rs, const bool* rt) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);

        bool multiplicand[32];
        for (int i = 0; i < 32; i++) {
            multiplicand[i] = reg[rs_idx].bits[i];
            HI.bits[i] = false;
            LO.bits[i] = reg[rt_idx].bits[i];
        }

        bool alu_ctrl[3] = {false, true, false}; // ADD
        bool inA[32], inB[32], out[32];

        for (int step = 0; step < 32; ++step) {
            if (LO.bits[31] == true) {
                for (int i = 0; i < 32; i++) {
                    inA[i] = HI.bits[i];
                    inB[i] = multiplicand[i];
                }
                ALU(alu_ctrl, inA, inB, out);
                for (int i = 0; i < 32; i++) HI.bits[i] = out[i];
            }
            bool hi_lowest_bit = HI.bits[31];
            shift_right_32(HI.bits);
            shift_right_32(LO.bits);
            LO.bits[0] = hi_lowest_bit;
        }
        printf("mult R%d, R%d (ALU Multiplier completed. LO value: %d)\n", rs_idx, rt_idx, toint_signed_from_int(LO.bits, 32));
    }

    void div_mips(const bool* rs, const bool* rt) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);

        bool divisor[32];
        for (int i = 0; i < 32; i++) {
            divisor[i] = reg[rt_idx].bits[i];
            HI.bits[i] = false;
            LO.bits[i] = reg[rs_idx].bits[i];
        }

        if (toint_signed_from_int(divisor, 32) == 0) {
            cerr << "not accept to divide with 0" << endl;
            return;
        }

        bool alu_add[3] = {false, true, false};
        bool alu_sub[3] = {true, true, false};
        bool inA[32], inB[32], out[32];

        for (int step = 0; step < 32; ++step) {
            bool lo_highest_bit = LO.bits[0];
            shift_left_32(HI.bits, lo_highest_bit);
            shift_left_32(LO.bits, false);

            for (int i = 0; i < 32; i++) {
                inA[i] = HI.bits[i];
                inB[i] = divisor[i];
            }
            ALU(alu_sub, inA, inB, out);

            if (out[0] == false) {
                for (int i = 0; i < 32; i++) HI.bits[i] = out[i];
                LO.bits[31] = true;
            } else {
                for (int i = 0; i < 32; i++) {
                    inA[i] = out[i];
                    inB[i] = divisor[i];
                }
                ALU(alu_add, inA, inB, out);
                for (int i = 0; i < 32; i++) HI.bits[i] = out[i];
                LO.bits[31] = false;
            }
        }
        printf("div R%d, R%d (ALU Divider completed. Quotient LO: %d, Remainder HI: %d)\n",
               rs_idx, rt_idx, toint_signed_from_int(LO.bits, 32), toint_signed_from_int(HI.bits, 32));
    }

    // R-format 12: syscall
    void syscall() {
        // 依據 MIPS 規範，系統呼叫代號通常儲存在 $v0 (R2) 暫存器中
        int v0_val = toint_signed_from_int(reg[2].bits, 32);
        printf("syscall triggered (R2/v0 = %d): ", v0_val);

        switch (v0_val) {
            case 1: // 列印整數 (通常放在 $a0 / R4)
                printf("Print Integer: %d\n", toint_signed_from_int(reg[4].bits, 32));
                break;
            case 10: // 結束程式
                printf("Exit Program.\n");
                // 可以選擇將 PC 指向非法位址來停下模擬器
                pc_code = -1;
                break;
            default:
                printf("Unsupported syscall service code.\n");
                break;
        }
    }

    void jr(const bool* rs) {
        int rs_idx = toint(rs, 5);
        int target_pc = toint_signed_from_int(reg[rs_idx].bits, 32);
        printf("jr R%d -> jump to address(instr line number): %d\n", rs_idx, target_pc);
        pc_code = target_pc;
    }

    bool beq(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);

        const bool *a = reg[rs_idx].bits;
        const bool *b = reg[rt_idx].bits;
        bool isEqual = true;

        for (int i = 0; i < 32; i++) {
            if (a[i] ^ b[i]) {
                isEqual = false;
                break;
            }
        }

        printf("beq compare R%d and R%d -> (result: %d)\n", rs_idx, rt_idx, isEqual);
        if (isEqual) {
            pc_code += offset;
            return true;
        }
        return false;
    }

    bool bne(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);

        const bool *a = reg[rs_idx].bits;
        const bool *b = reg[rt_idx].bits;
        bool isNotEqual = false;

        for (int i = 0; i < 32; i++) {
            if (a[i] ^ b[i]) {
                isNotEqual = true;
                break;
            }
        }

        printf("bne compare R%d and R%d -> (result: %d)\n", rs_idx, rt_idx, isNotEqual);
        if (isNotEqual) {
            pc_code += offset;
            return true;
        }
        return false;
    }

    void lw(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);

        int base_val = toint_signed_from_int(reg[rs_idx].bits, 32);
        int mem_address = base_val + offset;

        if (mem_address < 0 || mem_address >= 1024) {
            fprintf(stderr, "lw segmentation error: %d\n", mem_address);
            return;
        }

        int loaded_value = data_mem[mem_address];
        bool temp[32];
        for (int i = 31; i >= 0; --i) {
            temp[i] = (loaded_value >> (31 - i)) & 1;
        }
        write_register(rt_idx, temp);
        printf("lw R%d, %d(R%d) -> load value: %d\n", rt_idx, offset, rs_idx, loaded_value);
    }

    void addi(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int immediate = toint_signed(immed, 16);

        int val_rs = toint_signed_from_int(reg[rs_idx].bits, 32);
        int result = val_rs + immediate;

        bool temp[32];
        for (int i = 31; i >= 0; --i) {
            temp[i] = (result >> (31 - i)) & 1;
        }
        write_register(rt_idx, temp);
        printf("addi R%d, R%d, %d (result: %d)\n", rt_idx, rs_idx, immediate, result);
    }

    // I-format 9: addiu
    void addiu(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        // 注意：即使是 addiu，16-bit 立即數在 MIPS 中依然做「有號延伸 (Sign-Extension)」
        int immediate = toint_signed(immed, 16);

        int val_rs = toint_signed_from_int(reg[rs_idx].bits, 32);
        int result = val_rs + immediate;

        bool temp[32];
        for (int i = 31; i >= 0; --i) {
            temp[i] = (result >> (31 - i)) & 1;
        }
        write_register(rt_idx, temp);
        printf("addiu R%d, R%d, %d (result: %d)\n", rt_idx, rs_idx, immediate, result);
    }

    // I-format 15: lui
    void lui(const bool* rt, const bool* immed) {
        int rt_idx = toint(rt, 5);
        int immediate = toint(immed, 16); // 取出 16-bit 立即數

        bool temp[32] = {false};
        // 把 16-bit 填入高位 (bits 0 ~ 15)，低位為 0
        for (int i = 0; i < 16; ++i) {
            temp[i] = immed[i];
        }
        write_register(rt_idx, temp);
        printf("lui R%d, %d (result: %d)\n", rt_idx, immediate, (immediate << 16));
    }

    void slt(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int rd_idx = toint(rd, 5);

        int val_rs = toint_signed_from_int(reg[rs_idx].bits, 32);
        int val_rt = toint_signed_from_int(reg[rt_idx].bits, 32);

        bool temp[32];
        if (val_rs < val_rt) ret1(temp);
        else ret0(temp);

        write_register(rd_idx, temp);
        printf("slt R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, val_rs < val_rt);
    }

    void sw(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);

        int base_val = toint_signed_from_int(reg[rs_idx].bits, 32);
        int mem_address = base_val + offset;

        if (mem_address < 0 || mem_address >= 1024) {
            fprintf(stderr, "sw segmentation error: %d\n", mem_address);
            return;
        }

        int value_to_store = toint_signed_from_int(reg[rt_idx].bits, 32);
        data_mem[mem_address] = value_to_store;
        printf("sw R%d, %d(R%d) -> store value: %d into data_mem[%d]\n", rt_idx, offset, rs_idx, value_to_store, mem_address);
    }

    // --- 格式分流器 ---
    void Rformat(const bool* command) {
        int code = toint(command + 26, 6);
        switch(code) {
            case 8:  jr(command + 6); break;
            case 12: syscall(); break;                      // 新增 R-format 12 (syscall)
            case 24: mult(command + 6, command + 11); break;
            case 26: div_mips(command + 6, command + 11); break;
            case 32: add(command + 6, command + 11, command + 16); break;
            case 33: addu(command + 6, command + 11, command + 16); break; // 新增 R-format 33 (addu)
            case 34: sub(command + 6, command + 11, command + 16); break;
            case 36: and_mips(command + 6, command + 11, command + 16); break;
            case 37: or_mips(command + 6, command + 11, command + 16); break;
            case 42: slt(command + 6, command + 11, command + 16); break;
            default: printf("unknown R-format funct code: %d\n", code); break;
        }
    }

    void Jformat(const bool* command) {
        int op = toint(command, 6);
        int target_address = toint(command + 6, 26);

        switch(op) {
            case 2:
                printf("j -> absolute jump to target address(instr line number): %d\n", target_address);
                pc_code = target_address;
                break;
            case 3: {
                printf("jal -> record the returned address to R31 and jump to the location: %d\n", target_address);
                int next_instruction = pc_code + 1;
                bool temp[32];
                for (int i = 31; i >= 0; --i) {
                    temp[i] = (next_instruction >> (31 - i)) & 1;
                }
                write_register(31, temp);
                pc_code = target_address;
                break;
            }
            default: printf("unknown J-format opcode: %d\n", op); break;
        }
    }

    bool Iformat(const bool* command) {
        int code = toint(command, 6);
        switch(code) {
            case 4:  return beq(command + 6, command + 11, command + 16);
            case 5:  return bne(command + 6, command + 11, command + 16);
            case 8:  addi(command + 6, command + 11, command + 16); break;
            case 9:  addiu(command + 6, command + 11, command + 16); break; // 新增 I-format 9 (addiu)
            case 15: lui(command + 11, command + 16); break;               // 新增 I-format 15 (lui, 注意 rs 欄位在此指令通常不使用，寫入 rt)
            case 35: lw(command + 6, command + 11, command + 16); break;
            case 43: sw(command + 6, command + 11, command + 16); break;
            default: printf("unknown I-format opcode: %d\n", code); break;
        }
        return false;
    }

    // --- 單一 32-bit 指令底層執行邏輯 ---
    bool executeSingleCommand(const bool* command) {
        int op_type = toint(command, 6);
        bool is_jumped = false;

        if (op_type == 0) {
            int funct_code = toint(command + 26, 6);
            if (funct_code == 8) {
                is_jumped = true;
            }
            Rformat(command);
        } else if (op_type == 2 || op_type == 3) {
            is_jumped = true;
            Jformat(command);
        } else {
            is_jumped = Iformat(command);
        }
        return is_jumped;
    }

public:
    // 建構子：初始化暫存器與記憶體狀態
    MipsSimulator() {
        pc_code = 0;
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 32; j++) reg[i].bits[j] = false;
        }
        for (int i = 0; i < 32; i++) {
            HI.bits[i] = false;
            LO.bits[i] = false;
        }
        for (int i = 0; i < 1024; i++) data_mem[i] = 0;

        // 預設測試資料
        data_mem[8] = 99;
        reg[1].bits[28] = true; reg[1].bits[30] = true; // R1 = 10
        reg[2].bits[30] = true; reg[2].bits[31] = true; // R2 = 3
    }

    // --- 直接傳入 32 字元的二進位字串並執行 ---
    void command(string machineCode) {
        if (machineCode.length() != 32) {
            cerr << "Error: Machine code must be exactly 32 bits long!" << endl;
            return;
        }

        bool cmdBits[32];
        for (int i = 0; i < 32; ++i) {
            cmdBits[i] = (machineCode[i] == '1');
        }

        executeSingleCommand(cmdBits);
    }

    // 讀取並執行完整的二進位二進制檔案
    bool runBinaryFile(const string& filename) {
        ifstream file(filename, ios::binary | ios::ate);
        if (!file.is_open()) {
            cerr << "failed to load file: " << filename << endl;
            return false;
        }

        streampos fileSize = file.tellg();
        file.seekg(0, ios::beg);

        vector<bool> bits;
        bits.reserve(fileSize * 8);

        char byte;
        while (file.read(&byte, 1)) {
            for (int i = 7; i >= 0; --i) {
                bits.push_back((byte >> i) & 1);
            }
        }
        file.close();

        int total_bits = bits.size();
        int total_instructions = total_bits / 32;
        cout << "success to load file, include with " << total_instructions << " commands" << endl;

        pc_code = 0;
        while (pc_code >= 0 && pc_code * 32 < total_bits) {
            bool current_command[32];
            for (int i = 0; i < 32; i++) {
                current_command[i] = bits[i + pc_code * 32];
            }

            bool is_jumped = executeSingleCommand(current_command);
            if (!is_jumped) {
                pc_code++;
            }
        }
        return true;
    }

    void dump_registers() {
        printf("\n==================== REGISTER DUMP ====================\n");
        for (int i = 0; i < 32; ++i) {
            int val = toint_signed_from_int(reg[i].bits, 32);
            printf("R%d: %d\t", i, val);
            if ((i + 1) % 4 == 0) {
                printf("\n");
            }
        }
        int hi_val = toint_signed_from_int(HI.bits, 32);
        int lo_val = toint_signed_from_int(LO.bits, 32);
        printf("HI: %d\tLO: %d\tPC: %d\n", hi_val, lo_val, pc_code);
        printf("=======================================================\n\n");
    }
};
int main() {
    MipsSimulator sim;

    cout << "--- fibonacci testcase ---" << endl;

    sim.command("00100100000010000000000000000111");
    sim.command("00100100000100000000000000000000");
    sim.command("00100100000100010000000000000001");
    sim.command("00100100000010010000000000000010");
    sim.command("00000001000010010101000000101010");
    sim.command("00010101010000000000000000000111");
    sim.command("00000010001100001001000000100001");
    sim.command("00000010001000001000000000100001");
    sim.command("00000010010000001000100000100001");
    sim.command("00100100100010010000000000000001");
    for(int i = 0; i < 5; i++) {
        sim.command("00000010001100001001000000100001");
        sim.command("00000010001000001000000000100001");
        sim.command("00000010010000001000100000100001");
        sim.command("00100101001010010000000000000001");
    }
    sim.command("00100100000000100000000000000001");
    sim.command("00000010010000000010000000100001");
    sim.command("00000000000000000000000000001100");
    sim.command("00100100000000100000000000001010");
    sim.command("00000000000000000000000000001100");
    sim.dump_registers();
    return 0;
}
