#include "rom.h"
#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct snes_header_t *get_rom_header(void *rom)
{
    struct snes_header_t *header = NULL;
    int rom_type = ROM_TYPE_UNKNOWN;

    //header = (struct snes_header_t *)((unsigned char *)rom + 0x00007fff - sizeof(struct snes_header_t)) + 1;
    header = (struct snes_header_t *)((unsigned char *)rom + 0x00007FC0);
    rom_type = header->data.rom_makeup & 0x37;

    switch(rom_type)
    {
        case ROM_TYPE_LoROM:
        case ROM_TYPE_LoROM | ROM_TYPE_FastROM:
        case ROM_TYPE_SA1ROM:
            header->data.rom_makeup &= 0x37;
        break;

        default:
            //header = (struct snes_header_t *)((unsigned char *)rom + 0x0000ffff - sizeof(struct snes_header_t)) + 1;
            header = (struct snes_header_t *)((unsigned char *)rom + 0x0000FFC0);
            rom_type = header->data.rom_makeup & 0x37;

            switch(rom_type)
            {
                case ROM_TYPE_HiROM:
                case ROM_TYPE_HiROM | ROM_TYPE_FastROM:
                case ROM_TYPE_SA1ROM:
                    header->data.rom_makeup &= 0x37;
                break;

                default:
                    header = NULL;
                break;
            }
        break;
    }

    return header;
}



struct rom_t load_rom_file(char *file_name)
{
    FILE *rom_file;
    void *file_buffer;
    unsigned long rom_file_size;

    unsigned char *rom_buffer;
    unsigned char *c;
    struct snes_header_t *header;
    struct rom_t rom;

    int has_smc_header;
    int i;
    int j;
    char *rom_type_str;
    int rom_type = ROM_TYPE_UNKNOWN;
    char rom_name[32];




    rom_file = fopen(file_name, "rb");

    rom.header = NULL;
    rom.rom_buffer = NULL;

    if(!rom_file)
    {
        printf("couldn't open file %s\n", file_name);
        return rom;
    }


    fseek(rom_file, 0, SEEK_END);
    rom_file_size = ftell(rom_file);
    rewind(rom_file);

    file_buffer = calloc(1, rom_file_size);

    fread(file_buffer, 1, rom_file_size, rom_file);
    fclose(rom_file);

    has_smc_header = rom_file_size % 1024;

    rom_buffer = (unsigned char *)file_buffer + has_smc_header;

    header = get_rom_header(rom_buffer);

    if(header)
    {
        switch(header->data.rom_makeup)
        {
            case ROM_TYPE_LoROM:
                rom_type_str = "LoROM";
            break;

            case ROM_TYPE_HiROM:
                rom_type_str = "HiROM";
            break;

            case ROM_TYPE_LoROM | ROM_TYPE_FastROM:
                rom_type_str = "LoROM + FastROM";
            break;

            case ROM_TYPE_HiROM | ROM_TYPE_FastROM:
                rom_type_str = "HiROM + FastROM";
            break;

            case ROM_TYPE_SA1ROM:
                rom_type_str = "SA1 ROM";
            break;

            case ROM_TYPE_ExLoROM:
                rom_type_str = "ExLoROM";
            break;

            case ROM_TYPE_ExHiROM:
                rom_type_str = "ExHiROM";
            break;

            default:
                rom_type_str = "UNKNOWN";
            break;
        }

//        for(i = 0; i < 22; i++)
//        {
//            if(header->data.rom_title[i] != '\0')
//            {
//                break;
//            }
//        }

//        memcpy(rom_name, header->data.rom_title + i, 22 - i);
//        rom_name[21] = '\0';

//        printf("rom size : %d bytes\n", rom_file_size);
//        printf("rom type : %s\n", rom_type_str);
//        printf("smc header : %s\n", has_smc_header ? (has_smc_header == 512 ? "yes" : "broken") : "no");
//
//        //if(rom_type != ROM_TYPE_UNKNOWN)
//        {
//            printf("rom title : %s\n", rom_name);
//        }

        c = header;

        rom.header = header;
        rom.rom_buffer = rom_buffer;
        rom.base = file_buffer;
        rom.reset_vector = (((unsigned short)c[60]) | (((unsigned short)c[61]) << 8));
        rom.rom_size = rom_file_size;
    }
    else
    {
        printf("Corrupt rom...");

        free(file_buffer);
        file_buffer = NULL;
    }


    //rom.reset_vector = 0x841c - 0x8000;
    //rom.reset_vector = rom_buffer[0x] | ((unsigned short)c[61] << 8);

    return rom;
}


void dump_rom(struct rom_t *rom)
{
    unsigned char *c;
    char tr;
    int i;
    int j;
    int line_offset;

    c = rom->header;

    for(i = 0; i < 4; i++)
    {
        line_offset = (unsigned char *)c + i * 16 - rom->rom_buffer;
        printf("%08x: ", line_offset);

        for(j = 0; j < 16; j++)
        {
            if(j == 8)
            {
                printf("  ");
            }
            printf("%02x ", c[i * 16 + j]);
        }

        printf("|");
        for(j = 0; j < 16; j++)
        {
            tr = c[i * 16 + j];

            if(!isalnum(tr) && tr != ' ')
            {
                tr = '.';
            }

            printf("%c", tr);
        }
        printf("|");

        printf("\n");
    }
}


extern unsigned char *cpu_ram;

void map_rom(struct rom_t *rom)
{
    int i;

    unsigned int address = 0;
    unsigned int rom_offset = 0;
    unsigned int byte_count;

    //int rom_size = 0x400 << rom->header->data.rom_size;
    int rom_size = rom->rom_size;

    for(i = 0; i < 0x7d && rom_size > 0; i++)
    {
        address = EFFECTIVE_ADDRESS(i, 0x8000);
        byte_count = rom_size > 0x8000 ? 0x8000 : rom_size;
        memcpy(cpu_ram + address, rom->rom_buffer + rom_offset, byte_count);
        rom_offset += byte_count;
        rom_size -= 0x8000;
    }
}










