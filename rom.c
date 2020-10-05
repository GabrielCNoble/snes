#include "rom.h"
#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


//struct rom_t current_rom;

void *(*rom_pointer_function)(uint32_t effective_address);
unsigned char *rom_buffer;
struct rom_header_t *rom_header;

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
            case ROM_MAPPING_MODE_20_FAST:        
            case ROM_MAPPING_MODE_20_SLOW:
            case ROM_MAPPING_MODE_21_FAST:
            case ROM_MAPPING_MODE_21_SLOW:
            case ROM_MAPPING_MODE_23_SLOW:
            case ROM_MAPPING_MODE_25_FAST:
            case ROM_MAPPING_MODE_25_SLOW:
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



uint32_t load_rom_file(char *file_name)
{
    FILE *rom_file;
    void *file_buffer;
    unsigned long rom_file_size;

//    unsigned char *rom_buffer;
//    unsigned char *c;
//    struct snes_header_t *header;
//    struct rom_t rom;

    int has_smc_header;
//    int i;
//    int j;
    char *rom_type_str;
    char *rom_size_str;
    char *ram_size_str;
//    int rom_type = ROM_TYPE_UNKNOWN;
    char game_name[32];


    rom_file = fopen(file_name, "rb");

//    current_rom.header = NULL;
//    current_rom.rom_buffer = NULL;

    if(!rom_file)
    {
        printf("couldn't open file %s\n", file_name);
        return 0;
    }


    fseek(rom_file, 0, SEEK_END);
    rom_file_size = ftell(rom_file);
    rewind(rom_file);

    file_buffer = calloc(1, rom_file_size);

    fread(file_buffer, 1, rom_file_size, rom_file);
    fclose(rom_file);

    has_smc_header = rom_file_size % 1024;

    rom_buffer = (unsigned char *)file_buffer + has_smc_header;
    rom_header = get_rom_header(rom_buffer);
//    rom_header = (struct snes_header_t *)(rom_buffer + 0x00007fb0);
    
    switch(rom_header->map_mode)
    {
        case ROM_MAPPING_MODE_20_SLOW:
            rom_type_str = "mode 20, slow";
            rom_pointer_function = mode20_rom_pointer;
        break;
        
        case ROM_MAPPING_MODE_21_SLOW:
            rom_type_str = "mode 21, slow";
            rom_pointer_function = mode21_rom_pointer;
        break;
        
        case ROM_MAPPING_MODE_20_FAST:
            rom_type_str = "mode 20, fast";
            rom_pointer_function = mode20_rom_pointer;
        break;
        
        case ROM_MAPPING_MODE_21_FAST:
            rom_type_str = "mode 21, fast";
            rom_pointer_function = mode21_rom_pointer;
        break;

        default:
            rom_type_str = "UNKNOWN";
        break;
    }
    
    switch(rom_header->rom_size)
    {
        case 0x09:
            rom_size_str = "3 - 4Mb";
        break;
        
        case 0x0A:
            rom_size_str = "5 - 8Mb";
        break;
        
        case 0x0B:
            rom_size_str = "9 - 16Mb";
        break;
        
        case 0x0C:
            rom_size_str = "17 - 32Mb";
        break;
        
        case 0x0D:
            rom_size_str = "33 - 64Mb";
        break;
        
        default:
            rom_size_str = "UNKNOWN";
        break;
    }
    
    switch(rom_header->ram_size)
    {
        case 0x00:
            ram_size_str = "no ram";
        break;
        
        case 0x01:
            ram_size_str = "16Kb";
        break;
        
        case 0x03:
            ram_size_str = "64Kb";
        break;
        
        case 0x05:
            ram_size_str = "256Kb";
        break;
        
        case 0x06:
            ram_size_str = "512Kb";
        break;
        
        case 0x07:
            ram_size_str = "1Mb";
        break;
    }
    
    strncpy(game_name, rom_header->game_title, 21);
    
    printf("rom type: %s\n", rom_type_str);
    printf("rom size: %s\n", rom_size_str);
    printf("ram size: %s\n", ram_size_str);
    printf("game name: %s\n", game_name);
    
    return 1;
    
}


//void dump_rom(struct rom_t *rom)
//{
//    unsigned char *c;
//    char tr;
//    int i;
//    int j;
//    int line_offset;
//
//    c = rom->header;
//
//    for(i = 0; i < 4; i++)
//    {
//        line_offset = (unsigned char *)c + i * 16 - rom->rom_buffer;
//        printf("%08x: ", line_offset);
//
//        for(j = 0; j < 16; j++)
//        {
//            if(j == 8)
//            {
//                printf("  ");
//            }
//            printf("%02x ", c[i * 16 + j]);
//        }
//
//        printf("|");
//        for(j = 0; j < 16; j++)
//        {
//            tr = c[i * 16 + j];
//
//            if(!isalnum(tr) && tr != ' ')
//            {
//                tr = '.';
//            }
//
//            printf("%c", tr);
//        }
//        printf("|");
//
//        printf("\n");
//    }
//}

void *mode20_rom_pointer(uint32_t effective_address)
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

void *mode21_rom_pointer(uint32_t effective_address)
{
    /* http://gatchan.net/uploads/Consoles/SNES/Flashcard/SNES_MemMap.txt */
    /* mode 21 (HiROM) uses address line 15 (so, it addresses from 0x0000 to 0xffff),
    and ignores addresss lines 22 and 23 (so the bank index effectively goes from
    0x00 to 0x3f, and the ignored lines cause those banks to be mirrored onto
    banks 0x40 to 0x7f, 0x80 to 0xbf and 0xc0 to 0xff). */
    
    /* mode 21 rom uses 64K banks, so contiguous snes addresses maps contiguously
    to rom memory. */
    
    return rom_buffer + ((effective_address & 0xffff) | (effective_address & 0x003f0000));
}

void *rom_pointer(unsigned int effective_address)
{
    if(rom_pointer_function)
    {
        return rom_pointer_function(effective_address);
    }
    
    return NULL;
}

uint32_t rom_read(uint32_t effective_address)
{
    return *(uint32_t *)rom_pointer(effective_address);
}









