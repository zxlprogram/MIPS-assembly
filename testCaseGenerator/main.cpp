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
    writeBinaryString(buffer,"00100100000010000000000000000111");
    writeBinaryString(buffer,"00100100000100000000000000000000");
    writeBinaryString(buffer,"00100100000100010000000000000001");
    writeBinaryString(buffer,"00100100000010010000000000000010");
    writeBinaryString(buffer,"00000001000010010101000000101010");
    writeBinaryString(buffer,"00010101010000000000000000000111");
    writeBinaryString(buffer,"00000010001100001001000000100001");
    writeBinaryString(buffer,"00000010001000001000000000100001");
    writeBinaryString(buffer,"00000010010000001000100000100001");
    writeBinaryString(buffer,"00100101001010010000000000000001");
    writeBinaryString(buffer,"00001000000000000000000000000100");
    writeBinaryString(buffer,"00100100000000100000000000000001");
    writeBinaryString(buffer,"00000010010000000010000000100001");
    writeBinaryString(buffer,"00000000000000000000000000001100");
    writeBinaryString(buffer,"00100100000000100000000000001010");
    writeBinaryString(buffer,"00000000000000000000000000001100");
    ofstream outFile("fibonacci.bin", ios::binary);
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
