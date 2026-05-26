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

    "000101" "00001" "00010" "0011000000000000";
    "000000" "00001" "00010" "00011" "00000" "100000";
    "000000" "00011" "00010" "00101" "00000" "100010";
    "000000" "00101" "00010" "0000000000" "011000";
    "000010" "00000000000000000000000110";
    "000000" "00001" "00010" "0000000000" "011010";
    "100011" "00010" "00111" "0000000000000101";
    writeBinaryString(buffer,"00010100001000100011000000000000000000000010001000011000001000000000000001100010001010000010001000000000101000100000000000011000000010000000000000000000000001100000000000100010000000000001101010001100010001110000000000000101");
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
