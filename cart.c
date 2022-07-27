#include "cart.h"
#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


//struct rom_t current_rom;

uint32_t rom_sizes[] = {
    [ROM_SIZE_3_4M]     = 0x400000,
    [ROM_SIZE_5_8M]     = 0x800000,
    [ROM_SIZE_9_16M]    = 0x1000000,
    [ROM_SIZE_17_32M]   = 0x2000000,
    [ROM_SIZE_33_64M]   = 0x4000000
};

const char *rom_size_strs[] = {
    [ROM_SIZE_3_4M]     = "3 - 4M",
    [ROM_SIZE_5_8M]     = "5 - 8M",
    [ROM_SIZE_9_16M]    = "9 - 16M",
    [ROM_SIZE_17_32M]   = "17 - 32M",
    [ROM_SIZE_33_64M]   = "33 - 64M",
};

uint32_t ram_sizes[] = {
    [RAM_SIZE_NONE]     = 0,
    [RAM_SIZE_16K]      = 0x4000,
    [RAM_SIZE_64K]      = 0x10000,
    [RAM_SIZE_256K]     = 0x40000,
    [RAM_SIZE_512K]     = 0x80000,
    [RAM_SIZE_1M]       = 0x100000,
};

const char *ram_size_strs[] = {
    [RAM_SIZE_NONE]     = "None",
    [RAM_SIZE_16K]      = "16K",
    [RAM_SIZE_64K]      = "64K",
    [RAM_SIZE_256K]     = "256K",
    [RAM_SIZE_512K]     = "512K",
    [RAM_SIZE_1M]       = "1M",
};

const char *map_mode_strs[] = {
    [MAP_MODE_20_SLOW] = "Mode 20, slow",
    [MAP_MODE_21_SLOW] = "Mode 21, slow",
    [MAP_MODE_23_SLOW] = "Mode 23, slow",
    [MAP_MODE_25_SLOW] = "Mode 25, slow",
    [MAP_MODE_20_FAST] = "Mode 20, fast",
    [MAP_MODE_21_FAST] = "Mode 21, fast",
    [MAP_MODE_25_FAST] = "Mode 25, fast"
};

void *(*map_mode_functions[])(uint32_t effective_address) = {
    [MAP_MODE_20_SLOW] = mode20_cart_pointer,
    [MAP_MODE_21_SLOW] = mode21_cart_pointer,
    [MAP_MODE_20_FAST] = mode20_cart_pointer,
    [MAP_MODE_21_FAST] = mode21_cart_pointer,
};

void *(*cart_pointer)(uint32_t effective_address);
uint8_t *               rom_buffer;
uint8_t *               sram;
struct rom_header_t *   rom_header;

// uint32_t cart_loc(uint32_t effective_address)
// {

// }

struct rom_header_t *get_rom_header(void *rom)
{
    struct rom_header_t *header = NULL;
    uint32_t offsets[] = {0x00007fb0, 0x0000ffb0, 0x0040ffb0};

    /* where the rom header is located depends on the mapping mode of the rom.
    For mode 20 and 23 roms, the header is at 0x7fb0, for mode 21, it's at
    0xffb0, and for mode 25, it's at 40ffb0. To find out which type of rom
    we're dealing with, we poke at all those locations and test whether there's
    valid information. If there is, this is very likely the correct location. */

    for(uint32_t offset_index = 0; offset_index < 3; offset_index++)
    {
        header = (struct rom_header_t *)((unsigned char *)rom + offsets[offset_index]);

        switch(header->map_mode)
        {
            case MAP_MODE_20_FAST:
            case MAP_MODE_20_SLOW:
            case MAP_MODE_21_FAST:
            case MAP_MODE_21_SLOW:
            case MAP_MODE_23_SLOW:
            case MAP_MODE_25_FAST:
            case MAP_MODE_25_SLOW:
                offset_index = 3;
            break;

            default:
                /* if we're in the last iteration and we get here, it means we
                didn't find the rom header. This may indicate a bad rom, or
                maybe a bug on our side. */
                header = NULL;
            break;
        }
    }

    return header;
}



