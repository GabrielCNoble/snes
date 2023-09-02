#include "input.h"
#include <stdio.h>
#include "ctrl.h"
#include "cpu/cpu.h"
#include "ppu.h"
#include "SDL2/SDL.h"
#include "ui.h"

// uint32_t controller_read = 0;
// uint32_t button_index = 0;
// extern uint8_t *ram1_regs;
// extern int32_t vram_offset;

SDL_GameController *    in_controller;
uint32_t                in_text_input;
extern union ctrl_t     controllers[4];
// uint32_t            in_controller_count = 0;

// union ctrl_t        controllers[4] = {0};
// uint32_t            halt = 0;

// uint32_t        ctrl_mouse_x;
// uint32_t        ctrl_mouse_y;

// void init_ctrl()
// {

// }

uint32_t in_ReadInput()
{
    SDL_Event event;
    uint32_t quit = 0;

    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_CONTROLLERDEVICEADDED:
            {
                in_controller = SDL_GameControllerOpen(event.cdevice.which);
            }
            break;

            case SDL_QUIT:
            case SDL_WINDOWEVENT_CLOSE:
                quit = 1;
            break;

            case SDL_CONTROLLERDEVICEREMOVED:
            {
                SDL_GameControllerClose(in_controller);
            }
            break;

            case SDL_MOUSEMOTION:
            {
                ui_MouseMoveEvent((float)event.motion.x, (float)event.motion.y);
            }
            break;

            case SDL_KEYDOWN:
                ui_KeyboardEvent(event.key.keysym.scancode, 1);
            break;

            case SDL_KEYUP:
                ui_KeyboardEvent(event.key.keysym.scancode, 0);
            break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                ui_MouseClickEvent(event.button.button, event.type == SDL_MOUSEBUTTONDOWN);
            break;

            case SDL_TEXTINPUT:
                for(uint32_t index = 0; event.text.text[index] && index < sizeof(event.text.text); index++)
                {
                    ui_TextInputEvent(event.text.text[index]);
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

    return quit;
}

uint32_t in_SetTextInput(uint32_t enable)
{
    enable = enable && 1;
    
    if(in_text_input != enable)
    {
        if(enable)
        {
            SDL_StartTextInput();
        }
        else
        {
            SDL_StopTextInput();
        }
    }

    in_text_input = enable;
}
