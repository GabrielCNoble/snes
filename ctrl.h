#ifndef CTRL_H
#define CTRL_H

#include <stdint.h>

union ctrl_t
{
    struct
    {
        uint8_t b;
        uint8_t y;
        uint8_t select;
        uint8_t start;
        uint8_t up;
        uint8_t down;
        uint8_t left;
        uint8_t right;
        uint8_t a;
        uint8_t x;
        uint8_t l;
        uint8_t r;
    };

    uint8_t buttons[12];
};

void init_ctrl();

void step_ctrl(int32_t cycle_count);

void ctrl_write(uint32_t effective_address, uint8_t value);

uint8_t ctrl_read(uint32_t effective_address);

#endif
