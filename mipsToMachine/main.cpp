#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <bitset>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <map>
#include <cstdlib>

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

        opMap["add"]     = {"R", 32};
        opMap["addu"]    = {"R", 33};
        opMap["sub"]     = {"R", 34};
        opMap["and"]     = {"R", 36};
        opMap["or"]      = {"R", 37};
        opMap["slt"]     = {"R", 42};
        opMap["jr"]      = {"R", 8};
        opMap["syscall"] = {"R", 12};

        opMap["beq"]  = {"I", 4};  opMap["bne"]  = {"I", 5};
        opMap["addi"] = {"I", 8};  opMap["addiu"]= {"I", 9};
        opMap["lui"]  = {"I", 15};
        opMap["lw"]   = {"I", 35}; opMap["sw"]   = {"I", 43};
        opMap["j"]    = {"J", 2};  opMap["jal"]  = {"J", 3};
    }

    string toBinary(int value, int bits) {
        unsigned int uValue = value;
        return bitset<32>(uValue).to_string().substr(32 - bits);
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

        if (opMap.find(inst) == opMap.end()) return "";

        string type = opMap[inst].first;
        int code = opMap[inst].second;
        string binCode = "";

        if (type == "R") {
            int opcode = 0;
            int rs = 0, rt = 0, rd = 0, shamt = 0;
            int funct = code;

            if (inst == "syscall") {
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
    vector<string> instr_mem;

    // 新增：硬體運行狀態旗標，用來徹底防止無窮迴圈
    bool is_running;

    string transHexFormat(string command) {
        string ret = "";
        for (size_t i = 0; i < command.length(); i++) {
            if (command[i] != ' ' && command[i] != '\n' && command[i] != '\r') {
                char c = command[i];
                int val = 0;
                if (c >= '0' && c <= '9') val = c - '0';
                else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
                else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
                ret += bitset<4>(val).to_string();
            }
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

    void ret0(bool* r) {
        for (int i = 0; i < 32; i++) r[i] = false;
    }

    void ret1(bool* r) {
        for (int i = 0; i < 31; i++) r[i] = false;
        r[31] = true;
    }

    void shift_right_32(bool* r) {
        for (int i = 31; i > 0; --i) r[i] = r[i - 1];
        r[0] = false;
    }

    void shift_left_32(bool* r, bool fill_bit) {
        for (int i = 0; i < 31; ++i) r[i] = r[i + 1];
        r[31] = fill_bit;
    }

    void int_to_register32(int value, Register32& target_reg) {
        for (int i = 31; i >= 0; --i) {
            target_reg.bits[i] = (value >> (31 - i)) & 1;
        }
    }

    // --- ALU ---
    bool ALU1bit(bool a, bool b, bool cin, bool invertB, int operation, bool *cout_val) {
        bool real_b = invertB ? !b : b;
        bool sum = a ^ real_b ^ cin;
        *cout_val = (a & real_b) | (cin & (a ^ real_b));

        switch (operation) {
            case 0: return a & real_b;
            case 1: return a | real_b;
            case 2: return sum;
            default: return false;
        }
    }

    void ALU(const bool* ctrl, const bool* inputA, const bool* inputB, bool* output) {
        int controlSignal = toint(ctrl, 3);
        bool invertB = false;
        int operation = 2;

        switch (controlSignal) {
            case 0: invertB = false; operation = 0; break;
            case 1: invertB = false; operation = 1; break;
            case 2: invertB = false; operation = 2; break;
            case 6: invertB = true;  operation = 2; break;
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
        if (reg_idx == 0) return;
        for (int i = 0; i < 32; i++) reg[reg_idx].bits[i] = data[i];
    }

    // --- MIPS 指令實作 ---
    void add(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5); int rd_idx = toint(rd, 5);
        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i]; inB[i] = reg[rt_idx].bits[i];
        }
        bool alu_ctrl[3] = {false, true, false};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
    }

    void addu(const bool* rs, const bool* rt, const bool* rd) {
        add(rs, rt, rd);
    }

    void sub(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5); int rd_idx = toint(rd, 5);
        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i]; inB[i] = reg[rt_idx].bits[i];
        }
        bool alu_ctrl[3] = {true, true, false};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
    }

    void and_mips(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5); int rd_idx = toint(rd, 5);
        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i]; inB[i] = reg[rt_idx].bits[i];
        }
        bool alu_ctrl[3] = {false, false, false};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
    }

    void or_mips(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5); int rd_idx = toint(rd, 5);
        bool inA[32], inB[32], out[32];
        for (int i = 0; i < 32; i++) {
            inA[i] = reg[rs_idx].bits[i]; inB[i] = reg[rt_idx].bits[i];
        }
        bool alu_ctrl[3] = {false, false, true};
        ALU(alu_ctrl, inA, inB, out);
        write_register(rd_idx, out);
    }

    void mult(const bool* rs, const bool* rt) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        bool multiplicand[32];
        for (int i = 0; i < 32; i++) {
            multiplicand[i] = reg[rs_idx].bits[i];
            HI.bits[i] = false; LO.bits[i] = reg[rt_idx].bits[i];
        }
        bool alu_ctrl[3] = {false, true, false};
        bool inA[32], inB[32], out[32];
        for (int step = 0; step < 32; ++step) {
            if (LO.bits[31]) {
                for (int i = 0; i < 32; i++) { inA[i] = HI.bits[i]; inB[i] = multiplicand[i]; }
                ALU(alu_ctrl, inA, inB, out);
                for (int i = 0; i < 32; i++) HI.bits[i] = out[i];
            }
            bool hi_lowest_bit = HI.bits[31];
            shift_right_32(HI.bits); shift_right_32(LO.bits);
            LO.bits[0] = hi_lowest_bit;
        }
    }

    void div_mips(const bool* rs, const bool* rt) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        bool divisor[32];
        for (int i = 0; i < 32; i++) {
            divisor[i] = reg[rt_idx].bits[i];
            HI.bits[i] = false; LO.bits[i] = reg[rs_idx].bits[i];
        }
        if (toint_signed(divisor, 32) == 0) return;

        bool alu_add[3] = {false, true, false};
        bool alu_sub[3] = {true, true, false};
        bool inA[32], inB[32], out[32];

        for (int step = 0; step < 32; ++step) {
            bool lo_highest_bit = LO.bits[0];
            shift_left_32(HI.bits, lo_highest_bit); shift_left_32(LO.bits, false);
            for (int i = 0; i < 32; i++) { inA[i] = HI.bits[i]; inB[i] = divisor[i]; }
            ALU(alu_sub, inA, inB, out);
            if (out[0] == false) {
                for (int i = 0; i < 32; i++) HI.bits[i] = out[i];
                LO.bits[31] = true;
            } else {
                for (int i = 0; i < 32; i++) { inA[i] = out[i]; inB[i] = divisor[i]; }
                ALU(alu_add, inA, inB, out);
                for (int i = 0; i < 32; i++) HI.bits[i] = out[i];
                LO.bits[31] = false;
            }
        }
    }

    void syscall() {
        int v0_val = toint_signed(reg[2].bits, 32);
        if (v0_val == 1) {
            printf("[Syscall Print]: %d\n", toint_signed(reg[4].bits, 32));
        } else if (v0_val == 10) {
            printf("[Syscall Exit]: Program terminated safely.\n");
            is_running = false; // 強制關閉硬體模擬迴圈
        }
    }

    void jr(const bool* rs) {
        int rs_idx = toint(rs, 5);
        int target = toint_signed(reg[rs_idx].bits, 32);
        int_to_register32(target, PC);
    }

    bool beq(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);
        bool isEqual = true;
        for (int i = 0; i < 32; i++) {
            if (reg[rs_idx].bits[i] != reg[rt_idx].bits[i]) { isEqual = false; break; }
        }
        if (isEqual) {
            int current_pc = toint_signed(PC.bits, 32);
            int_to_register32(current_pc + 1 + offset, PC);
            return true;
        }
        return false;
    }

    bool bne(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);
        bool isNotEqual = false;
        for (int i = 0; i < 32; i++) {
            if (reg[rs_idx].bits[i] != reg[rt_idx].bits[i]) { isNotEqual = true; break; }
        }
        if (isNotEqual) {
            int current_pc = toint_signed(PC.bits, 32);
            int_to_register32(current_pc + 1 + offset, PC);
            return true;
        }
        return false;
    }

    void lw(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);
        int mem_address = toint_signed(reg[rs_idx].bits, 32) + offset;
        if (mem_address >= 0 && mem_address < 1024) {
            int loaded = data_mem[mem_address];
            for (int i = 31; i >= 0; --i) reg[rt_idx].bits[i] = (loaded >> (31 - i)) & 1;
        }
    }

    void addi(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        int immediate = toint_signed(immed, 16);
        int result = toint_signed(reg[rs_idx].bits, 32) + immediate;
        for (int i = 31; i >= 0; --i) reg[rt_idx].bits[i] = (result >> (31 - i)) & 1;
    }

    void addiu(const bool* rs, const bool* rt, const bool* immed) {
        addi(rs, rt, immed);
    }

    void lui(const bool* rt, const bool* immed) {
        int rt_idx = toint(rt, 5);
        bool temp[32] = {false};
        for (int i = 0; i < 16; ++i) temp[i] = immed[i];
        write_register(rt_idx, temp);
    }

    void slt(const bool* rs, const bool* rt, const bool* rd) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5); int rd_idx = toint(rd, 5);
        bool temp[32];
        if (toint_signed(reg[rs_idx].bits, 32) < toint_signed(reg[rt_idx].bits, 32)) ret1(temp);
        else ret0(temp);
        write_register(rd_idx, temp);
    }

    void sw(const bool* rs, const bool* rt, const bool* immed) {
        int rs_idx = toint(rs, 5); int rt_idx = toint(rt, 5);
        int offset = toint_signed(immed, 16);
        int mem_address = toint_signed(reg[rs_idx].bits, 32) + offset;
        if (mem_address >= 0 && mem_address < 1024) {
            data_mem[mem_address] = toint_signed(reg[rt_idx].bits, 32);
        }
    }

    bool executeSingleCommand(const bool* command) {
        int op_type = toint(command, 6);
        bool is_jumped = false;

        if (op_type == 0) {
            int funct_code = toint(command + 26, 6);
            if (funct_code == 8) is_jumped = true;

            switch(funct_code) {
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
            }
        } else if (op_type == 2 || op_type == 3) {
            is_jumped = true;
            int target_address = toint(command + 6, 26);
            if (op_type == 2) {
                int_to_register32(target_address, PC);
            } else {
                int next_instruction = toint_signed(PC.bits, 32) + 1;
                bool temp[32];
                for (int i = 31; i >= 0; --i) temp[i] = (next_instruction >> (31 - i)) & 1;
                write_register(31, temp);
                int_to_register32(target_address, PC);
            }
        } else {
            switch(op_type) {
                case 4:  is_jumped = beq(command + 6, command + 11, command + 16); break;
                case 5:  is_jumped = bne(command + 6, command + 11, command + 16); break;
                case 8:  addi(command + 6, command + 11, command + 16); break;
                case 9:  addiu(command + 6, command + 11, command + 16); break;
                case 15: lui(command + 11, command + 16); break;
                case 35: lw(command + 6, command + 11, command + 16); break;
                case 43: sw(command + 6, command + 11, command + 16); break;
            }
        }
        return is_jumped;
    }

    void runSimulation() {
        ret0(PC.bits);
        is_running = true; // 啟動硬體

        int safety_counter = 0; // 防止程式意外越界的安全閥

        while (is_running) {
            int current_pc_val = toint_signed(PC.bits, 32);

            // 安全檢查：若 PC 指向無效記憶體或執行超過一萬步，立刻停機
            if (current_pc_val < 0 || current_pc_val >= (int)instr_mem.size() || safety_counter++ > 10000) {
                break;
            }

            string current_bit_str = instr_mem[current_pc_val];
            bool current_command[32];
            for (int i = 0; i < 32; i++) {
                current_command[i] = (current_bit_str[i] == '1');
            }

            bool is_jumped = executeSingleCommand(current_command);

            if (!is_jumped && is_running) {
                int_to_register32(toint_signed(PC.bits, 32) + 1, PC);
            }
        }
    }

