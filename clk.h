#ifndef CLK_H
#define CLK_H

#include <stdint.h>

struct clk_t
{
    uint64_t count;
    uint64_t count_per_tick;
//    double period;
//    uint32_t frequency;
};


void init_clock(struct clk_t *clock, uint32_t frequency);

uint32_t update_clock(struct clk_t *clock);

#endif // CLK_H
