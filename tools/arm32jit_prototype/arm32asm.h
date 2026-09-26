// Minimal Thumb-2 encoder for exactly the instructions Stage 2 needs. Every encoding here was
// derived empirically (assembled with arm-linux-gnueabihf-as, verified with objdump), not from
// memory of the ARM ARM -- see the probe*.s scratch files this was checked against.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

class Arm32Asm {
public:
    std::vector<uint16_t> code;

    void movw(unsigned rd, uint32_t imm16) {
        unsigned i = (imm16 >> 11) & 1, imm4 = (imm16 >> 12) & 0xF, imm3 = (imm16 >> 8) & 7, imm8 = imm16 & 0xFF;
        code.push_back(uint16_t(0xF000 | (i << 10) | (0x24 << 4) | imm4));
        code.push_back(uint16_t((imm3 << 12) | (rd << 8) | imm8));
    }
    void movt(unsigned rd, uint32_t imm16) {
        unsigned i = (imm16 >> 11) & 1, imm4 = (imm16 >> 12) & 0xF, imm3 = (imm16 >> 8) & 7, imm8 = imm16 & 0xFF;
        code.push_back(uint16_t(0xF000 | (i << 10) | (0x2C << 4) | imm4));
        code.push_back(uint16_t((imm3 << 12) | (rd << 8) | imm8));
    }
    // Loads a full 32-bit constant into Rd (movw+movt pair).
    void load32(unsigned rd, uint32_t value) {
        movw(rd, value & 0xFFFF);
        movt(rd, (value >> 16) & 0xFFFF);
    }
    void mov(unsigned rd, unsigned rm) {
        code.push_back(uint16_t(0x4600 | (((rd >> 3) & 1) << 7) | ((rm & 0xF) << 3) | (rd & 7)));
    }
    void push_r4_lr() { code.push_back(0xB510); }
    void pop_r4_pc()  { code.push_back(0xBD10); }
    void blx(unsigned rm) { code.push_back(uint16_t(0x4780 | (rm << 3))); }
    void bx(unsigned rm)  { code.push_back(uint16_t(0x4700 | (rm << 3))); }

    size_t sizeBytes() const { return code.size() * 2; }
};
