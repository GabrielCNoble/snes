#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
    struct rom_t rom;
    char param[512];

    reset_cpu();

    rom.header = NULL;

    int disasm_start;
    int disasm_count;

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
                    if(!strcmp(param, "-r"))
                    {
                        if(rom.header)
                        {
                            free(rom.base);
                        }

                        scanf("%s", param);
                        rom = load_rom_file(param);
                        if(rom.header)
                        {
                            map_rom(&rom);
                        }
                    }
                    else
                    {
                        if(!strcmp(param, "-d"))
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
