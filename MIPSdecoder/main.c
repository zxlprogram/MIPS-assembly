#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static int **reg;
static int *HI;
static int *LO;
static int *data_mem;
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

// 供 command (bool 陣列) 使用的轉換
int toint(bool* r, int length) {
    int ret = 0;
    for(int i = 0; i < length; ++i) {
        ret = (ret << 1) | r[i];
    }
    return ret;
}

// 供 command (bool 陣列) 使用的有號轉換
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

int toint_signed_from_int(int* r, int length) {
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

// 32-bit 邏輯右移 (補 0)
void shift_right_32(int* r) {
    for (int i = 31; i > 0; --i) {
        r[i] = r[i - 1];
    }
    r[0] = 0;
}

// 32-bit 左移 (最低位補帶入的 bit)
void shift_left_32(int* r, bool fill_bit) {
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

// ==================== 32-bit 串聯 ALU ====================
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
        int val = toint_signed_from_int(reg[i], 32);
        printf("R%d: %d\t", i, val);
        if ((i + 1) % 4 == 0) {
            printf("\n");
        }
    }
    int hi_val = toint_signed_from_int(HI, 32);
    int lo_val = toint_signed_from_int(LO, 32);
    printf("HI: %d\tLO: %d\tPC: %d\n", hi_val, lo_val, pc_code);
    printf("=======================================================\n\n");
}

// 統一的暫存器寫入埠，加入 R0 保護機制
void write_register(int reg_idx, bool* data) {
    if (reg_idx == 0) return; // R0 唯讀保護，不允許寫入
    for(int i = 0; i < 32; i++) {
        reg[reg_idx][i] = data[i];
    }
}

void add(bool* rs, bool* rt, bool* rd) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(rd, 5);

    bool inA[32], inB[32], out[32];
    for(int i = 0; i < 32; i++) {
        inA[i] = reg[rs_idx][i];
        inB[i] = reg[rt_idx][i];
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
        inA[i] = reg[rs_idx][i];
        inB[i] = reg[rt_idx][i];
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
        inA[i] = reg[rs_idx][i];
        inB[i] = reg[rt_idx][i];
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
        inA[i] = reg[rs_idx][i];
        inB[i] = reg[rt_idx][i];
    }

    bool alu_ctrl[3] = {0, 0, 1};
    ALU(alu_ctrl, inA, inB, out);

    write_register(rd_idx, out);
    printf("or R%d, R%d, R%d (result: %d)\n", rd_idx, rs_idx, rt_idx, toint_signed(out, 32));
}

void mult(bool* rs, bool* rt) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);

    int multiplicand[32];
    for(int i = 0; i < 32; i++) {
        multiplicand[i] = reg[rs_idx][i];
        HI[i] = 0;
        LO[i] = reg[rt_idx][i];
    }

    bool alu_ctrl[3] = {0, 1, 0};
    bool inA[32], inB[32], out[32];

    for (int step = 0; step < 32; ++step) {
        if (LO[31] == 1) {
            for(int i = 0; i < 32; i++) {
                inA[i] = HI[i];
                inB[i] = multiplicand[i];
            }
            ALU(alu_ctrl, inA, inB, out);
            for(int i = 0; i < 32; i++) HI[i] = out[i];
        }

        bool hi_lowest_bit = HI[31];
        shift_right_32(HI);
        shift_right_32(LO);
        LO[0] = hi_lowest_bit;
    }

    printf("mult R%d, R%d (ALU Multiplier completed. LO value: %d)\n", rs_idx, rt_idx, toint_signed_from_int(LO, 32));
}

void div_mips(bool* rs, bool* rt) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);

    int divisor[32];
    for(int i = 0; i < 32; i++) {
        divisor[i] = reg[rt_idx][i];
        HI[i] = 0;
        LO[i] = reg[rs_idx][i];
    }

    if (toint_signed_from_int(divisor, 32) == 0) {
        fprintf(stderr, "not accept to divide with 0\n");
        return;
    }

    bool alu_add[3] = {0, 1, 0};
    bool alu_sub[3] = {1, 1, 0};
    bool inA[32], inB[32], out[32];

    for (int step = 0; step < 32; ++step) {
        bool lo_highest_bit = LO[0];
        shift_left_32(HI, lo_highest_bit);
        shift_left_32(LO, 0);

        for(int i = 0; i < 32; i++) {
            inA[i] = HI[i];
            inB[i] = divisor[i];
        }
        ALU(alu_sub, inA, inB, out);

        if (out[0] == 0) {
            for(int i = 0; i < 32; i++) HI[i] = out[i];
            LO[31] = 1;
        } else {
            for(int i = 0; i < 32; i++) {
                inA[i] = out[i];
                inB[i] = divisor[i];
            }
            ALU(alu_add, inA, inB, out);
            for(int i = 0; i < 32; i++) HI[i] = out[i];
            LO[31] = 0;
        }
    }

    printf("div R%d, R%d (ALU Divider completed. Quotient LO: %d, Remainder HI: %d)\n",
           rs_idx, rt_idx, toint_signed_from_int(LO, 32), toint_signed_from_int(HI, 32));
}

// 實作 jr (Jump Register)
void jr(bool* rs) {
    int rs_idx = toint(rs, 5);
    int target_pc = toint_signed_from_int(reg[rs_idx], 32);

    printf("jr R%d -> jump to address(instr line number): %d\n", rs_idx, target_pc);
    pc_code = target_pc; // 直接修改暫存器變數控制硬體跳躍
}

