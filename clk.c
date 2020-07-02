#include "clk.h"
#include "SDL/include/SDL2/SDL.h"
#include <stdio.h>

//uint32_t frequency_initialized = 0;
//double counter_period = 0.0;
//uint64_t counter_frequency = 0;

void init_clock(struct clk_t *clock, uint32_t frequency)
{
//    if(!frequency_initialized)
//    {
//        counter_frequency = SDL_GetPerformanceFrequency();
//        counter_period = 1.0 / (double)counter_frequency; 
//        frequency_initialized = 1;
//    }
//    
//    clock->frequency = frequency;
//    clock->period = 1.0 / (double)clock->frequency;
////    clock->count_per_tick = 
//    clock->last_count = 0;
}

uint32_t update_clock(struct clk_t *clock)
{
//    uint64_t current_count = SDL_GetPerformanceCounter();
//    uint64_t counter_delta = current_count - clock->last_count;
//    float counter_elapsed = counter_period * (float)counter_delta;
//    uint32_t clock_elapsed = (uint32_t)(counter_elapsed / clock->period);
//    
//    if(clock_elapsed)
//    {
//        clock->last_count = current_count;
//    }
//    return clock_elapsed;
}
