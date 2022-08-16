#include <stdio.h>
#include "ctrl.h"
#include "cpu/cpu.h"
#include "ppu.h"
#include "SDL2/SDL.h"

uint32_t controller_read = 0;
uint32_t button_index = 0;
extern uint8_t *ram1_regs;

SDL_GameController *game_controller;

union ctrl_t controllers[4] = {0};

void init_ctrl()
{

}

void step_ctrl(int32_t cycle_count)
{
    if(!(ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
    {
        controller_read = 0;
    }
    else
    {
        if(!controller_read)
        {
            SDL_Event event;

            while(SDL_PollEvent(&event))
            {
                switch(event.type)
                {
                    case SDL_CONTROLLERDEVICEADDED:
                    {
                        game_controller = SDL_GameControllerOpen(event.cdevice.which);
                    }
                    break;

                    case SDL_CONTROLLERDEVICEREMOVED:
                    {
                        SDL_GameControllerClose(game_controller);
                    }
                    break;

                    case SDL_CONTROLLERBUTTONUP:
                    case SDL_CONTROLLERBUTTONDOWN:
                    {
                        switch(event.cbutton.button)
                        {
                            case SDL_CONTROLLER_BUTTON_A:
                                controllers[0].a = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_B:
                                controllers[0].b = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_X:
                                controllers[0].x = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_Y:
                                controllers[0].y = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                                controllers[0].up = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                                controllers[0].down = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                                controllers[0].left = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                                controllers[0].right = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_START:
                                controllers[0].start = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_BACK:
                                controllers[0].select = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                                controllers[0].l = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;

                            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                                controllers[0].r = (event.type == SDL_CONTROLLERBUTTONDOWN);
                            break;
                        }
                    }
                    break;
                }
            }

            if(ram1_regs[CPU_REG_NMITIMEN] & CPU_NMITIMEN_FLAG_STD_CTRL_EN)
            {
                ram1_regs[CPU_REG_STDCTRL1L] = (controllers[0].a << 7) | (controllers[0].x << 6) | (controllers[0].l << 5) | (controllers[0].r << 4);
                ram1_regs[CPU_REG_STDCTRL1H] = (controllers[0].b << 7) | (controllers[0].y << 6) | (controllers[0].select << 5) | (controllers[0].start << 4);
                ram1_regs[CPU_REG_STDCTRL1H] |= (controllers[0].up << 3) | (controllers[0].down << 2) | (controllers[0].left << 1) | (controllers[0].right);
            }

            controller_read = 1;
            button_index = 0;
        }
    }
}

void ctrl_write(uint32_t effective_address, uint8_t value)
{
    if(value == 1)
    {
        button_index = 0;
    }
}

uint8_t ctrl_read(uint32_t effective_address)
{
    uint8_t status = controllers[0].buttons[button_index];
    button_index++;
//    printf("button: %d\n", status);
    return status;
}
