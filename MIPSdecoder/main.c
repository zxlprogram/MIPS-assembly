#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    bool bits[32]; // bits[0] is MSB, bits[31] is LSB
} Register32;

static Register32 reg[32];
static Register32 HI;
static Register32 LO;
static int data_mem[1024];
static int pc_code = 0;

bool* readBinToPtrBool(const char* filename, int* total_bits) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "failed to load file: %s\n", filename);
        *total_bits = 0;
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    *total_bits = (int)(fileSize * 8);
    bool* bitArray = (bool*)malloc((*total_bits) * sizeof(bool));
    if (!bitArray) {
        fprintf(stderr, "Memory allocation failed\n");
        *total_bits = 0;
        fclose(file);
        return NULL;
    }

    char byte;
    int bitIdx = 0;
    while (fread(&byte, 1, 1, file) == 1) {
        for (int i = 7; i >= 0; --i) {
            bool bit = (byte >> i) & 1;
            bitArray[bitIdx++] = bit;
        }
    }
    fclose(file);
    return bitArray;
}

int toint(bool* r, int length) {
    int ret = 0;
    for(int i = 0; i < length; ++i) {
        ret = (ret << 1) | r[i];
    }
    return ret;
}

int toint_signed(bool* r, int length) {
    int ret = 0;
    if (r[0]) {
        for(int i = 0; i < length; ++i) {
            ret = (ret << 1) | !r[i];
        }
        ret = -(ret + 1);
    } else {
        for(int i = 0; i < length; ++i) {
            ret = (ret << 1) | r[i];
        }
    }
    return ret;
}

int toint_signed_from_int(bool* r, int length) {
    int ret = 0;
    if (r[0]) {
        for(int i = 0; i < length; ++i) {
            ret = (ret << 1) | !r[i];
        }
        ret = -(ret + 1);
    } else {
        for(int i = 0; i < length; ++i) {
            ret = (ret << 1) | r[i];
        }
    }
    return ret;
}

void ret0(bool* r) {
    for(int i = 0; i < 32; i++) r[i] = 0;
}

void ret1(bool* r) {
    for(int i = 0; i < 31; i++) r[i] = 0;
    r[31] = 1;
}

// 32-bit 邏輯右移 (MSB補0)
void shift_right_32(bool* r) {
    for (int i = 31; i > 0; --i) {
        r[i] = r[i - 1];
    }
    r[0] = 0;
}

// 32-bit 左移 (LSB補帶入的 bit)
void shift_left_32(bool* r, bool fill_bit) {
    for (int i = 0; i < 31; ++i) {
        r[i] = r[i + 1];
    }
    r[31] = fill_bit;
}

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

