#include "emu.h"
#include "SDL2/SDL.h"

uint32_t breakpoint_count = 0;
struct breakpoint_t breakpoints[512];

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

uint32_t window_width = 800;
uint32_t window_height = 600;

char *breakpoint_register_names[] = 
{
    [BREAKPOINT_REGISTER_Y] = "Y",
    [BREAKPOINT_REGISTER_X] = "X"
};

extern struct cpu_state_t cpu_state;

//void set_breakpoint(uint32_t effective_address)
//{
//    breakpoints[breakpoint_count] = effective_address & 0x00ffffff;
//    breakpoint_count++;
//}

void set_execution_breakpoint(uint32_t effective_address)
{
    struct breakpoint_t *breakpoint = breakpoints + breakpoint_count;
    breakpoint_count++;
    
    breakpoint->type = BREAKPOINT_TYPE_EXECUTION;
    breakpoint->address = effective_address;
    
    printf("breakpoint set on address %x\n", effective_address);
}

void set_register_breakpoint(uint32_t reg, uint32_t value)
{
    struct breakpoint_t *breakpoint = breakpoints + breakpoint_count;
    breakpoint_count++;
    
    breakpoint->type = BREAKPOINT_TYPE_REGISTER;
    breakpoint->reg = reg;
    breakpoint->value = value;
    
    printf("breakpoint set on register %s, for value %x\n", breakpoint_register_names[reg], value);
}

void clear_breakpoints()
{
    breakpoint_count = 0;
    printf("breakpoints cleared\n");
}

void reset_emu()
{
//    if(!window)
//    {
//        window = SDL_CreateWindow("bluh", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, 0);
//        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
//    }
//    
    reset_cpu();
}

uint32_t step_emu()
{
    uint32_t cpu_cycles = step_cpu();
    step_ppu(cpu_cycles);
    
    for(uint32_t breakpoint_index = 0; breakpoint_index < breakpoint_count; breakpoint_index++)
    {
        struct breakpoint_t *breakpoint = breakpoints + breakpoint_index;
        
        switch(breakpoint->type)
        {
            case BREAKPOINT_TYPE_EXECUTION:
                if(breakpoint->address == EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_state.reg_pc))
                {
                    return 0;
                }
            break;
            
            case BREAKPOINT_TYPE_REGISTER:
            {
                uint32_t reg_value;
                
                switch(breakpoint->reg)
                {
                    case BREAKPOINT_REGISTER_Y:
                        reg_value = cpu_state.reg_y.reg_y;
                    break;
                    
                    case BREAKPOINT_REGISTER_YL:
                        reg_value = cpu_state.reg_y.reg_yL;
                    break;
                    
                    case BREAKPOINT_REGISTER_YH:
                        reg_value = cpu_state.reg_y.reg_yH;
                    break;
                    
                    case BREAKPOINT_REGISTER_X:
                        reg_value = cpu_state.reg_x.reg_x;
                    break;
                }
                
                if(breakpoint->value == reg_value)
                {
                    return 0;
                }
            }
            break;
        }
    }
    
    return 1;
}
