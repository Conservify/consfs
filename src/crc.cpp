#include "phylum/crc.h"

namespace phylum {

#if defined(PROGMEM)
#define FLASH_PROGMEM PROGMEM
#define FLASH_READ_DWORD(x) (pgm_read_dword_near(x))
#else
#define FLASH_PROGMEM
#define FLASH_READ_DWORD(x) (*(uint32_t*)(x))
#endif

static uint32_t crc_table[16] FLASH_PROGMEM = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t crc32_update(uint32_t crc, uint8_t data) {
    uint8_t tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = FLASH_READ_DWORD(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = FLASH_READ_DWORD(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
}

uint32_t crc32_checksum(uint8_t *data, size_t size) {
    uint32_t crc = ~0;
    while (size-- > 0) {
        crc = crc32_update(crc, *(data++));
    }
    return ~crc;
}

}
