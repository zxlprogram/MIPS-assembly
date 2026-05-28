#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <bitset>
#include <cstdio>
#include<algorithm>
#include <sstream>
#include <map>
#include <cstdlib>
#include <queue>

using namespace std;
class MipsAssembler {
private:
    map<string, int> regMap;
    map<string, pair<string, int>> opMap;
    string cleanToken(string s) {
        s.erase(remove(s.begin(), s.end(), ','), s.end());
        s.erase(remove(s.begin(), s.end(), '$'), s.end());
        s.erase(remove(s.begin(), s.end(), '('), s.end());
        s.erase(remove(s.begin(), s.end(), ')'), s.end());
        return s;
    }
    void initMaps() {
        regMap["zero"] = 0; regMap["0"] = 0;
        regMap["v0"] = 2;   regMap["v1"] = 3;
        for (int i = 0; i < 4; i++) regMap["a" + to_string(i)] = 4 + i;
        for (int i = 0; i < 8; i++) regMap["t" + to_string(i)] = 8 + i;
        for (int i = 0; i < 8; i++) regMap["s" + to_string(i)] = 16 + i;
        regMap["sp"] = 29;  regMap["ra"] = 31;

        // R-format
        opMap["add"]     = {"R", 32};
        opMap["addu"]    = {"R", 33};
        opMap["sub"]     = {"R", 34};
        opMap["and"]     = {"R", 36};
        opMap["or"]      = {"R", 37};
        opMap["slt"]     = {"R", 42};
        opMap["jr"]      = {"R", 8};
        opMap["syscall"] = {"R", 12};

        // I-format & J-format
        opMap["beq"]  = {"I", 4};  opMap["bne"]  = {"I", 5};
        opMap["addi"] = {"I", 8};  opMap["addiu"]= {"I", 9};
        opMap["lui"]  = {"I", 15};
        opMap["lw"]   = {"I", 35}; opMap["sw"]   = {"I", 43};
        opMap["j"]    = {"J", 2};  opMap["jal"]  = {"J", 3};
    }

    string toBinary(int value, int bits) {
        if (value < 0) {
            unsigned int uValue = value;
            return bitset<32>(uValue).to_string().substr(32 - bits);
        }
        return bitset<32>(value).to_string().substr(32 - bits);
    }

    string binToHex(string bin) {
        stringstream ss;
        for (int i = 0; i < 32; i += 4) {
            int value = bitset<4>(bin.substr(i, 4)).to_ulong();
            ss << hex << uppercase << value;
        }
        return ss.str();
    }

    int getRegNum(string regStr) {
        regStr = cleanToken(regStr);
        if (regMap.find(regStr) != regMap.end()) return regMap[regStr];
        return stoi(regStr);
    }

public:
    MipsAssembler() { initMaps(); }

    string assembleLine(string line) {
        replace(line.begin(), line.end(), '(', ' ');
        replace(line.begin(), line.end(), ')', ' ');

        stringstream ss(line);
        string inst;
        ss >> inst;

        if (opMap.find(inst) == opMap.end()) {
            return "ERROR: Unknown Instruction [" + inst + "]";
        }

        string type = opMap[inst].first;
        int code = opMap[inst].second;
        string binCode = "";

        if (type == "R") {
            int opcode = 0;
            int rs = 0, rt = 0, rd = 0, shamt = 0;
            int funct = code;

            if (inst == "syscall") {
                // syscall 不需要後續參數，中間 20 bits 補 0 即可
                rs = 0; rt = 0; rd = 0; shamt = 0;
            } else if (inst == "jr") {
                string regRs; ss >> regRs;
                rs = getRegNum(regRs);
            } else {
                string regRd, regRs, regRt;
                ss >> regRd >> regRs >> regRt;
                rd = getRegNum(regRd);
                rs = getRegNum(regRs);
                rt = getRegNum(regRt);
            }
            binCode = toBinary(opcode, 6) + toBinary(rs, 5) + toBinary(rt, 5) +
                      toBinary(rd, 5) + toBinary(shamt, 5) + toBinary(funct, 6);

        } else if (type == "I") {
            int opcode = code;
            int rs = 0, rt = 0, imm = 0;

            if (inst == "lw" || inst == "sw") {
                string regRt, immStr, regRs;
                ss >> regRt >> immStr >> regRs;
                rt = getRegNum(regRt);
                imm = stoi(immStr);
                rs = getRegNum(regRs);
            } else if (inst == "lui") {
                string regRt, immStr;
                ss >> regRt >> immStr;
                rt = getRegNum(regRt);
                imm = stoi(immStr);
                rs = 0;
            } else if (inst == "beq" || inst == "bne") {
                string regRs, regRt, immStr;
                ss >> regRs >> regRt >> immStr;
                rs = getRegNum(regRs);
                rt = getRegNum(regRt);
                imm = stoi(immStr);
            } else {
                string regRt, regRs, immStr;
                ss >> regRt >> regRs >> immStr;
                rt = getRegNum(regRt);
                rs = getRegNum(regRs);
                imm = stoi(immStr);
            }
            binCode = toBinary(opcode, 6) + toBinary(rs, 5) + toBinary(rt, 5) + toBinary(imm, 16);

        } else if (type == "J") {
            int opcode = code;
            string targetStr; ss >> targetStr;
            int target = stoi(cleanToken(targetStr));
            binCode = toBinary(opcode, 6) + toBinary(target, 26);
        }

        return binToHex(binCode);
    }
};

