#ifndef ROM_H
#define ROM_H

#include <stdint.h>

enum ROM_TYPE
{
    ROM_MAPPING_MODE_20_SLOW = 0x20,
    ROM_MAPPING_MODE_21_SLOW = 0x21,
    ROM_MAPPING_MODE_23_SLOW = 0x23,
    ROM_MAPPING_MODE_25_SLOW = 0x25,
    ROM_MAPPING_MODE_20_FAST = 0x30,
    ROM_MAPPING_MODE_21_FAST = 0x31,
    ROM_MAPPING_MODE_25_FAST = 0x35
};



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


struct rom_header_t *get_rom_header(void *rom);

uint32_t load_rom_file(char *file_name);

void dump_rom(struct rom_t *rom);

void *mode20_rom_pointer(uint32_t effective_address);

void *mode21_rom_pointer(uint32_t effective_address);

void *rom_pointer(unsigned int effective_address);

uint32_t rom_read(uint32_t effective_address);

//__fastcall void access_rom(unsigned int effective_address, int write, void *data, int width);

//void map_rom(struct rom_t *rom);

#endif // ROM_H








