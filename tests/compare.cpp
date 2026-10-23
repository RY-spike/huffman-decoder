#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std;

vector<char> readFile(const string& filename) {
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "FAIL: 找不到文件 " << filename << endl;
        exit(1);
    }
    return vector<char>((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001);
    if (argc != 3) {
        cout << "用法: " << argv[0] << " 输入文件1 输入文件2" << endl;
        return 1;
    }

    string file1 = argv[1];
    string file2 = argv[2];

    vector<char> data1 = readFile(file1);
    vector<char> data2 = readFile(file2);

    if (data1 == data2) {
        cout << "PASS: 两个文件完全一致" << endl;
        cout << "  文件1: " << file1 << " (" << data1.size() << " 字节)" << endl;
        cout << "  文件2: " << file2 << " (" << data2.size() << " 字节)" << endl;
        return 0;
    }

    cout << "FAIL: 两个文件不一致" << endl;
    cout << "  文件1: " << file1 << " (" << data1.size() << " 字节)" << endl;
    cout << "  文件2: " << file2 << " (" << data2.size() << " 字节)" << endl;

    size_t min_len = min(data1.size(), data2.size());
    for (size_t i = 0; i < min_len; ++i) {
        if (data1[i] != data2[i]) {
            cout << "  第一个不同位置: 第 " << i << " 字节" << endl;
            cout << "  文件1 该字节: 0x" << hex << uppercase << setw(2) << setfill('0') << (int)(unsigned char)data1[i] << endl;
            cout << "  文件2 该字节: 0x" << hex << uppercase << setw(2) << setfill('0') << (int)(unsigned char)data2[i] << endl;
            return 1;
        }
    }
    cout << "  前 " << min_len << " 字节相同，但长度不同" << endl;
    return 1;
}