class MipsSimulator {
    private:
    struct Register32 {
        bool bits[32] = {false};
    };

    Register32 reg[32];
    Register32 HI;
    Register32 LO;
    Register32 PC;
    int data_mem[1024];

    // --- 虛擬記憶體分頁架構資料結構 ---
    struct PageTableEntry {
        bool valid = false;
        int pfn = -1; // Physical Frame Number
    };

    static const int PAGE_SIZE = 128;       // 每個 Page 有 128 個 Word 元素 (128 * 4 = 512 Bytes)
    static const int NUM_FRAMES = 8;        // 實體記憶體頁框數: 1024 / 128 = 8 個 Frames
    static const int NUM_PAGES = 64;        // 虛擬記憶體頁面數: 擴展至 64 個 Pages (總共 8192 Words 虛擬空間)

    PageTableEntry page_table[NUM_PAGES];
    int disk_swap[NUM_PAGES][PAGE_SIZE];    // 模擬磁碟置換空間 (Swap Space)
    bool frame_allocated[NUM_FRAMES];       // 記錄實體頁框使用狀態
    queue<int> fifo_queue;                  // FIFO 頁面替換演算法佇列

    void init_virtual_memory() {
        for (int i = 0; i < NUM_PAGES; i++) {
            page_table[i].valid = false;
            page_table[i].pfn = -1;
            for (int j = 0; j < PAGE_SIZE; j++) {
                disk_swap[i][j] = 0;
            }
        }
        for (int i = 0; i < NUM_FRAMES; i++) {
            frame_allocated[i] = false;
        }
        while (!fifo_queue.empty()) fifo_queue.pop();
    }

    // 記憶體管理單元 (MMU)：虛擬地址翻譯成實體地址 + 缺頁中斷處理
    int translate_address(int virtual_address) {
        if (virtual_address < 0 || virtual_address >= (NUM_PAGES * PAGE_SIZE)) {
            fprintf(stderr, "Virtual Memory Segmentation Fault: Address %d out of bound!\n", virtual_address);
            return -1;
        }

        int vpn = virtual_address / PAGE_SIZE;    // 計算第幾頁 (Virtual Page Number)
        int offset = virtual_address % PAGE_SIZE; // 計算頁內偏移量 (Offset)

        // 檢查是否發生 Page Fault
        if (!page_table[vpn].valid) {
            printf("[Page Fault] Virtual Page %d is not in Physical Memory. Resolving...\n", vpn);
            int allocated_frame = -1;

            // 1. 尋找是否有空閒的實體頁框
            for (int i = 0; i < NUM_FRAMES; i++) {
                if (!frame_allocated[i]) {
                    allocated_frame = i;
                    frame_allocated[i] = true;
                    break;
                }
            }

            // 2. 如果實體記憶體已滿，執行 FIFO 頁面置換演算法
            if (allocated_frame == -1) {
                int victim_vpn = fifo_queue.front();
                fifo_queue.pop();
                allocated_frame = page_table[victim_vpn].pfn;

                printf("[Page Replacement] Physical Frame %d is full. Swapping out Victim Page %d to Disk.\n", allocated_frame, victim_vpn);

                // 將受害者頁面的資料從實體記憶體寫回 Disk Swap Space
                for (int i = 0; i < PAGE_SIZE; i++) {
                    disk_swap[victim_vpn][i] = data_mem[allocated_frame * PAGE_SIZE + i];
                }
                page_table[victim_vpn].valid = false;
                page_table[victim_vpn].pfn = -1;
            }

            // 3. 將需要的頁面從 Disk 載入到被分配的實體頁框中
            page_table[vpn].valid = true;
            page_table[vpn].pfn = allocated_frame;
            for (int i = 0; i < PAGE_SIZE; i++) {
                data_mem[allocated_frame * PAGE_SIZE + i] = disk_swap[vpn][i];
            }
            fifo_queue.push(vpn);
            printf("[Page Loaded] Virtual Page %d successfully mapped to Physical Frame %d.\n", vpn, allocated_frame);
        }

        // 返回對應的實體記憶體陣列索引
        return (page_table[vpn].pfn * PAGE_SIZE) + offset;
    }

