#ifndef MEM_H
#define MEM_H

#include <stdint.h>

enum ACCESS
{
    ACCESS_REGS = 0,
    ACCESS_RAM1,
    ACCESS_RAM2,
    ACCESS_CART,
};

#define RAM1_START          0x0000
#define RAM1_REGS_START     0x0000
#define RAM1_END            0x2000
#define PPU_REGS_START      0x2100
#define PPU_REGS_END        0x2184
#define CPU_REGS_START      0x4200
#define CPU_REGS_END        0x437b
#define RAM1_REGS_END       0x6000

#define RAM1_REGS_BANK_RANGE0_START 0x00
#define RAM1_REGS_BANK_RANGE0_END   0x3f
#define RAM1_REGS_BANK_RANGE1_START 0x80
#define RAM1_REGS_BANK_RANGE1_END   0xbf
#define RAM1_EXTRA_BANK             0x7e

#define RAM2_START      0x007e2000
#define RAM2_END        0x007fffff

struct mem_write_t
{
    void (*write)(uint32_t effective_address, uint8_t value);
};

struct mem_read_t
{
    uint8_t (*read)(uint32_t effective_address);
};

struct blah_t
{
    uint64_t clock;
};

void init_mem();

void shutdown_mem();

uint32_t access_location(uint32_t effective_address);

// void *memory_pointer(uint32_t effective_address);

void write_byte(uint32_t effective_address, uint8_t data);

uint8_t read_byte(uint32_t effective_address);

uint16_t read_word(uint32_t effective_address);

void poke(uint32_t effective_address, uint32_t *value);


#endif // DMOV_H
