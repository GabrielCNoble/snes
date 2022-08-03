#ifndef DMA_H
#define DMA_H

#include <stdint.h>

enum DMA_WRITE_MODES
{
    DMA_WRITE_MODE_BYTE_ONCE = 0,
    DMA_WRITE_MODE_WORDLH_ONCE,
    DMA_WRITE_MODE_BYTE_TWICE,
    DMA_WRITE_MODE_WORDLH_TWICE,
    DMA_WRITE_MODE_DWORDLHLH_ONCE,
};

enum DMA_ADDR_MODES
{
    DMA_ADDR_MODE_FIXED     = 1,
    DMA_ADDR_MODE_INC       = 0,
    DMA_ADDR_MODE_DEC       = 2
};

enum DMA_DIRECTION
{
    DMA_DIRECTION_CPU_PPU = 0,
    DMA_DIRECTION_PPU_CPU,
};

#define DMA_CYCLES_PER_BYTE 8
#define DMA_BASE_REG 0x2100

struct dma_t
{
    uint32_t    addr;
    uint32_t    count;
    uint32_t    increment;
    uint16_t    regs[4];
    uint8_t     cur_reg : 2;
    uint8_t     direction : 1;
    // uint8_t     channel : 3;
    // uint8_t     addr_mode : 2;
};

#define HDMA_ENTRY_MODE_MASK 0x80
#define HDMA_ENTRY_COUNT_MASK 0x7f
/* hdma table entry */
struct hdma_entry_t
{
    uint8_t mode_count;
    uint8_t data[];
};

struct hdma_t
{
    uint16_t    regs[4];
    // uint16_t    data_bank_reg;
    // uint16_t    data_addr_reg;
    uint8_t     cur_reg;
    uint8_t     last_reg;
    uint8_t     indirect : 1;
};

/* HDMA operation has a kind of annoying to emulate cycle overhead.
To avoid a jungle of conditionals inside a single function, we use
a small state machine instead. */
enum HDMA_STATES
{
    /* HDMA is idle, waiting for the channel initialization period or h-blank */
    HDMA_STATE_IDLE = 0,
    /* start HDMA initialization. This is the first 18 cycle overhead */
    HDMA_STATE_INIT,
    /* iterate over channels, starting initialization on those enabled */
    HDMA_STATE_INIT_CHANNELS,
    /* initialize an enabled channel */
    HDMA_STATE_INIT_CHANNEL,
    /* start HDMA transfers. This is the first 18 cycle overhead */
    HDMA_STATE_START,
    /* iterate over channels, staring transfers on those enabled */
    HDMA_STATE_START_CHANNELS,
    /* setup an enabled channel for transfer */
    HDMA_STATE_START_CHANNEL,
    /* transfer */
    HDMA_STATE_TRANSFER,
};

#define HDMA_INIT_LINE 0
#define HDMA_INIT_DOT 6
#define HDMA_INIT_CYCLE_OVERHEAD 18
#define HDMA_INIT_DIR_ADDR_CYCLE_OVERHEAD 8
#define HDMA_INIT_IND_ADDR_CYCLE_OVERHEAD 24
#define HDMA_START_CYCLE_OVERHEAD 18
#define HDMA_START_CHANNEL_CYCLE_OVERHEAD 8
#define HDMA_IND_ADDR_START_CYCLE_OVERHEAD 16

enum HDMA_PARAMS
{
    HDMA_PARAM_INDIRECT = 1 << 6
};


#define DMA_CHANNEL_REG(reg, channel) (reg | (channel << 4))
#define DMA_PARAM_WRITE_MODE_MASK 0x7
#define DMA_INDIRECT_ADDR_MASK 0x40

void init_dma();

void step_dma(int32_t cycle_count);

uint32_t hdma_idle_state(int32_t cycle_count);

uint32_t hdma_init_state(int32_t cycle_count);

void setup_hdma_channel_regs(uint32_t channel);

uint32_t hdma_init_channels_state(int32_t cycle_count);

uint32_t hdma_init_channel_state(int32_t cycle_count);

uint32_t hdma_start_state(int32_t cycle_count);

uint32_t hdma_start_channels_state(int32_t cycle_count);

uint32_t hdma_start_channel_state(int32_t cycle_count);



uint32_t hdma_transfer_state(int32_t cycle_count);

void step_hdma(int32_t cycle_count);

void dma_reg_list(uint16_t mode, uint16_t reg, uint16_t *reg_list);

void mdmaen_write(uint32_t effective_address, uint8_t data);

void hdmaen_write(uint32_t effective_address, uint8_t data);

#endif