    string transHexFormat(string command) {
        string ret="";
        for(int i=0;i<command.length();i++) {
            if(command[i]!=' ')
                ret+=bitset<4>(command[i]-(command[i]>='A'?(command[i]>='a'?-'a'+'A'-10:'A'-10):'0')).to_string();
        }
        return ret;
    }

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
    void int_to_register32(int value, Register32& target_reg) {
        for (int i = 31; i >= 0; --i) {
            target_reg.bits[i] = (value >> (31 - i)) & 1;
        }
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
        if (reg_idx == 0) return; // R0 read-only
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

    void addu(const bool* rs, const bool* rt, const bool* rd) {
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

    void syscall() {
        int v0_val = toint_signed_from_int(reg[2].bits, 32);
        printf("syscall triggered (R2/v0 = %d): ", v0_val);

        switch (v0_val) {
            case 1:
                printf("Print Integer: %d\n", toint_signed_from_int(reg[4].bits, 32));
                break;
            case 10:
                printf("Exit Program.\n");
                // 用全 1 表示非法終止狀態
                for(int i=0; i<32; i++) PC.bits[i] = true;
                break;
            default:
                printf("Unsupported syscall service code.\n");
                break;
        }
    }

    void jr(const bool* rs) {
        int rs_idx = toint(rs, 5);
        printf("jr R%d -> jump to address(instr line number): %d\n", rs_idx, toint_signed_from_int(reg[rs_idx].bits, 32));
        for(int i=0; i<32; i++) {
            PC.bits[i] = reg[rs_idx].bits[i];
        }
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
            int current_pc = toint_signed(PC.bits, 32);
            int_to_register32(current_pc + offset, PC);
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
            int current_pc = toint_signed(PC.bits, 32);
            int_to_register32(current_pc + offset, PC);
            return true;
        }
        return false;
    }

    void lw(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5);
        int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);

        int base_val = toint_signed_from_int(reg[rs_idx].bits, 32);
        int virtual_address = base_val + offset;

        // 經由 MMU 頁表映射翻譯為實際的實體記憶體索引
        int physical_index = translate_address(virtual_address);
        if (physical_index == -1) return;

        int loaded_value = data_mem[physical_index];
        bool temp[32];
        for (int i = 31; i >= 0; --i) {
            temp[i] = (loaded_value >> (31 - i)) & 1;
        }
        write_register(rt_idx, temp);
        printf("lw R%d, %d(R%d) -> [Virtual Address: %d -> Physical Index: %d] load value: %d\n", rt_idx, offset, rs_idx, virtual_address, physical_index, loaded_value);
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

    void addiu(const bool* rs, const bool* rt, const bool* immed) {
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
        printf("addiu R%d, R%d, %d (result: %d)\n", rt_idx, rs_idx, immediate, result);
    }

    void lui(const bool* rt, const bool* immed) {
        int rt_idx = toint(rt, 5);
        int immediate = toint(immed, 16);

        bool temp[32] = {false};
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
        int virtual_address = base_val + offset;

        // 經由 MMU 頁表映射翻譯為實際的實體記憶體索引
        int physical_index = translate_address(virtual_address);
        if (physical_index == -1) return;

        int value_to_store = toint_signed_from_int(reg[rt_idx].bits, 32);
        data_mem[physical_index] = value_to_store;
        printf("sw R%d, %d(R%d) -> [Virtual Address: %d -> Physical Index: %d] store value: %d into data_mem\n", rt_idx, offset, rs_idx, virtual_address, physical_index, value_to_store);
    }

    void Rformat(const bool* command) {
        int code = toint(command + 26, 6);
        switch(code) {
            case 8:  jr(command + 6); break;
            case 12: syscall(); break;
            case 24: mult(command + 6, command + 11); break;
            case 26: div_mips(command + 6, command + 11); break;
            case 32: add(command + 6, command + 11, command + 16); break;
            case 33: addu(command + 6, command + 11, command + 16); break;
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
                int_to_register32(target_address, PC);
                break;
            case 3: {
                printf("jal -> record the returned address to R31 and jump to the location: %d\n", target_address);
                int current_pc = toint_signed(PC.bits, 32);
                int next_instruction = current_pc + 1;

                bool temp[32];
                for (int i = 31; i >= 0; --i) {
                    temp[i] = (next_instruction >> (31 - i)) & 1;
                }
                write_register(31, temp);
                int_to_register32(target_address, PC);
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
            case 9:  addiu(command + 6, command + 11, command + 16); break;
            case 15: lui(command + 11, command + 16); break;
            case 35: lw(command + 6, command + 11, command + 16); break;
            case 43: sw(command + 6, command + 11, command + 16); break;
            default: printf("unknown I-format opcode: %d\n", code); break;
        }
        return false;
    }

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
    MipsSimulator() {
        ret0(PC.bits); // PC 初始化歸零
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 32; j++) reg[i].bits[j] = false;
        }
        ret0(HI.bits);
        ret0(LO.bits);
        for (int i = 0; i < 1024; i++) data_mem[i] = 0;

