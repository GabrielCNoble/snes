#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SDL_MAIN_HANDLED
#include "SDL/include/SDL2/SDL.h"


int main(int argc, char *argv[])
{
    char param[512];
    unsigned int poke_addr;
    unsigned int poke_value;
    int disasm_start;
    int disasm_count;
    uint32_t step_count;
    
//    SDL_Init(SDL_INIT_TIMER);
//    printf("%d\n", SDL_GetPerformanceFrequency());
    
    reset_emu();
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
                        load_rom_file(param);
//                        reset_cpu();
                        reset_emu();
                        disassemble_current(1);
                    }
                    else
                    {
                        if(!strcmp(param, "-dump"))
                        {
                            scanf("%s", param);

                            if(!strcmp(param, "-start"))
                            {
                                scanf("%x", &disasm_start);
                            }
                            else
                            {
                                disasm_start = -1;
                            }

                            scanf("%s", param);

                            if(!strcmp(param, "-count"))
                            {
                                scanf("%x", &disasm_count);
                            }
                            else
                            {
                                disasm_count = 1024;
                            }

                            disassemble(disasm_start, disasm_count);
                            
                        }
                        else if(!strcmp(param, "-step"))
                        {
                            step_emu();
                            disassemble_current(1);
                        }
                        else if(!strcmp(param, "-stepc"))
                        {
                            scanf("%d", &step_count);
                            while(step_count)
                            {
                                step_emu();
                                step_count--;
                            }
                            disassemble_current(1);
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
                            while(1)
                            {
                                step_emu();
                                //disassemble_current(1);
                            }
                        }
                        else if(!strcmp(param, "-reset"))
                        {
                            reset_emu();
                            disassemble_current(1);
                        }
                        else if(!strcmp(param, "-drv"))
                        {

                        }
                        else if(!strcmp(param, "-h"))
                        {

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
}
