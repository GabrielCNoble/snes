#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

extern uint32_t interactive_mode;
extern uint32_t animated_mode;
extern FILE *trace_file;

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

    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("error: %s\n", SDL_GetError());
        exit(-1);
    }

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

    return 0;
}
