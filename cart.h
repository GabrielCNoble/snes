#ifndef ROM_H
#define ROM_H

#include <stdint.h>

enum MAP_MODE
{
    MAP_MODE_20_SLOW = 0x20,
    MAP_MODE_21_SLOW = 0x21,
    MAP_MODE_23_SLOW = 0x23,
    MAP_MODE_25_SLOW = 0x25,
    MAP_MODE_20_FAST = 0x30,
    MAP_MODE_21_FAST = 0x31,
    MAP_MODE_25_FAST = 0x35
};

enum ROM_SIZE
{
    ROM_SIZE_3_4M   = 0x09,
    ROM_SIZE_5_8M   = 0x0a,
    ROM_SIZE_9_16M  = 0x0b,
    ROM_SIZE_17_32M = 0x0c,
    ROM_SIZE_33_64M = 0x0d,
};

enum RAM_SIZE
{
    RAM_SIZE_NONE   = 0x00,
    RAM_SIZE_16K    = 0x01,
    RAM_SIZE_64K    = 0x03,
    RAM_SIZE_256K   = 0x05,
    RAM_SIZE_512K   = 0x06,
    RAM_SIZE_1M     = 0x07,
};

// enum CART_LOCS
// {
//     CART_LOC_ROM = 0,
//     CART_LOC_SRAM
// };
//#pragma pack(push, 1)
//struct snes_header_data_t
//{
//    uint16_t maker_code;
//    uint32_t game_code;
//    uint8_t unused0[7];
//    uint8_t expansion_ram_size;
//    uint8_t special_version;
//    uint8_t cartridge_sub_number;
//    uint8_t game_title[21];
//    uint8_t map_mode;
//    uint8_t cartridge_type;
//    uint8_t rom_size;
//    uint8_t ram_size;
//    uint8_t destination_code;
//    uint8_t unused1;
//    uint8_t mask_rom_version;
//    uint8_t complement_check[2];
//    uint8_t check_sum[2];
////    unsigned char rom_makeup;
////    unsigned char rom_type;
////    unsigned char rom_size;
////    unsigned char sram_size;
////    unsigned short creator_id;
////    unsigned char version;
////    unsigned char checksum_complement;
////    unsigned short checksum;
//};
//#pragma pack(pop)

#pragma pack(push, 1)
struct rom_header_t
{
    uint16_t maker_code;
    uint32_t game_code;
    uint8_t unused0[7];
    uint8_t expansion_ram_size;
    uint8_t special_version;
    uint8_t cartridge_sub_number;
    uint8_t game_title[21];
    uint8_t map_mode;
    uint8_t cartridge_type;
    uint8_t rom_size;
    uint8_t ram_size;
    uint8_t destination_code;
    uint8_t unused1;
    uint8_t mask_rom_version;
    uint8_t complement_check[2];
    uint8_t check_sum[2];
};
#pragma pack(pop)

//struct rom_t
//{
//    void *base;
//    unsigned char *rom_buffer;
//    struct snes_header_t *header;
//    unsigned short reset_vector;
//    unsigned int rom_size;
//};

// uint32_t cart_loc(uint32_t effective_address);

struct rom_header_t *get_rom_header(void *rom);

uint32_t load_cart(char *file_name);

void unload_cart();

void *mode20_cart_pointer(uint32_t effective_address, uint32_t write);

void *mode21_cart_pointer(uint32_t effective_address, uint32_t write);

// void *cart_pointer(uint32_t effective_address);

uint8_t cart_read(uint32_t effective_address);

void cart_write(uint32_t effective_address, uint8_t data);

#endif








