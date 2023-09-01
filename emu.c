#include <stdio.h>
#include <string.h>
#include "emu.h"
#include "ui.h"
#include "SDL2/SDL.h"
#include "GL/glew.h"

uint32_t breakpoint_count = 0;
struct breakpoint_t breakpoints[512];

SDL_Window *    emu_window = NULL;
SDL_GLContext   emu_context;
SDL_Renderer *  renderer = NULL;
SDL_Texture *   backbuffer_textures[2] = {};
uint32_t        run_window_thread = 1;
uint32_t        blit_backbuffer_texture = 0;
uint32_t        get_input = 0;
uint64_t        counter_frequency;
uint64_t        prev_count = 0;
SDL_atomic_t    blit_semaphore = {0};
uint64_t        master_cycles = 0;
//  uint32_t        mem_refresh_state = 0;
//uint32_t        scanline_cycles = 0;
// int32_t         mem_refresh_cyles = 0;
// int32_t         mem_refresh_start = 538;

uint32_t    window_width = 800;
uint32_t    window_height = 600;
uint32_t    interactive_mode = 0;
uint32_t    animated_mode = 0;
FILE *      trace_file;
uint64_t    frame = 0;
float       accum_time = 0;

uint32_t    read_address_buffer[ADDRESS_BUFFER_SIZE];
uint32_t    read_address_count = 0;
uint32_t    write_address_buffer[ADDRESS_BUFFER_SIZE];
uint32_t    write_address_count = 0;

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
uint32_t                        cur_framebuffer = 0;
struct dot_t *                  framebuffers[2];
extern uint32_t                 cpu_cycle_count;
extern uint8_t                  active_channels;
extern uint16_t                 hcounter;
extern uint16_t                 vcounter;
extern uint32_t                 scanline_cycles;
extern uint32_t                 halt;

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

void set_read_write_breakpoint(uint32_t type, uint32_t address)
{
    struct breakpoint_t *breakpoint = breakpoints + breakpoint_count;
    breakpoint_count++;

    breakpoint->type = type;
    breakpoint->value = address;

    if(type == BREAKPOINT_TYPE_READ)
    {
        printf("breakpoint set for reads from address %x\n", address);
    }
    else
    {
        printf("breakpoint set for writes to address %x\n", address);
    }
}

void clear_breakpoints()
{
    breakpoint_count = 0;
    printf("breakpoints cleared\n");
}

void blit_backbuffer()
{
    SDL_UpdateTexture(backbuffer_textures[cur_framebuffer], NULL, framebuffer, sizeof(struct dot_t) * FRAMEBUFFER_WIDTH);
    SDL_RenderCopy(renderer, backbuffer_textures[cur_framebuffer], NULL, NULL);
    SDL_RenderPresent(renderer);
    cur_framebuffer ^= 1;
    framebuffer = framebuffers[cur_framebuffer];
}

void init_emu()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    emu_window = SDL_CreateWindow("snes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_OPENGL);
    emu_context = SDL_GL_CreateContext(emu_window);
    SDL_GL_MakeCurrent(emu_window, emu_context);
    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if(status != GLEW_OK)
    {
        printf("couldn't initialize glew (%d)\n%s\n", status, glewGetErrorString(status));
        exit(-1);
    }

    // renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    // SDL_RenderSetVSync(renderer, 1);
    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_RenderClear(renderer);
    // SDL_RenderPresent(renderer);

    framebuffers[0] = calloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, sizeof(struct dot_t));
    framebuffers[1] = calloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, sizeof(struct dot_t));

    framebuffer = framebuffers[cur_framebuffer];

    // backbuffer_textures[0] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
    // backbuffer_textures[1] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
    // SDL_SetTextureScaleMode(backbuffer_textures[0], SDL_ScaleModeLinear);
    // SDL_SetTextureScaleMode(backbuffer_textures[1], SDL_ScaleModeLinear);
    // SDL_UpdateTexture(backbuffer_textures[cur_framebuffer], NULL, framebuffer, sizeof(struct dot_t) * FRAMEBUFFER_WIDTH);
    // SDL_RenderCopy(renderer, backbuffer_textures[cur_framebuffer], NULL, NULL);
    // SDL_RenderPresent(renderer);

    counter_frequency = SDL_GetPerformanceFrequency();
    prev_count = SDL_GetPerformanceCounter();
    ui_Init();
    init_apu();
    init_ppu();
    init_mem();
    init_dma();
}

