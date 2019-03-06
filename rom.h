#ifndef ROM_H
#define ROM_H




enum ROM_TYPE
{
    ROM_TYPE_LoROM = 0x20,
    ROM_TYPE_HiROM = 0x21,
    ROM_TYPE_SA1ROM = 0x23,
    ROM_TYPE_FastROM = 0x10,
    ROM_TYPE_ExLoROM = 0x32,
    ROM_TYPE_ExHiROM = 0x35,
    ROM_TYPE_UNKNOWN = 0xff,
};



#pragma pack(push, 1)
struct snes_header_data_t
{
    unsigned char rom_title[21];
    unsigned char rom_makeup;
    unsigned char rom_type;
    unsigned char rom_size;
    unsigned char sram_size;
    unsigned short creator_id;
    unsigned char version;
    unsigned char checksum_complement;
    unsigned short checksum;
};
#pragma pack(pop)

struct snes_header_t
{
    struct snes_header_data_t data;
    unsigned char padding[64 - sizeof(struct snes_header_data_t)];
};

struct rom_t
{
    void *base;
    unsigned char *rom_buffer;
    struct snes_header_t *header;
    unsigned short reset_vector;
    unsigned int rom_size;
};


struct snes_header_t *get_rom_header(void *rom);

void load_rom_file(char *file_name);

void dump_rom(struct rom_t *rom);

__fastcall void *rom_pointer(unsigned int effective_address);

//__fastcall void access_rom(unsigned int effective_address, int write, void *data, int width);

//void map_rom(struct rom_t *rom);

#endif // ROM_H








