#include <stdio.h>
#include <string.h>
#include "emu.h"
#include "SDL2/SDL.h"

uint32_t breakpoint_count = 0;
struct breakpoint_t breakpoints[512];

SDL_Window *    window = NULL;
SDL_Renderer *  renderer = NULL;
SDL_Texture *   backbuffer_texture = NULL;
uint32_t        run_window_thread = 1;
uint32_t        blit_backbuffer_texture = 0;
uint32_t        get_input = 0;
uint64_t        counter_frequency;
uint64_t        prev_count = 0;
SDL_atomic_t    blit_semaphore = {0};
uint64_t        master_cycles = 0;
 uint32_t        mem_refresh_state = 0;
//uint32_t        scanline_cycles = 0;
int32_t         mem_refresh = 0;

uint32_t window_width = 800;
uint32_t window_height = 600;
uint32_t interactive_mode = 0;
uint32_t animated_mode = 0;

char *breakpoint_register_names[] =
{
    [BREAKPOINT_REGISTER_Y] = "Y",
    [BREAKPOINT_REGISTER_X] = "X"
};

extern struct cpu_state_t cpu_state;

extern uint8_t *                ram1_regs;
extern uint8_t *                ram2;
extern struct mem_write_t *     reg_writes;
extern struct mem_read_t *      reg_reads;
extern struct dot_t *           framebuffer;
extern uint32_t                 cpu_cycle_count;
extern uint8_t                  active_channels;
extern uint16_t                 hcounter;
extern uint16_t                 vcounter;
extern uint32_t                 scanline_cycles;

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

int window_thread(void *data)
{
    window = SDL_CreateWindow("snes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
//
    backbuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    while(run_window_thread)
    {
        SDL_Event event;
        SDL_PollEvent(&event);

        if(SDL_AtomicGet(&blit_semaphore))
        {
            // SDL_AtomicLock(&backbuffer_texture_spinlock);
            SDL_UpdateTexture(backbuffer_texture, NULL, framebuffer, sizeof(struct dot_t) * FRAMEBUFFER_WIDTH);
            SDL_RenderCopy(renderer, backbuffer_texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            SDL_AtomicSet(&blit_semaphore, 0);
            // blit_backbuffer_texture = 0;
            // SDL_AtomicUnlock(&backbuffer_texture_spinlock);
            // SDL_UnlockMutex(backbuffer_texture_mutex);
        //    uint64_t current_count = SDL_GetPerformanceCounter();
        //    double delta = (double)(current_count - prev_count) / (double)counter_frequency;
        //    printf("frame time: %f ms\n", delta * 1000.0);
//
//
        //    prev_count = current_count;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return 0;
}

void blit_backbuffer()
{
//    SDL_AtomicSet(&blit_semaphore, 1);
//    while(SDL_AtomicGet(&blit_semaphore) == 1);
    SDL_UpdateTexture(backbuffer_texture, NULL, framebuffer, sizeof(struct dot_t) * FRAMEBUFFER_WIDTH);
    SDL_RenderCopy(renderer, backbuffer_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void init_emu()
{
//    SDL_DetachThread(SDL_CreateThread(window_thread, "window thread", NULL));

    window = SDL_CreateWindow("snes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    backbuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

    SDL_UpdateTexture(backbuffer_texture, NULL, framebuffer, sizeof(struct dot_t) * FRAMEBUFFER_WIDTH);
    SDL_RenderCopy(renderer, backbuffer_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
//    SDL_AtomicSet(&blit_semaphore, 0);

    counter_frequency = SDL_GetPerformanceFrequency();
    prev_count = SDL_GetPerformanceCounter();
    init_apu();
    init_ppu();
    init_mem();
    init_dma();
}

void shutdown_emu()
{
    run_window_thread = 0;
    shutdown_ppu();
    shutdown_mem();
}

void reset_emu()
{
    master_cycles = 0;
    reset_cpu();
    reset_ppu();
}

uint32_t step_emu()
{
    int32_t step_cycles = 0;
    uint32_t hdma_active = ram1_regs[CPU_REG_HDMAEN] && ((ram1_regs[CPU_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) == CPU_HVBJOY_FLAG_HBLANK);

//    mem_refresh = scanline_cycles >= 536 && scanline_cycles <= 536 + 40;
    step_cycles = 4;

    if((!ram1_regs[CPU_REG_MDMAEN] || !hdma_active))
    {
        step_cpu(step_cycles);
    }

    step_dma(step_cycles);
    step_hdma(step_cycles);
    step_apu(step_cycles);
    step_ctrl(step_cycles);

    if(step_ppu(step_cycles) || animated_mode)
    {
        uint64_t cur_count = SDL_GetPerformanceCounter();
        float delta = (float)(cur_count - prev_count) / (float)counter_frequency;
        printf("frame time: %f ms\n", delta * 1000.0);
        prev_count = cur_count;
        blit_backbuffer();
    }

    uint32_t scanline_cycle = vcounter == 0 ? 536 : 538;

//    mem_refresh = scanline_cycles >= scanline_cycle && scanline_cycles < scanline_cycle + 40;

    if(scanline_cycles >= scanline_cycle && scanline_cycles < scanline_cycle + 40)
    {
        assert_pin(CPU_PIN_RDY);
    }
    else
    {
        deassert_pin(CPU_PIN_RDY);
    }

   if(ram1_regs[CPU_REG_TIMEUP] & 0x80)
   {
       assert_pin(CPU_PIN_IRQ);
   }
   else
   {
       deassert_pin(CPU_PIN_IRQ);
   }

    master_cycles += step_cycles;

    for(uint32_t breakpoint_index = 0; breakpoint_index < breakpoint_count; breakpoint_index++)
    {
        struct breakpoint_t *breakpoint = breakpoints + breakpoint_index;

        switch(breakpoint->type)
        {
            case BREAKPOINT_TYPE_EXECUTION:
                // if(breakpoint->address == EFFECTIVE_ADDRESS(cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word))
                if(breakpoint->address == cpu_state.instruction_address)
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
                        reg_value = cpu_state.regs[REG_Y].word;
                    break;

                    case BREAKPOINT_REGISTER_YL:
                        reg_value = cpu_state.regs[REG_Y].byte[0];
                    break;

                    case BREAKPOINT_REGISTER_YH:
                        reg_value = cpu_state.regs[REG_Y].byte[1];
                    break;

                    case BREAKPOINT_REGISTER_X:
                        reg_value = cpu_state.regs[REG_X].word;
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

void dump_emu()
{
    printf("master cycles: %llu\n", master_cycles);
    dump_ppu();
    dump_cpu(1);
    printf("\n");
}
