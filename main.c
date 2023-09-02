#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

// extern uint32_t             interactive_mode;
// extern uint32_t             animated_mode;

extern FILE *               trace_file;
extern GLuint               emu_framebuffer_texture;
extern uint32_t             emu_window_width;
extern uint32_t             emu_window_height;
extern uint8_t *            vram;
extern uint8_t *            cgram;
extern uint8_t *            ram1_regs;
extern struct background_t  backgrounds[4];


struct viewer_tile_t
{
    struct dot_t pixels[64];
};


GLuint                  m_tile_viewer_texture;
struct dot_t *          m_tile_viewer_dots;
// struct viewer_tile_t *  m_tile_viewer_tiles;
#define                 M_TILE_VIEWER_TILES_PER_ROW 8
#define                 M_TILE_VIEWER_MAX_TILE_ROWS 256


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

    // m_tile_viewer_tiles = calloc(M_TILE_VIEWER_TILES_PER_ROW * M_TILE_VIEWER_MAX_TILE_ROWS, sizeof(struct viewer_tile_t));

    // for(uint32_t tile_index = 0; tile_index < M_TILE_VIEWER_TILES_PER_ROW * M_TILE_VIEWER_MAX_TILE_ROWS; tile_index++)
    // {
    //     for(uint32_t pixel_index = 0; pixel_index < 64; pixel_index++)
    //     {
    //         m_tile_viewer_tiles[tile_index].pixels[pixel_index].a = 255;
    //     }
    // }

    m_tile_viewer_dots = calloc(8 * 8 * M_TILE_VIEWER_TILES_PER_ROW * M_TILE_VIEWER_MAX_TILE_ROWS, sizeof(struct dot_t));

    glGenTextures(1, &m_tile_viewer_texture);
    glBindTexture(GL_TEXTURE_2D, m_tile_viewer_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8 * M_TILE_VIEWER_TILES_PER_ROW , 8 * M_TILE_VIEWER_MAX_TILE_ROWS, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_tile_viewer_dots);

    reset_emu();

    uint32_t run_emulation = 0;
    uint32_t tile_bits_per_pixel = 4;
    uint32_t tile_viewer_first_row = 0;
    uint32_t tile_viewer_last_row = 0;
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


        igSetNextWindowSize((ImVec2){emu_window_width, emu_window_height - 18}, 0);
        igSetNextWindowPos((ImVec2){0, 18}, 0, (ImVec2){0, 0});
        igBegin("##main_dockspace_indow", NULL, ImGuiWindowFlags_NoDecoration);
        {
            ImGuiID main_dockspace = igGetID_Str("main_dockspace");
            ImVec2 dockspace_size;
            igGetContentRegionAvail(&dockspace_size);

            if(igDockBuilderGetNode(main_dockspace) == NULL)
            {
                igDockBuilderAddNode(main_dockspace, ImGuiDockNodeFlags_DockSpace | ImGuiWindowFlags_NoBackground);
                igDockBuilderSetNodeSize(main_dockspace, dockspace_size);

                ImGuiID left_node;
                ImGuiID right_node;
                ImGuiID bottom_node;

                igDockBuilderSplitNode(main_dockspace, ImGuiDir_Right, 0.4f, &right_node, &left_node);
                igDockBuilderSplitNode(left_node, ImGuiDir_Down, 0.3f, &bottom_node, &left_node);
                igDockBuilderDockWindow("##framebuffer", left_node);
                igDockBuilderDockWindow("##data_viewer", right_node);
                igDockBuilderDockWindow("##trace", bottom_node);
                igDockBuilderFinish(main_dockspace);
            }

            igDockSpace(main_dockspace, dockspace_size, ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoTabBar, NULL);

            if(igBegin("##data_viewer", NULL, ImGuiWindowFlags_NoScrollbar))
            {
                ImVec2 window_size;
                igGetContentRegionAvail(&window_size);
                if(igBeginTabBar("Views", 0))
                {
                    if(igBeginTabItem("CPU", NULL, 0))
                    {
                        igEndTabItem();
                    }

                    if(igBeginTabItem("VRAM (Raw)", NULL, 0))
                    {
                        if(igBeginTable("##vram_raw", 17, ImGuiTableFlags_ScrollY, (ImVec2){0, 0}, 0.0))
                        {
                            ImGuiListClipper clipper;
                            ImGuiListClipper_ImGuiListClipper(&clipper);
                            ImGuiListClipper_Begin(&clipper, 17 * (0xffff / 16), 1.0f);
                            while(ImGuiListClipper_Step(&clipper))
                            {
                                uint32_t first_row = clipper.DisplayStart / 17;
                                uint32_t last_row = clipper.DisplayEnd / 17;

                                uint32_t vram_offset = first_row * 16;
                                igTableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 48.0f, 0);
                                for(uint32_t row_index = first_row; row_index < last_row; row_index++)
                                {
                                    igTableNextRow(0, 0);
                                    igTableNextColumn();
                                    igText("0x%04x: ", vram_offset);

                                    for(uint32_t column_index = 0; column_index < 16; column_index++)
                                    {
                                        igTableNextColumn();
                                        igText("%02x", vram[column_index + vram_offset]);
                                    }

                                    vram_offset += 16;
                                }
                            }
                            igEndTable();
                        }

                        igEndTabItem();
                    }
                    if(igBeginTabItem("VRAM (Tiles)", NULL, 0))
                    {
                        if(igRadioButton_Bool("2 bpp", tile_bits_per_pixel == 2))
                        {
                            tile_bits_per_pixel = 2;
                        }
                        igSameLine(0.0f, -1.0f);
                        if(igRadioButton_Bool("4 bpp", tile_bits_per_pixel == 4))
                        {
                            tile_bits_per_pixel = 4;
                        }
                        igSameLine(0.0f, -1.0f);
                        if(igRadioButton_Bool("8 bpp", tile_bits_per_pixel == 8))
                        {
                            tile_bits_per_pixel = 8;
                        }

                        igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0, 0});
                        igPushStyleVar_Vec2(ImGuiStyleVar_ItemSpacing, (ImVec2){4, 0});
                        igPushStyleVar_Vec2(ImGuiStyleVar_CellPadding , (ImVec2){0, 2});
                        if(igBeginTable("##vram_tiles", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoPadInnerX, (ImVec2){0, 0}, 0.0))
                        {
                            uint32_t tile_count = 0x10000 / (8 * tile_bits_per_pixel);
                            ImGuiListClipper clipper;
                            ImGuiListClipper_ImGuiListClipper(&clipper);
                            ImGuiListClipper_Begin(&clipper, tile_count / M_TILE_VIEWER_TILES_PER_ROW, -1.0f);
                            
                            uint32_t last_row = 0;
                            uint32_t row_count = 0;

                            while(ImGuiListClipper_Step(&clipper))
                            {   

                                last_row = clipper.DisplayEnd;
                                // row_count = clipper.ItemsCount;

                                for(uint32_t row_index = clipper.DisplayStart; row_index < clipper.DisplayEnd; row_index++)
                                {
                                    uint32_t vram_offset = (row_index * tile_bits_per_pixel * 8) * M_TILE_VIEWER_TILES_PER_ROW;
                                    igTableNextRow(0, 0);
                                    igTableNextColumn();
                                    igText("0x%04x: ", vram_offset);
                                    igSameLine(0.0f, -1.0f);

                                    ImVec2 uv0;
                                    ImVec2 uv1;
                                    uv0.y = (float)(row_index % M_TILE_VIEWER_MAX_TILE_ROWS) / (float)M_TILE_VIEWER_MAX_TILE_ROWS;
                                    uv1.y = uv0.y + 8.0f / (float)(M_TILE_VIEWER_MAX_TILE_ROWS * 8);
                                    for(uint32_t column_index = 0; column_index < M_TILE_VIEWER_TILES_PER_ROW; column_index++)
                                    {
                                        uv0.x = (float)(column_index * 8) / (float)(M_TILE_VIEWER_TILES_PER_ROW * 8);
                                        uv1.x = uv0.x + 8.0f / (float)(M_TILE_VIEWER_TILES_PER_ROW * 8);
                                        igImage((ImTextureID)(uintptr_t)m_tile_viewer_texture, (ImVec2){32, 32}, uv0, uv1, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                                        igSameLine(0.0f, -1.0f);
                                    }
                                    row_count++;
                                }
                            }

                            uint32_t update_start = last_row - row_count;
                            uint32_t update_end = last_row;

                            struct mode0_cgram_t *mode0_cgram = (struct mode0_cgram_t *)cgram;

                            uint32_t changed_row_count = update_end - update_start;
                            // printf("update %d rows\n", changed_row_count);
                            for(uint32_t row_index = 0; row_index < changed_row_count; row_index++)
                            {
                                // uint32_t vram_offset = (row_index * tile_bits_per_pixel * 8) * M_TILE_VIEWER_TILES_PER_ROW;

                                struct dot_t *first_dot = m_tile_viewer_dots + row_index * M_TILE_VIEWER_TILES_PER_ROW * 8 * 8;
                                for(uint32_t dot_index = 0; dot_index < 8 * 8 * M_TILE_VIEWER_TILES_PER_ROW; dot_index++)
                                {
                                    uint32_t tile_index = (dot_index >> 3) % 8 + ((row_index + update_start) << 3);
                                    struct dot_t *dot = first_dot + dot_index;
                                    uint32_t dot_x = dot_index % 8;
                                    uint32_t dot_y = dot_index / (8 * M_TILE_VIEWER_TILES_PER_ROW);

                                    uint8_t color_index = chr16_dot(vram, tile_index, dot_x, dot_y);
                                    if(color_index > 0)
                                    {
                                        struct col_t color = pal16_col(&mode0_cgram->obj_colors, 0, color_index);
                                        dot->r = color.r;
                                        dot->g = color.g;
                                        dot->b = color.b;
                                        dot->a = 255;
                                    }
                                    else
                                    {
                                        dot->a = 0;
                                    }
                                }
                            }

                            update_start %= M_TILE_VIEWER_MAX_TILE_ROWS;
                            update_end %= M_TILE_VIEWER_MAX_TILE_ROWS;

                            uint32_t copy_size = changed_row_count * 8;
                            glBindTexture(GL_TEXTURE_2D, m_tile_viewer_texture);

                            if(update_end < update_start)
                            {
                                uint32_t sub_copy_size = update_end * 8;
                                copy_size -= sub_copy_size;
                                struct dot_t *first_dot = m_tile_viewer_dots + copy_size * 8 * M_TILE_VIEWER_TILES_PER_ROW;
                                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8 * M_TILE_VIEWER_TILES_PER_ROW, sub_copy_size, GL_RGBA, GL_UNSIGNED_BYTE, first_dot);    
                            }

                            uint32_t copy_start = update_start * 8;
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, copy_start, 8 * M_TILE_VIEWER_TILES_PER_ROW, copy_size, GL_RGBA, GL_UNSIGNED_BYTE, m_tile_viewer_dots);
    
                            igEndTable();
                        }
                        igPopStyleVar(3);

                        igEndTabItem();
                    }

                    if(igBeginTabItem("Backgrounds", NULL, 0))
                    {
                        uint8_t bg_mode = ram1_regs[PPU_REG_BGMODE] & PPU_BGMODE_MODE_MASK;
                        uint8_t chr_size = ram1_regs[PPU_REG_BGMODE] >> PPU_BGMODE_BG1_CHR_SIZE_SHIFT;

                        igText("BG mode: %d", bg_mode);
                        // char label[64];
                        for(uint32_t background_index = 0; background_index < 4; background_index++)
                        {
                            // sprintf(label, "BG %d", background_index);
                            struct background_t *background = backgrounds + background_index;
                            uint32_t screen_size = ram1_regs[PPU_REG_BG1SC + background_index] & 0x3;
                            igPushID_Int(background_index);
                            if(igBeginChild_Str("Background", (ImVec2){0, 128}, 1, 0))
                            {    
                                igText("X: %d, Y: %d", background->offset.offsets[0], background->offset.offsets[1]);
                                igText("Screen size: %d", screen_size);
                                igText("Chr size: %s", (chr_size & PPU_BGMODE_BG_CHR_SIZE_MASK) ? "16x16" : "8x8");
                                igText("Name base address: 0x%04x", (uintptr_t)background->chr_base - (uintptr_t)vram);
                                igText("Screen data base address: 0x%04x", (uintptr_t)background->data_base[0] - (uintptr_t)vram);
                            }
                            igEndChild();
                            igPopID();

                            chr_size >>= 1;
                        }
                        igEndTabItem();
                    }

                    igEndTabBar();
                }
            }
            igEnd();

            // ImVec2 window_size = (ImVec2){FRAMEBUFFER_WIDTH * 2, FRAMEBUFFER_HEIGHT * 2};
            // igSetNextWindowSize(window_size, 0);
            if(igBegin("##framebuffer", NULL, ImGuiWindowFlags_NoScrollbar))
            {
                ImVec2 window_size;
                igGetContentRegionAvail(&window_size);
                igImage((ImTextureID)(uintptr_t)emu_framebuffer_texture, window_size, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){0, 0, 0, 0});
            }
            igEnd();

            if(igBegin("##trace", NULL, 0))
            {

            }
            igEnd();


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