uint32_t load_cart(char *file_name)
{
    FILE *file;
    void *file_buffer;
    uint64_t file_size;

//    unsigned char *rom_buffer;
//    unsigned char *c;
//    struct snes_header_t *header;
//    struct rom_t rom;

    int has_smc_header;
//    int i;
//    int j;
    const char *rom_type_str;
    const char *rom_size_str;
    const char *ram_size_str;
//    int rom_type = ROM_TYPE_UNKNOWN;
    char game_name[32];
    char save_file[512];


    file = fopen(file_name, "rb");

//    current_rom.header = NULL;
//    current_rom.rom_buffer = NULL;

    if(!file)
    {
        printf("couldn't open rom file %s\n", file_name);
        return 0;
    }


    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);
    file_buffer = calloc(1, file_size);
    fread(file_buffer, 1, file_size, file);
    fclose(file);

    has_smc_header = file_size % 1024;

    rom_buffer = (uint8_t *)file_buffer + has_smc_header;
    rom_header = get_rom_header(rom_buffer);

    rom_type_str = map_mode_strs[rom_header->map_mode];
    cart_pointer = map_mode_functions[rom_header->map_mode];
    rom_size_str = rom_size_strs[rom_header->rom_size];
    ram_size_str = ram_size_strs[rom_header->ram_size];

    strncpy(game_name, (const char *)rom_header->game_title, 21);

    printf("rom type: %s\n", rom_type_str);
    printf("rom size: %s\n", rom_size_str);
    printf("ram size: %s\n", ram_size_str);
    printf("game name: %s\n", game_name);

    if(!rom_sizes[rom_header->rom_size])
    {
        rom_header->rom_size = ROM_SIZE_17_32M;
    }

    uint8_t *resized_rom_buffer = calloc(1, rom_sizes[rom_header->rom_size]);
    memcpy(resized_rom_buffer, file_buffer, file_size);
    rom_buffer = resized_rom_buffer + has_smc_header;

    if(rom_header->ram_size != RAM_SIZE_NONE)
    {
        sram = calloc(1, ram_sizes[rom_header->ram_size]);
        strcpy(save_file, file_name);
        uint32_t index = strlen(save_file);
        while(index)
        {
            if(save_file[index] == '.')
            {
                save_file[index] = '\0';
                break;
            }
            index--;
        }
        strcat(save_file, ".srm");
        file = fopen(save_file, "rb");
        if(!file)
        {
            printf("couldn't open save file %s\n", save_file);
        }

        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        rewind(file);
        fread(sram, 1, file_size, file);
        fclose(file);
    }

    free(file_buffer);
    return 1;
}

void unload_cart()
{
    if(rom_buffer)
    {
        free(rom_buffer);
        rom_buffer = NULL;
    }

    if(sram)
    {
        free(sram);
        rom_buffer = NULL;
    }
}

void *mode20_cart_pointer(uint32_t effective_address)
{
    /* http://gatchan.net/uploads/Consoles/SNES/Flashcard/SNES_MemMap.txt */
    /* mode 20 (LoROM) ignores address line 15 (so, it addresses from 0x0000 to 0x7fff.
    It also ignores address line 23 (which is the last bit of the bank index). That
    means bank indexes 0x00 to 0x7f and 0x80 to 0xff refer to the same banks. */

    /* mode 20 rom gets mapped to the upper half of each bank, which means the effective
    address will have bit 15 set */

    /* one bank contains the first half of a "chunk". The next bank then contains
    the second half. This means the first 15 bits of the effective address (0 - 14)
    will contain the offset inside each chunk, and the bank will select which chunk
    we're indexing into. Although the rom is fragmented when seen through SNES'
    address space, it's in reality a contiguous block of data. To index into this
    contiguous block we'll use the first 15 bits of the effective address, and will
    shift left the bank index by one bit, turning bit 0 of the bank index into bit 15
    of the offset into the rom. */

    return rom_buffer + ((effective_address & 0x7fff) | ((effective_address & 0x007f0000) >> 1));
}

void *mode21_cart_pointer(uint32_t effective_address)
{
    /* http://gatchan.net/uploads/Consoles/SNES/Flashcard/SNES_MemMap.txt */
    /* mode 21 (HiROM) uses address line 15 (so, it addresses from 0x0000 to 0xffff),
    and ignores addresss lines 22 and 23 (so the bank index effectively goes from
    0x00 to 0x3f, and the ignored lines cause those banks to be mirrored onto
    banks 0x40 to 0x7f, 0x80 to 0xbf and 0xc0 to 0xff). */

    /* mode 21 rom uses 64K banks, so contiguous snes addresses maps contiguously
    to rom memory. */

    effective_address &= 0x003fffff;
    uint32_t offset = effective_address & 0xffff;
    uint32_t bank = effective_address >> 16;

    if(bank >= 0x30 && bank <= 0x3f && offset >= 0x6000 && offset < 0x8000)
    {
        return sram + (offset - 0x6000);
    }

    return rom_buffer + effective_address;
}

// void *cart_pointer(uint32_t effective_address)
// {
//     if(cart_pointer_function)
//     {
//         return cart_pointer_function(effective_address);
//     }

//     return NULL;
// }

uint8_t cart_read(uint32_t effective_address)
{
    return *(uint8_t *)cart_pointer(effective_address);
}

void cart_write(uint32_t effective_address, uint8_t data)
{
    uint8_t *pointer = cart_pointer(effective_address);
    *pointer = data;
}