void ALU(bool* ctrl, bool* inputA, bool* inputB, bool* output) {
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

void write_register(int reg_idx, bool* data) {
    if (reg_idx == 0) return; // R0 唯讀保護
    for(int i = 0; i < 32; i++) {
        reg[reg_idx].bits[i] = data[i];
    }
}

void add(bool* rs, bool* rt, bool* rd) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(rd, 5);

    bool inA[32], inB[32], out[32];
    for(int i = 0; i < 32; i++) {
        inA[i] = reg[rs_idx].bits[i];
        inB[i] = reg[rt_idx].bits[i];
    }

    bool alu_ctrl[3] = {0, 1, 0};
    ALU(alu_ctrl, inA, inB, out);

    write_register(rd_idx, out);
    printf("add R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
}

void sub(bool* rs, bool* rt, bool* rd) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(rd, 5);

    bool inA[32], inB[32], out[32];
    for(int i = 0; i < 32; i++) {
        inA[i] = reg[rs_idx].bits[i];
        inB[i] = reg[rt_idx].bits[i];
    }

    bool alu_ctrl[3] = {1, 1, 0};
    ALU(alu_ctrl, inA, inB, out);

    write_register(rd_idx, out);
    printf("sub R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
}

void and_mips(bool* rs, bool* rt, bool* rd) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(rd, 5);

    bool inA[32], inB[32], out[32];
    for(int i = 0; i < 32; i++) {
        inA[i] = reg[rs_idx].bits[i];
        inB[i] = reg[rt_idx].bits[i];
    }

    bool alu_ctrl[3] = {0, 0, 0};
    ALU(alu_ctrl, inA, inB, out);

    write_register(rd_idx, out);
    printf("and R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
}

void or_mips(bool* rs, bool* rt, bool* rd) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(rd, 5);

    bool inA[32], inB[32], out[32];
    for(int i = 0; i < 32; i++) {
        inA[i] = reg[rs_idx].bits[i];
        inB[i] = reg[rt_idx].bits[i];
    }

    bool alu_ctrl[3] = {0, 0, 1};
    ALU(alu_ctrl, inA, inB, out);

    write_register(rd_idx, out);
    printf("or R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
}

void mult(bool* rs, bool* rt) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);

    bool multiplicand[32];
    for(int i = 0; i < 32; i++) {
        multiplicand[i] = reg[rs_idx].bits[i];
        HI.bits[i] = 0;
        LO.bits[i] = reg[rt_idx].bits[i];
    }

    bool alu_ctrl[3] = {0, 1, 0}; // ADD
    bool inA[32], inB[32], out[32];

    for (int step = 0; step < 32; ++step) {
        if (LO.bits[31] == 1) { // 檢查 LSB
            for(int i = 0; i < 32; i++) {
                inA[i] = HI.bits[i];
                inB[i] = multiplicand[i];
            }
            ALU(alu_ctrl, inA, inB, out);
            for(int i = 0; i < 32; i++) HI.bits[i] = out[i];
        }

        // 串聯右移：HI 的最低位移入 LO 的最高位
        bool hi_lowest_bit = HI.bits[31];
        shift_right_32(HI.bits);
        shift_right_32(LO.bits);
        LO.bits[0] = hi_lowest_bit;
    }

    printf("mult R%d, R%d (ALU Multiplier completed. LO value: %d)\n", rs_idx, rt_idx, toint_signed_from_int(LO.bits, 32));
}

void div_mips(bool* rs, bool* rt) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);

    bool divisor[32];
    for(int i = 0; i < 32; i++) {
        divisor[i] = reg[rt_idx].bits[i];
        HI.bits[i] = 0;
        LO.bits[i] = reg[rs_idx].bits[i];
    }

    if (toint_signed_from_int(divisor, 32) == 0) {
        fprintf(stderr, "not accept to divide with 0\n");
        return;
    }

    bool alu_add[3] = {0, 1, 0};
    bool alu_sub[3] = {1, 1, 0};
    bool inA[32], inB[32], out[32];

    for (int step = 0; step < 32; ++step) {
        bool lo_highest_bit = LO.bits[0];
        shift_left_32(HI.bits, lo_highest_bit);
        shift_left_32(LO.bits, 0);

        for(int i = 0; i < 32; i++) {
            inA[i] = HI.bits[i];
            inB[i] = divisor[i];
        }
        ALU(alu_sub, inA, inB, out);

        if (out[0] == 0) {
            for(int i = 0; i < 32; i++) HI.bits[i] = out[i];
            LO.bits[31] = 1;
        } else {
            for(int i = 0; i < 32; i++) {
                inA[i] = out[i];
                inB[i] = divisor[i];
            }
            ALU(alu_add, inA, inB, out);
            for(int i = 0; i < 32; i++) HI.bits[i] = out[i];
            LO.bits[31] = 0;
        }
    }

    printf("div R%d, R%d (ALU Divider completed. Quotient LO: %d, Remainder HI: %d)\n",
           rs_idx, rt_idx, toint_signed_from_int(LO.bits, 32), toint_signed_from_int(HI.bits, 32));
}

void jr(bool* rs) {
    int rs_idx = toint(rs, 5);
    int target_pc = toint_signed_from_int(reg[rs_idx].bits, 32);

    printf("jr R%d -> jump to address(instr line number): %d\n", rs_idx, target_pc);
    pc_code = target_pc;
}

bool beq(bool* rs, bool* rt, bool* immed) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int offset = toint_signed(immed, 16);

    bool *a = reg[rs_idx].bits;
    bool *b = reg[rt_idx].bits;
    bool isEqual = true;

    for(int i = 0; i < 32; i++) {
        if(a[i] ^ b[i]) {
            isEqual = false;
            break;
        }
    }

    printf("beq compare R%d and R%d -> (result: %d)\n", rs_idx, rt_idx, isEqual);
    if(isEqual) {
        pc_code += offset;
        return true;
    }
    return false;
}

