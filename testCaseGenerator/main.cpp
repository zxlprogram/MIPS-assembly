#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;
void writeBinaryString(vector<char>& buffer, const string& binStr) {
    char currentByte = 0;
    for (size_t i = 0; i < binStr.length(); ++i) {
        currentByte = (currentByte << 1) | (binStr[i] - '0');
        if ((i + 1) % 8 == 0) {
            buffer.push_back(currentByte);
            currentByte = 0;
        }
    }
}

int main() {
    vector<char> buffer;
    string ins_0_bne  = "000101" "00001" "00010" "0011000000000000";
    string ins_1_add  = "000000" "00001" "00010" "00011" "00000" "100000";
    string ins_2_sub  = "000000" "00011" "00010" "00101" "00000" "100010";
    string ins_3_mult = "000000" "00101" "00010" "0000000000" "011000";
    string ins_4_jump = "000010" "00000000000000000000000110";
    string ins_5_div  = "000000" "00001" "00010" "0000000000" "011010";
    string ins_6_lw   = "100011" "00010" "00111" "0000000000000101";

    writeBinaryString(buffer, ins_0_bne);
    writeBinaryString(buffer, ins_1_add);
    writeBinaryString(buffer, ins_2_sub);
    writeBinaryString(buffer, ins_3_mult);
    writeBinaryString(buffer, ins_4_jump);
    writeBinaryString(buffer, ins_5_div);
    writeBinaryString(buffer, ins_6_lw);
    /*
    if (R1 != R2) {
        R3 = R1 + R2;
        R5 = R3 - R2;
        mult R5, R2;
        goto TARGET_LABEL;
    }
    div R1, R2;
    TARGET_LABEL:
    lw R7, 5(R2);
    */

    ofstream outFile("input.bin", ios::binary);
    if (!outFile) {
        cerr << "failed to build the testcase. " << endl;
        return 1;
    }

    outFile.write(buffer.data(), buffer.size());
    outFile.close();

    cout << "success to build the testcase 'input.bin'" << endl;
    cout << "include  " << buffer.size() / 4 << " commands, totally " << buffer.size() << " bytes." << endl;

    return 0;
}
