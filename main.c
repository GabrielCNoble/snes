#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

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
        return -1;
    }
    
//    reset_emu();
    
    if(argc > 1)
    {
        if(!strcmp(argv[1], "-i"))
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
                        if(load_rom_file(param))
                        {
                            reset_emu();
                            dump_cpu(1);
                        }
                    }
                    else
                    {
                        if(!strcmp(param, "-step"))
                        {
                            step_emu();
                            dump_cpu(1);
                            dump_ppu();
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
                        else if(!strcmp(param, "-run"))
                        {
                            while(step_emu());
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
                            while(step_emu())
                            {
//                                SDL_Delay(1);
                                dump_cpu(1);
                            }
                        }
                        else if(!strcmp(param, "-breakpoint"))
                        {
                            scanf("%s", param);
                            
                            if(!strcmp(param, "e"))
                            {
                                uint32_t address;
                                scanf("%x", &address);
                                set_execution_breakpoint(address);
                            }
                            else if(!strcmp(param, "r"))
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
                            else if(!strcmp(param, "c"))
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
//        rom = load_rom_file(argv[1]);
//        //dump_rom(&rom);
//        //disassable(rom.rom_buffer + rom.reset_vector, 256);
//        map_rom(&rom);
    }

    //load_rom("final_fantasy_v.smc");
    
    return 0;
}
