require_extension(EXT_XKGOST);
require_rv64;
static uint8_t sbox[8][16] = {
    {12,  4,  6,  2, 10,  5, 11,  9, 14,  8, 13,  7,  0,  3, 15,  1},
    { 6,  8,  2,  3,  9, 10,  5, 12,  1, 14,  4,  7, 11, 13,  0, 15},
    {11,  3,  5,  8,  2, 15, 10, 13, 14,  1,  7,  4, 12,  9,  6,  0},
    {12,  8,  2,  1, 13,  4, 15,  6,  7,  0, 10,  5,  3, 14,  9, 11},
    { 7, 15,  5, 10,  8,  1,  6, 13,  0,  9,  3, 14, 11,  4,  2, 12},
    { 5, 13, 15,  6,  9,  2, 12, 10, 11,  7,  8,  1,  4,  3, 14,  0},
    { 8, 14,  2,  5,  6,  9,  1, 12, 15,  4, 11,  0, 13, 10,  3,  7},
    { 1,  7, 14, 13,  0,  5,  8,  3,  4, 15, 10,  6,  9, 12, 11,  2},
};

#ifdef __has_builtin
# define has_bswaps (__has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap64))
#else
# define has_bswaps (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#endif

reg_t rs1 = RS1, rs2 = RS2;
#if has_bswaps
rs1 = __builtin_bswap64(rs1), rs2 = __builtin_bswap32(rs2);
#else
rs1 = ((rs1 & 0x00FF00FF00FF00FFLL) <<  8) | ((rs1 & 0xFF00FF00FF00FF00LL) >>  8);
rs2 = ((rs2 & 0x00FF00FF00FF00FFLL) <<  8) | ((rs2 & 0xFF00FF00FF00FF00LL) >>  8);
rs1 = ((rs1 & 0x0000FFFF0000FFFFLL) << 16) | ((rs1 & 0xFFFF0000FFFF0000LL) >> 16);
rs2 = ((rs2 & 0x0000FFFF0000FFFFLL) << 16) | ((rs2 & 0xFFFF0000FFFF0000LL) >> 16);
rs1 = (rs1 << 32) | (rs1 >> 32);
#endif

uint32_t lo_key;
uint32_t t = 0;
uint32_t g;
lo_key = (uint32_t)rs1 + (uint32_t)rs2;
for (size_t i = 0; i < 32; i += 4)
    t |= (uint32_t)sbox[i >> 2][(lo_key >> i) & 0xF] << i;
g = (t << 11) | (t >> 21);
g ^= (uint32_t)(rs1 >> 32);
rs1 = (rs1 << 32) | g;

#if has_bswaps
WRITE_RD(__builtin_bswap64(rs1));
#else
rs1 = ((rs1 & 0x00FF00FF00FF00FFLL) <<  8) | ((rs1 & 0xFF00FF00FF00FF00LL) >>  8);
rs1 = ((rs1 & 0x0000FFFF0000FFFFLL) << 16) | ((rs1 & 0xFFFF0000FFFF0000LL) >> 16);
rs1 = (rs1 << 32) | (rs1 >> 32);
WRITE_RD(rs1);
#endif
#undef has_bswaps