        // 初始化虛擬記憶體分頁表與交換空間
        init_virtual_memory();

        // 預設測試資料 (此處寫入虛擬空間或對應初始預載入)
        // 為了讓原本測試正常，我們直接把預設資料存放到對應的虛擬地址空間中
        // 原本暫存器預設：R29(sp) = 0, 所以 8($sp) 是虛擬地址 8
        // 虛擬地址 8 屬於 Page 0。我們先將對應的磁碟空間預設為 99
        disk_swap[0][8] = 99;

        reg[1].bits[28] = true; reg[1].bits[30] = true; // R1 = 10
        reg[2].bits[30] = true; reg[2].bits[31] = true; // R2 = 3
    }

    void command(string machineCode) {
        string formatted=transHexFormat(machineCode);
        if(formatted.length()==32)
            machineCode=formatted;
        if(machineCode.length() != 32) {
            cerr << "Error: Machine code must be exactly 32 bits long!" << endl;
            return;
        }

        bool cmdBits[32];
        for (int i = 0; i < 32; ++i) {
            cmdBits[i] = (machineCode[i] == '1');
        }

        executeSingleCommand(cmdBits);
    }

    void multicommand(string machineCode) {
        string formatted="";
        for(char c:machineCode)
            if((c>='0'&&c<='9')||(c>='A'&&c<='F'))
                formatted+=c;
        machineCode=formatted;
        for(int it=0;it<machineCode.length();it+=8) {
            string singleMachineCode="";
            for(int i=0;i<8;i++) {
                singleMachineCode+=machineCode[it+i];
            }
            bool cmdBits[32];
            string bitCode=transHexFormat(singleMachineCode);
            for(int i=0;i<32;++i) {
                cmdBits[i]=(bitCode[i]=='1');
            }
            executeSingleCommand(cmdBits);
        }
    }

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

        ret0(PC.bits);

        while (toint_signed(PC.bits, 32) >= 0 && toint_signed(PC.bits, 32) * 32 < total_bits) {
            int current_pc_val = toint_signed(PC.bits, 32);
            bool current_command[32];
            for (int i = 0; i < 32; i++) {
                current_command[i] = bits[i + current_pc_val * 32];
            }

            bool is_jumped = executeSingleCommand(current_command);
            if (!is_jumped) {
                int_to_register32(toint_signed(PC.bits, 32) + 1, PC);
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
        int pc_val = toint_signed(PC.bits, 32);
        printf("HI: %d\tLO: %d\tPC: %d\n", hi_val, lo_val, pc_val);
        printf("=======================================================\n\n");
    }

    void reset() {
        for(Register32 &i:reg) {
            ret0(i.bits);
        }
        ret0(HI.bits);
        ret0(LO.bits);
        ret0(PC.bits);
        init_virtual_memory();
    }
};

int main() {
    MipsSimulator sim;
    MipsAssembler mipasm;
    vector<string> assemblyCode = {
        "addi $t0, $zero, 7",
        "addi $s0, $zero, 0",
        "addi $s1, $zero, 1",
        "addi $t1, $zero, 2",
        "add $t1, $t1, $s0",
        "beq $t0, $s0, 7",
        "lw $v0, 8($sp)",
        "sw $v0, 12($sp)",
        "jr $ra",
        "j 10"
    };
    for (const string& asmLine : assemblyCode) {
        string hexCode = mipasm.assembleLine(asmLine);
        sim.multicommand(hexCode);
    }
    sim.dump_registers();
    sim.reset();
    sim.multicommand("24 08 00 07 24 10 00 00 24 11 00 01 24 09 00 02 01 09 50 2A 15 40 00 07 02 30 90 21 02 20 80 21 02 40 88 21 24 89 00 01 02 30 90 21 02 20 80 21 02 40 88 21 25 29 00 01 02 30 90 21 02 20 80 21 02 40 88 21 25 29 00 01 02 30 90 21 02 20 80 21 02 40 88 21 25 29 00 01 02 30 90 21 02 20 80 21 02 40 88 21 25 29 00 01 02 30 90 21 02 20 80 21 02 40 88 21 25 29 00 01 24 02 00 01 02 40 20 21 00 00 00 0C 24 02 00 0A 00 00 00 0C");
    sim.dump_registers();
    return 0;
}