void shutdown_emu()
{
    run_window_thread = 0;
    free(framebuffers[0]);
    free(framebuffers[1]);
    shutdown_ppu();
    shutdown_mem();
}

void reset_emu()
{
    master_cycles = 0;
    reset_cpu();
    reset_ppu();
}

uint32_t step_emu(int32_t step_cycles)
{
    uint32_t status = 0;
//    uint32_t hdma_active = ram1_regs[CPU_REG_HDMAEN] && ((ram1_regs[CPU_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) == CPU_HVBJOY_FLAG_HBLANK);

    if(!ram1_regs[CPU_REG_MDMAEN])
    {
        if(step_cpu(&step_cycles))
        {
            status |= EMU_STATUS_END_OF_INSTRUCTION;
        }
    }
    else
    {
        status |= EMU_STATUS_END_OF_INSTRUCTION;
    }

    step_dma(step_cycles);
    step_hdma(step_cycles);
    step_apu(step_cycles);
    step_ctrl(step_cycles);

    if(step_ppu(step_cycles) || animated_mode)
    {
        frame++;
        uint64_t cur_count = SDL_GetPerformanceCounter();
        accum_time += (float)(cur_count - prev_count) / (float)counter_frequency;
//        printf("frame time: %f ms\n", delta * 1000.0);
        prev_count = cur_count;

        if(frame >= 60)
        {
            frame = 0;
            accum_time /= 60.0;
            printf("frame time: %f ms\n", accum_time * 1000.0);
            accum_time = 0.0;
        }
        blit_backbuffer();
    }

    if(scanline_cycles >= 538 && scanline_cycles < 578)
    {
        /* dram refresh */
        deassert_rdy(0);
    }
    else
    {
        assert_rdy(0);
    }

   if(ram1_regs[CPU_REG_TIMEUP] & 0x80)
   {
       assert_irq(1);
   }
   else
   {
       deassert_irq(1);
   }

    master_cycles += step_cycles;

    if(status & EMU_STATUS_END_OF_INSTRUCTION)
    {
        uint32_t read_count = read_address_count;
        uint32_t write_count = write_address_count;

        write_address_count = 0;
        read_address_count = 0;

        for(uint32_t breakpoint_index = 0; breakpoint_index < breakpoint_count; breakpoint_index++)
        {
            struct breakpoint_t *breakpoint = breakpoints + breakpoint_index;

            switch(breakpoint->type)
            {
                case BREAKPOINT_TYPE_EXECUTION:
                    if(breakpoint->address == EFFECTIVE_ADDRESS(cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word))
                    // if(breakpoint->address == cpu_state.instruction_address)
                    {
                        return status | EMU_STATUS_BREAKPOINT;
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
                        return status | EMU_STATUS_BREAKPOINT;
                    }
                }
                break;

                case BREAKPOINT_TYPE_READ:
                {
                    for(uint32_t index = 0; index < read_count; index++)
                    {
                        if(breakpoint->value == read_address_buffer[index])
                        {
                            return status | EMU_STATUS_BREAKPOINT;
                        }
                    }
                }
                break;

                case BREAKPOINT_TYPE_WRITE:
                {
                    for(uint32_t index = 0; index < write_count; index++)
                    {
                        if(breakpoint->value == write_address_buffer[index])
                        {
                            return status | EMU_STATUS_BREAKPOINT;
                        }
                    }
                }
                break;
            }
        }

        if(halt)
        {
            halt = 0;
            status |= EMU_STATUS_BREAKPOINT;
        }
    }

    return status;
}

void dump_emu()
{
    printf("master cycles: %llu\n", master_cycles);
    dump_dma();
    dump_ppu();
    dump_cpu(1);
    printf("\n");
}

void write_trace()
{
//    fprintf(trace_file, "[%llu]: %s\n", master_cycles, instruction_str(cpu_state.instruction_address));
    fprintf(trace_file, "%s\n", instruction_str2(cpu_state.instruction_address));
}
