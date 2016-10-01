#include <intrin.h>
#include <vector>
#include <bitset>
#include <array>
#include <iostream>

struct Flags {
    std::bitset<32> eax;
    std::bitset<32> ebx;
    std::bitset<32> ecx;
    std::bitset<32> edx;

    Flags(int eax, int ebx, int ecx, int edx) :
        eax(eax), ebx(ebx), ecx(ecx), edx(edx) { }
};

int main(int argc, char *argv[]) {
    std::array<int, 4> buffer;
    std::vector<Flags> flags;

    __cpuid(buffer.data(), 0);
    int nIDs = buffer[0];

    for (int i = 0; i <= nIDs; ++i) {
        __cpuidex(buffer.data(), i, 0);

        flags.emplace_back(buffer[0], buffer[1], buffer[2], buffer[3]);
    }

    if (flags[7].ebx[5])
        std::cout << "/arch:AVX2";
    else if (flags[1].ecx[28])
        std::cout << "/arch:AVX";

    return 0;
}