bool bne(bool* rs, bool* rt, bool* immed) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int offset = toint_signed(immed, 16);

    bool *a = reg[rs_idx].bits;
    bool *b = reg[rt_idx].bits;
    bool isNotEqual = false;

    for(int i = 0; i < 32; i++) {
        if(a[i] ^ b[i]) {
            isNotEqual = true;
            break;
        }
    }

    printf("bne compare R%d and R%d -> (result: %d)\n", rs_idx, rt_idx, isNotEqual);
    if(isNotEqual) {
        pc_code += offset;
        return true;
    }
    return false;
}

void lw(bool* rs, bool* rt, bool* immed) {
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

void addi(bool* rs, bool* rt, bool* immed) {
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

void slt(bool* rs, bool* rt, bool* rd) {
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

void sw(bool* rs, bool* rt, bool* immed) {
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

void Rformat(bool* opcode, bool* rs, bool* rt, bool* rd, bool* shamt, bool* funct) {
    int code = toint(funct, 6);
    switch(code) {
        case 8:  jr(rs); break;
        case 24: mult(rs, rt); break;
        case 26: div_mips(rs, rt); break;
        case 32: add(rs, rt, rd); break;
        case 34: sub(rs, rt, rd); break;
        case 36: and_mips(rs, rt, rd); break;
        case 37: or_mips(rs, rt, rd); break;
        case 42: slt(rs, rt, rd); break;
        default:
            printf("unknown R-format funct code: %d\n", code);
            break;
    }
}

void Jformat(bool* opcode, bool* target) {
    int op = toint(opcode, 6);
    int target_address = toint(target, 26);

    switch(op) {
        case 2:
            printf("j -> absolute jump to target address(instr line number): %d\n", target_address);
            pc_code = target_address;
            break;

        case 3:
        {
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

        default:
            printf("unknown J-format opcode: %d\n", op);
            break;
    }
}

bool Iformat(bool* opcode, bool* rs, bool* rt, bool* immed) {
    int code = toint(opcode, 6);
    switch(code) {
        case 4:  return beq(rs, rt, immed);
        case 5:  return bne(rs, rt, immed);
        case 35: lw(rs, rt, immed); break;
        case 43: sw(rs, rt, immed); break;
        case 8:  addi(rs, rt, immed); break;
        default:
            printf("unknown I-format opcode: %d\n", code);
            break;
    }
    return false;
}

int main() {
    for(int i = 0; i < 32; i++) {
        for(int j = 0; j < 32; j++) reg[i].bits[j] = 0;
    }

    for(int i = 0; i < 32; i++) {
        HI.bits[i] = 0;
        LO.bits[i] = 0;
    }

    for(int i = 0; i < 1024; i++) data_mem[i] = 0;

    data_mem[8] = 99;

    reg[1].bits[28] = 1; reg[1].bits[30] = 1; // R1 = 10
    reg[2].bits[30] = 1; reg[2].bits[31] = 1; // R2 = 3

    const char* filename = "input.bin";
    int total_bits = 0;
    bool* bits = readBinToPtrBool(filename, &total_bits);

    if (bits == NULL) {
        printf("failed to load file\n");
        return 1;
    }

    int total_instructions = total_bits / 32;
    printf("success to load file, include with %d commands\n", total_instructions);

    pc_code = 0;
    while(pc_code >= 0 && pc_code * 32 < total_bits) {
        bool command[32];
        for(int i = 0; i < 32; i++) {
            command[i] = bits[i + pc_code * 32];
        }

        int op_type = toint(command, 6);
        bool is_jumped = false;

        if(op_type == 0) {
            int funct_code = toint(command + 26, 6);
            if (funct_code == 8) {
                is_jumped = true;
            }
            Rformat(command, command + 6, command + 11, command + 16, command + 21, command + 26);
        }
        else if(op_type == 2 || op_type == 3) {
            is_jumped = true;
            Jformat(command, command + 6);
        }
        else {
            // I-format 執行，如果分支條件成立，回傳值會是 true 代表已經手動改過 PC 目的地了。
            is_jumped = Iformat(command, command + 6, command + 11, command + 16);
        }

        if (!is_jumped) {
            pc_code++;
        }
    }

    dump_registers();
    free(bits);

    return 0;
}
