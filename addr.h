#ifndef ADDR_COMMON_H
#define ADDR_COMMON_H




#define CPU_BANKS 0xff
#define CPU_BANK_SIZE 0xffff
#define CPU_RAM_SIZE 0x0fffffff
/* this macro won't work properly all the time. Some addressing modes temporarily increment the bank register. */
#define EFFECTIVE_ADDRESS(bank, offset) ( (((uint32_t)(offset)) & 0x0000ffff) | ( (((uint32_t)(bank)) << 16) & 0x00ff0000))

#define RAM1_START_BANK 0x00
#define RAM1_END_BANK 0x3f
#define RAM1_START_ADDRESS 0x0000
#define RAM1_END_ADDRESS 0x2000
//#define RAM1_START_ADDRESS EFFECTIVE_ADDRESS(0x00, 0x0000)
//#define RAM1_END_ADDRESS EFFECTIVE_ADDRESS(0x3f, 0x2000)

#define RAM2_ADDRESS_START EFFECTIVE_ADDRESS(0x7e, 0x0000)
#define RAM2_ADDRESS_END EFFECTIVE_ADDRESS(0x7f, 0xffff)

#define PPU_REGS_START_ADDRESS 0x2100
#define PPU_REGS_END_ADDRESS 0x2180

#define CPU_REGS_START_ADDRESS 0x4200
#define CPU_REGS_END_ADDRESS 0x437a

#define APU_START_ADDRESS 0x2140
#define APU_END_ADDRESS 0x2143

#define CONTROLLER_ADDRESS_START EFFECTIVE_ADDRESS(0x00, 0x4000)
#define CONTROLLER_ADDRESS_END EFFECTIVE_ADDRESS(0x00, 0x41ff)

#define DMA_ADDRESS_START EFFECTIVE_ADDRESS(0x00, 0x4200)
#define DMA_ADDRESS_END EFFECTIVE_ADDRESS(0x00, 0x5fff)


#endif // ADDR_COMMON_H
