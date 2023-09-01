#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

extern uint32_t         interactive_mode;
extern uint32_t         animated_mode;
extern FILE *           trace_file;
extern GLuint           emu_framebuffer_texture;
extern uint8_t *        vram;


int main(int argc, char *argv[])
{
    char param[512];
    unsigned int poke_addr;
    unsigned int poke_value;
    int disasm_start;
    int disasm_count;
    uint32_t step_count;

    uint32_t breakpoint_count = 0;
    uint32_t breakpoints[32];

    if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("error: %s\n", SDL_GetError());
        exit(-1);
    }

    init_emu();

    if(!load_cart("./rom/super_metroid_ntsc.sfc"))
    {
        printf("couldn't load rom\n");
    }

    reset_emu();

    uint32_t run_emulation = 0;
    uint32_t vram_offset = 0;

    while(!in_ReadInput())
    {
        ui_Begin();

        if(igBeginMainMenuBar())
        {
            if(igBeginMenu("Emulation", 1))
            {
                if(igMenuItem_Bool("Run", NULL, 0, 1))
                {
                    run_emulation = 1;
                }

                if(igMenuItem_Bool("Halt", NULL, 0, 1))
                {
                    run_emulation = 0;
                }
                igEndMenu();
            }

            igEndMainMenuBar();   
        }

        if(run_emulation)
        {
            while(!(step_emu(4) & EMU_STATUS_END_OF_FRAME));
        }

        vram_offset = 0;
        if(igBegin("vram", NULL, ImGuiWindowFlags_NoScrollbar))
        {
            if(igBeginTabBar("Views", 0))
            {
                if(igBeginTabItem("Raw", NULL, 0))
                {
                    if(igBeginTable("##vram", 16, ImGuiTableFlags_ScrollY, (ImVec2){0, 0}, 0.0))
                    {
                        for(uint32_t row_index = 0; row_index < 256; row_index++)
                        {
                            igTableNextRow(0, 0);
                            for(uint32_t column_index = 0; column_index < 16; column_index++)
                            {
                                igTableNextColumn();
                                igText("%02x", vram[column_index + vram_offset]);
                            }

                            vram_offset += 16;
                        }
                        igEndTable();
                    }

                    igEndTabItem();
                }
                if(igBeginTabItem("Graphics", NULL, 0))
                {
                    igEndTabItem();
                }
                igEndTabBar();
            }
        }
        igEnd();

        ImVec2 window_size = (ImVec2){FRAMEBUFFER_WIDTH * 2, FRAMEBUFFER_HEIGHT * 2};
        igSetNextWindowSize(window_size, 0);
        if(igBegin("framebuffer", NULL, ImGuiWindowFlags_NoScrollbar))
        {
            igImage((ImTextureID)(uintptr_t)emu_framebuffer_texture, window_size, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){0, 0, 0, 0});
        }
        igEnd();
        
        ui_End();
    }

/*
    if(argc > 1)
    {
        init_emu();
        if(!strcmp(argv[1], "-i"))
        {
            interactive_mode = 1;
        }

        if(interactive_mode)
        {
            do
            {
                scanf("%s", param);

                if(!strcmp(param, "-q"))
                {
                    printf("exit\n");
                    break;
                }
                else
                {
                    if(!strcmp(param, "-rom"))
                    {
                        scanf("%s", param);
                        if(load_cart(param))
                        {
                            reset_emu();
                            dump_emu();
                        }
                    }
                    else
                    {
                        if(!strcmp(param, "-stepi"))
                        {
                            while(!(step_emu(4) & EMU_STATUS_END_OF_INSTRUCTION));
                            dump_emu();
                        }
                        else if(!strcmp(param, "-stepc"))
                        {
                            step_emu(1);
                            dump_emu();
                        }
                        else if(!strcmp(param, "-stepn"))
                        {
                            uint32_t step_count;
                            scanf("%d", &step_count);
                            while(step_count)
                            {
                                step_emu(4);
                                step_count--;
                            }

                            dump_emu();
                        }
                        else if(!strcmp(param, "-trace"))
                        {
                            char file_name[64];
                            uint32_t step_count;
                            scanf("%s %d", file_name, &step_count);
                            trace_file = fopen(file_name, "w");
                            if(!trace_file)
                            {
                                printf("couldn't open trace file %s!\n", file_name);
                                return -1;
                            }

                            uint32_t status = 0;
                            while(step_count && (!(status & EMU_STATUS_BREAKPOINT)))
                            {

                                do
                                {
                                    status = step_emu(4);
                                }
                                while(!(status & EMU_STATUS_END_OF_INSTRUCTION));

                                write_trace();
                                step_count--;
                            }

                            fclose(trace_file);
                            trace_file = NULL;
                        }
                        else if(!strcmp(param, "-poke"))
                        {
                            scanf("%x", &poke_addr);
                            poke(poke_addr, NULL);
                        }
                        else if(!strcmp(param, "-pokev"))
                        {
                            scanf("%x", &poke_addr);
                            scanf("%x", &poke_value);
                            poke(poke_addr, &poke_value);
                        }
                        else if(!strcmp(param, "-h_regs"))
                        {
                            view_hardware_registers();
                        }
                        else if(!strcmp(param, "-vram"))
                        {
                            uint32_t start_addr;
                            uint32_t end_addr;
                            scanf("%32x %32x", &start_addr, &end_addr);
                            dump_vram(start_addr, end_addr);
                        }
                        else if(!strcmp(param, "-run"))
                        {
                            while(!(step_emu(4) & EMU_STATUS_BREAKPOINT));
                            dump_cpu(1);
                        }
                        else if(!strcmp(param, "-reset"))
                        {
                            reset_emu();
                            dump_cpu(1);
                        }
                        else if(!strcmp(param, "-drv"))
                        {

                        }
                        else if(!strcmp(param, "-h"))
                        {

                        }
                        else if(!strcmp(param, "-animate"))
                        {
                            animated_mode = 1;

                            while(step_emu(4))
                            {
                                 dump_cpu(1);
                            }
                        }
                        else if(!strcmp(param, "-breakpoint"))
                        {
                            scanf("%s", param);

                            if(!strcmp(param, "exec"))
                            {
                                uint32_t address;
                                scanf("%x", &address);
                                set_execution_breakpoint(address);
                            }
                            else if(!strcmp(param, "reg"))
                            {
                                uint32_t value;
                                scanf("%s", param);
                                scanf("%x", &value);
                                if(!strcmp(param, "Y"))
                                {
                                    set_register_breakpoint(BREAKPOINT_REGISTER_Y, value);
                                }
                                if(!strcmp(param, "X"))
                                {
                                    set_register_breakpoint(BREAKPOINT_REGISTER_X, value);
                                }
                            }
                            else if(!strcmp(param, "read"))
                            {
                                uint32_t value;
                                scanf("%x", &value);
                                set_read_write_breakpoint(BREAKPOINT_TYPE_READ, value);
                            }
                            else if(!strcmp(param, "write"))
                            {
                                uint32_t value;
                                scanf("%x", &value);
                                set_read_write_breakpoint(BREAKPOINT_TYPE_WRITE, value);
                            }
                            else if(!strcmp(param, "clear"))
                            {
                                clear_breakpoints();
                            }
                            else
                            {
                                printf("unknown type of breakpoint\n");
                            }
//                            uint32_t breakpoint;
//                            scanf("%x", &breakpoint);
//                            set_breakpoint(breakpoint);
                        }
                    }
                }

            }while(1);

        }
        else
        {

        }

        shutdown_emu();
    }

    */

    shutdown_emu();

    return 0;
}
