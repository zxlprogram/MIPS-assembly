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
    writeBinaryString(buffer,"00100000000000010000000000001010");
    writeBinaryString(buffer,"00100000000000100000000000000011");
    writeBinaryString(buffer,"00000000001000100001100000100000");
    writeBinaryString(buffer,"00000000001000100010000000100010");
    writeBinaryString(buffer,"00000000001000100010100000100100");
    writeBinaryString(buffer,"00000000001000100011000000100101");
    writeBinaryString(buffer,"00000000010000010011100000101010");
    writeBinaryString(buffer,"10101100000000110000000000001000");
    writeBinaryString(buffer,"10001100000010000000000000001000");
    writeBinaryString(buffer,"00010000011010000000000000000001");
    writeBinaryString(buffer,"00100000000010010000001111100111");
    writeBinaryString(buffer,"00010100001000100000000000000001");
    writeBinaryString(buffer,"00100000000010100000001111100111");
    writeBinaryString(buffer,"00000000001000100000000000011000");
    writeBinaryString(buffer,"00000000001000100000000000011010");
    writeBinaryString(buffer,"00001100000000000000000000010011");
    writeBinaryString(buffer,"00100000000010110000000001101111");
    writeBinaryString(buffer,"00001000000000000000000000010101");
    writeBinaryString(buffer,"00100000000011000000001111100111");
    writeBinaryString(buffer,"00100000000011010000001000101011");
    writeBinaryString(buffer,"00000011111000000000000000001000");
    writeBinaryString(buffer,"00100000000011100000001100001001");

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
