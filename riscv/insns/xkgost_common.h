// xkgost custom extension
// SIMD-like multiplication of GF(2^8) vectors
auto xkgost_gfmul = [](uint64_t a, uint64_t b) {
    uint64_t res = 0;
    // Replicated primitive polynomial p(x) = 1 + x + x^6 + x^7 + x^8 (0xC3)
    uint64_t mask_poly = 0xC3C3C3C3C3C3C3C3;
    uint64_t mask_b = 0x0101010101010101;
    uint64_t mask_a = mask_b << 7; // 0x8080808080808080
    uint64_t bm;
    uint64_t am;
    for (size_t i = 0; i < 8; ++i) {
        // Select LSB in each byte
        bm = (b & mask_b);
        // Extend LSB to all bits in each byte
        bm |= (bm << 1);
        bm |= (bm << 2);
        bm |= (bm << 4);
        // Add "a" to result if corresponding "b" bit is one
        res ^= a & bm;
        // Go to next "b" bit
        b >>= 1;
        // Select MSB in each byte
        am = (a & mask_a);
        // Clear MSB in each byte
        a ^= am;
        // Shift, since MSB is clear already it doesn't propagate to next byte
        a <<= 1;
        // Extend MSB to all bits in each byte
        am |= (am >> 1);
        am |= (am >> 2);
        am |= (am >> 4);
        // Substract minimal polynom from each "a" byte if MSB in corresponding bit is one
        a ^= (am & mask_poly);
    }
    return res;
};

auto xkgost_kuzn = [&](uint8_t coef[8][16], int64_t rs1, int64_t rs2) {
    int64_t rd = 0;
    uint64_t sum;
    uint64_t *coef1;
    uint64_t *coef2;
    // Multiply 8x16 matrix to 16 element vector represented by rs1, rs2
    for (size_t i = 0; i < 8; ++i) {
        // Pointers to row coefficients
        coef1 = (uint64_t*) &coef[i];
        coef2 = coef1 + 1;
        // SIMD like dot product of two vectors
        sum = xkgost_gfmul(*coef1, rs1) ^ xkgost_gfmul(*coef2, rs2);
        sum ^= (sum >> 32);
        sum ^= (sum >> 16);
        sum ^= (sum >> 8);
        sum &= 0xFF;
        // Put result to appropriate position
        rd |= sum << (8 * i);
    }
    return rd;
};

auto gost64_tau1 = [](uint64_t a, uint64_t b) {
    return ((((a >>  0) & 0xFF) <<  0) |
            (((b >>  0) & 0xFF) <<  8) |
            (((a >>  8) & 0xFF) << 16) |
            (((b >>  8) & 0xFF) << 24) |
            (((a >> 16) & 0xFF) << 32) |
            (((b >> 16) & 0xFF) << 40) |
            (((a >> 24) & 0xFF) << 48) |
            (((b >> 24) & 0xFF) << 56));
};

auto gost32_tau1 = [](uint32_t a, uint32_t b) {
    return ((((a >> 0) & 0xFF) <<  0) |
            (((b >> 0) & 0xFF) <<  8) |
            (((a >> 8) & 0xFF) << 16) |
            (((b >> 8) & 0xFF) << 24));
};

auto xkgost_stbg = [](uint64_t a) {
    static uint64_t mat[64] = {
        0x8e20faa72ba0b470ull, 0x47107ddd9b505a38ull, 0xad08b0e0c3282d1cull, 0xd8045870ef14980eull,
        0x6c022c38f90a4c07ull, 0x3601161cf205268dull, 0x1b8e0b0e798c13c8ull, 0x83478b07b2468764ull,
        0xa011d380818e8f40ull, 0x5086e740ce47c920ull, 0x2843fd2067adea10ull, 0x14aff010bdd87508ull,
        0x0ad97808d06cb404ull, 0x05e23c0468365a02ull, 0x8c711e02341b2d01ull, 0x46b60f011a83988eull,
        0x90dab52a387ae76full, 0x486dd4151c3dfdb9ull, 0x24b86a840e90f0d2ull, 0x125c354207487869ull,
        0x092e94218d243cbaull, 0x8a174a9ec8121e5dull, 0x4585254f64090fa0ull, 0xaccc9ca9328a8950ull,
        0x9d4df05d5f661451ull, 0xc0a878a0a1330aa6ull, 0x60543c50de970553ull, 0x302a1e286fc58ca7ull,
        0x18150f14b9ec46ddull, 0x0c84890ad27623e0ull, 0x0642ca05693b9f70ull, 0x0321658cba93c138ull,
        0x86275df09ce8aaa8ull, 0x439da0784e745554ull, 0xafc0503c273aa42aull, 0xd960281e9d1d5215ull,
        0xe230140fc0802984ull, 0x71180a8960409a42ull, 0xb60c05ca30204d21ull, 0x5b068c651810a89eull,
        0x456c34887a3805b9ull, 0xac361a443d1c8cd2ull, 0x561b0d22900e4669ull, 0x2b838811480723baull,
        0x9bcf4486248d9f5dull, 0xc3e9224312c8c1a0ull, 0xeffa11af0964ee50ull, 0xf97d86d98a327728ull,
        0xe4fa2054a80b329cull, 0x727d102a548b194eull, 0x39b008152acb8227ull, 0x9258048415eb419dull,
        0x492c024284fbaec0ull, 0xaa16012142f35760ull, 0x550b8e9e21f7a530ull, 0xa48b474f9ef5dc18ull,
        0x70a6a56e2440598eull, 0x3853dc371220a247ull, 0x1ca76e95091051adull, 0x0edd37c48a08a6d8ull,
        0x07e095624504536cull, 0x8d70c431ac02a736ull, 0xc83862965601dd1bull, 0x641c314b2b8ee083ull,
    };

    int64_t rd = 0;
    for (size_t i = 0; i < 64; ++i) {
        if ((a >> (63 - i)) & 1) {
            rd ^= mat[i];
        }
    }
    return rd;
};