public:
    MipsSimulator() { reset(); }

    void multicommand(string machineCode) {
        // 清除任何可能的空白字元
        string formatted = "";
        for(char c : machineCode) {
            if((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
                formatted += c;
        }

        instr_mem.clear();
        for(size_t it = 0; it < formatted.length(); it += 8) {
            if(it + 8 <= formatted.length()) {
                string singleHex = formatted.substr(it, 8);
                instr_mem.push_back(transHexFormat(singleHex));
            }
        }

        runSimulation();
    }

    void dump_registers() {
        printf("\n==================== REGISTER DUMP ====================\n");
        for (int i = 0; i < 32; ++i) {
            printf("R%d: %d\t", i, toint_signed(reg[i].bits, 32));
            if ((i + 1) % 4 == 0) printf("\n");
        }
        printf("HI: %d\tLO: %d\tPC: %d\n", toint_signed(HI.bits, 32), toint_signed(LO.bits, 32), toint_signed(PC.bits, 32));
        printf("=======================================================\n\n");
    }

    void reset() {
        ret0(PC.bits);
        is_running = false;
        for (int i = 0; i < 32; i++) ret0(reg[i].bits);
        ret0(HI.bits); ret0(LO.bits);
        for (int i = 0; i < 1024; i++) data_mem[i] = 0;
        instr_mem.clear();

        data_mem[508] = 99;
        int_to_register32(500, reg[29]);
    }
};

int main() {
    MipsSimulator sim;
    MipsAssembler mipasm;

    // 測試測資：動態生成費氏數列（直線版，絕不回跳，必定終止）
    vector<string> fibAssembly = {
        "addi $s0, $zero, 0",   // F(0) = 0
        "addi $s1, $zero, 1",   // F(1) = 1

        "add $t0, $s0, $s1",    // F(2) = 1
        "add $s0, $s1, $zero",
        "add $s1, $t0, $zero",

        "add $t0, $s0, $s1",    // F(3) = 2
        "add $s0, $s1, $zero",
        "add $s1, $t0, $zero",

        "add $t0, $s0, $s1",    // F(4) = 3
        "add $s0, $s1, $zero",
        "add $s1, $t0, $zero",

        "addi $v0, $zero, 1",   // Syscall 1: 印出整數
        "add $a0, $s1, $zero",  // 印出 F(4) 的結果
        "syscall",

        "addi $v0, $zero, 10",  // Syscall 10: 終止程式
        "syscall"
    };

    string programHex = "";
    for (const string& asmLine : fibAssembly) {
        string hex = mipasm.assembleLine(asmLine);
        if(!hex.empty()) programHex += hex;
    }

    cout << "=== 啟動安全硬體模擬（直線費氏數列驗證） ===" << endl;
    sim.multicommand(programHex);
    sim.dump_registers();

    return 0;
}