void beq(bool* rs, bool* rt, bool* immed) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(immed, 5);

    int *a = reg[rs_idx];
    int *b = reg[rt_idx];
    bool isEqual = true;

    for(int i = 0; i < 32; i++) {
        if(a[i] ^ b[i]) {
            isEqual = false;
            break;
        }
    }

    bool temp_rd[32];
    if(isEqual) ret1(temp_rd);
    else ret0(temp_rd);

    write_register(rd_idx, temp_rd);
    printf("beq compare R%d and R%d -> load result to R%d (result: %d)\n", rs_idx, rt_idx, rd_idx, isEqual);
}

void bne(bool* rs, bool* rt, bool* immed) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int rd_idx = toint(immed, 5);

    int *a = reg[rs_idx];
    int *b = reg[rt_idx];
    bool isNotEqual = false;

    for(int i = 0; i < 32; i++) {
        if(a[i] ^ b[i]) {
            isNotEqual = true;
            break;
        }
    }

    bool temp_rd[32];
    if(isNotEqual) ret1(temp_rd);
    else ret0(temp_rd);

    write_register(rd_idx, temp_rd);
    printf("bne compare R%d and R%d -> load result to R%d (result: %d)\n", rs_idx, rt_idx, rd_idx, isNotEqual);
}

void lw(bool* rs, bool* rt, bool* immed) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int offset = toint_signed(immed, 16);

    int base_val = toint_signed_from_int(reg[rs_idx], 32);
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

void sw(bool* rs, bool* rt, bool* immed) {
    int rs_idx = toint(rs, 5);
    int rt_idx = toint(rt, 5);
    int offset = toint_signed(immed, 16);

    int base_val = toint_signed_from_int(reg[rs_idx], 32);
    int mem_address = base_val + offset;

    if (mem_address < 0 || mem_address >= 1024) {
        fprintf(stderr, "sw segmentation error: %d\n", mem_address);
        return;
    }

    int value_to_store = toint_signed_from_int(reg[rt_idx], 32);
    data_mem[mem_address] = value_to_store;
    printf("sw R%d, %d(R%d) -> store value: %d into data_mem[%d]\n", rt_idx, offset, rs_idx, value_to_store, mem_address);
}

void Rformat(bool* opcode, bool* rs, bool* rt, bool* rd, bool* shamt, bool* funct) {
    int code = toint(funct, 6);
    switch(code) {
        case 8:  jr(rs); break; // 融合了 funct code 8 轉發給 jr
        case 24: mult(rs, rt); break;
        case 26: div_mips(rs, rt); break;
        case 32: add(rs, rt, rd); break;
        case 34: sub(rs, rt, rd); break;
        case 36: and_mips(rs, rt, rd); break;
        case 37: or_mips(rs, rt, rd); break;
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

void Iformat(bool* opcode, bool* rs, bool* rt, bool* immed) {
    int code = toint(opcode, 6);
    switch(code) {
        case 4:  beq(rs, rt, immed); break;
        case 5:  bne(rs, rt, immed); break;
        case 35: lw(rs, rt, immed); break;
        case 43: sw(rs, rt, immed); break;
        default:
            printf("unknown I-format opcode: %d\n", code);
            break;
    }
}

int main() {
    reg = (int**)malloc(32 * sizeof(int*));
    for(int i = 0; i < 32; i++) {
        reg[i] = (int*)malloc(32 * sizeof(int));
        for(int j = 0; j < 32; j++) reg[i][j] = 0;
    }

    HI = (int*)malloc(32 * sizeof(int));
    LO = (int*)malloc(32 * sizeof(int));
    for(int i = 0; i < 32; i++) HI[i] = LO[i] = 0;

    data_mem = (int*)malloc(1024 * sizeof(int));
    for(int i = 0; i < 1024; i++) data_mem[i] = 0;
    data_mem[8] = 99;

    reg[1][28] = 1; reg[1][30] = 1; // R1 = 10
    reg[2][30] = 1; reg[2][31] = 1; // R2 = 3

    const char* filename = "input.bin";
    int total_bits = 0;
    bool* bits = readBinToPtrBool(filename, &total_bits);

    if (bits == NULL) {
        printf("failed to load file\n");
        for(int i = 0; i < 32; i++) free(reg[i]);
        free(reg); free(HI); free(LO); free(data_mem);
        return 1;
    }

    int total_instructions = total_bits / 32;
    printf("success to load file, include with %d commands\n", total_instructions);

    pc_code = 0;
    while(pc_code * 32 < total_bits) {
        bool command[32];
        for(int i = 0; i < 32; i++) {
            command[i] = bits[i + pc_code * 32];
        }

        int op_type = toint(command, 6);

        // 核心修正：動態判定這個指令是否會自己改寫 PC 控制流
        bool is_jumped = (op_type == 2 || op_type == 3);

        if(op_type == 0) {
            // 如果是 R-format，進一步檢查是否為 jr (funct code 為 8)
            int funct_code = toint(command + 26, 6);
            if (funct_code == 8) {
                is_jumped = true;
            }
            Rformat(command, command + 6, command + 11, command + 16, command + 21, command + 26);
        }
        else if(is_jumped) {
            Jformat(command, command + 6);
        }
        else {
            Iformat(command, command + 6, command + 11, command + 16);
        }

        // 只有非跳躍指令，才循序執行下一行
        if (!is_jumped) {
            pc_code++;
        }
    }

    dump_registers();

    // 清理資源
    free(bits);
    for(int i = 0; i < 32; i++) free(reg[i]);
    free(reg); free(HI); free(LO); free(data_mem);

    return 0;
}
