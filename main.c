#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


int main(int argc, char *argv[])
{
//    struct rom_t rom;
    char param[512];

    unsigned int poke_addr;
    unsigned int poke_value;

    //reset_cpu();

//    rom.header = NULL;

    int disasm_start;
    int disasm_count;

    // int32_t operand0 = 0;
    // int32_t operand1 = 0;
 
    // printf("%d\n", alu_op(0, 0, ALU_OP_ADD, 1));
    // printf("%d\n", alu_op(1, 1, ALU_OP_ADD, 1));
    // printf("%d\n", alu_op(1, -1, ALU_OP_ADD, 1));
    // printf("%d\n", (int8_t)alu_op(127, -128, ALU_OP_ADD, 1));
    // printf("%d\n", (int8_t)alu_op(127, 128, ALU_OP_SUB, 1));
    // printf("%d\n", (int8_t)alu_op(127, 1, ALU_OP_ADD, 1));
    // printf("%d\n", (int8_t)alu_op(-1, 1, ALU_OP_DEC, 1));
    // printf("%d\n", (int8_t)alu_op(127, 1, ALU_OP_INC, 1));
    // printf("%d\n", (int8_t)alu_op(-1, 1, ALU_OP_INC, 1));
    // printf("%d\n", (int8_t)alu_op(0x80, 2, ALU_OP_SHL, 1));
    // printf("%d\n", (int8_t)alu_op(0x0, 2, ALU_OP_ROR, 1));
    // printf("%d\n", alu_op(128, 1, ALU_OP_SUB, 1));
    reset_cpu();
//    uint32_t value = 1;
//    for(uint32_t i = 0; i < 128; i++)
//    {
//        value = alu(value, 1, ALU_OP_XOR, 1);
//        printf("%d\n", (int8_t)value);
//    }
//    
//
//    return 0;

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
//                        if(rom.header)
//                        {
//                            free(rom.base);
//                        }

                        scanf("%s", param);
                        load_rom_file(param);
                        reset_cpu();
                        disassemble_current(1);
                    }
                    else
                    {
                        if(!strcmp(param, "-dump"))
                        {
                            scanf("%s", param);

                            if(!strcmp(param, "-header"))
                            {
//                                dump_rom(&rom);
                            }
                            else
                            {
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


                        }
                        else if(!strcmp(param, "-step"))
                        {
                            step_cpu();
                            disassemble_current(1);
                        }
                        else if(!strcmp(param, "-poke"))
                        {
                            scanf("%x", &poke_addr);
                            poke(poke_addr, -1);
                        }
                        else if(!strcmp(param, "-pokev"))
                        {
                            scanf("%x", &poke_addr);
                            scanf("%x", &poke_value);
                            poke(poke_addr, poke_value);
                        }
                        else if(!strcmp(param, "-h_regs"))
                        {
                            view_hardware_registers();
                        }
                        else if(!strcmp(param, "-run"))
                        {
                            while(1)
                            {
                                step_cpu();
                                //disassemble_current(1);
                            }
                        }
                        else if(!strcmp(param, "-reset"))
                        {
                            reset_cpu();
                            printf("the cpu has been reset...\n");
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
