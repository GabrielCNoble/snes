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
extern GLuint                       emu_framebuffer_texture;
extern uint32_t                     emu_window_width;
extern uint32_t                     emu_window_height;
extern struct breakpoint_list_t     emu_breakpoints[];
extern struct emu_thread_data_t     emu_emulation_data;
extern struct emu_log_t *           emu_log_entries;
extern uint32_t                     emu_log_entry_count;

extern struct cpu_state_t           cpu_state;
extern const char *                 opcode_strs[];

extern uint8_t *            vram;
extern uint8_t *            cgram;
extern uint8_t *            ram1_regs;
extern struct background_t  backgrounds[4];
extern const char *         ppu_reg_strs[];
extern uint16_t             vcounter;
extern uint16_t             hcounter;
extern uint32_t             vram_addr;
extern uint32_t             cur_obj_sizes[];
extern union oam_t          oam;

const char *m_breakpoint_type_names[] = {
    [BREAKPOINT_TYPE_EXECUTION]     = "Execution",
    [BREAKPOINT_TYPE_REGISTER]      = "Register",
    [BREAKPOINT_TYPE_MEM_READ]      = "MEM read",
    [BREAKPOINT_TYPE_MEM_WRITE]     = "MEM write",
    [BREAKPOINT_TYPE_VRAM_READ]     = "VRAM read",
    [BREAKPOINT_TYPE_VRAM_WRITE]    = "VRAM write",
    [BREAKPOINT_TYPE_REG_READ]      = "REG read",
    [BREAKPOINT_TYPE_REG_WRITE]     = "REG write",
    [BREAKPOINT_TYPE_DMA]           = "DMA"
};

uint64_t        m_counter_frequency;
uint64_t        m_prev_count = 0;
uint32_t        m_frame = 0;
float           m_accum_time = 0;
float           m_frame_time = 0;

struct viewer_tile_t
{
    struct dot_t pixels[64];
};

enum M_RUN_MODE
{
    M_RUN_MODE_RUN,
    M_RUN_MODE_STOP,
    M_RUN_MODE_NEXT_INSTRUCTION,
    M_RUN_MODE_NEXT_CYCLE,
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

    // if(!load_cart("./rom/tests/cpu/ADC/CPUADC.sfc"))
    // if(!load_cart("./rom/super_metroid_ntsc.sfc"))
    // {
    //     printf("couldn't load rom\n");
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

    uint32_t run_mode = M_RUN_MODE_STOP;
    uint32_t tile_bits_per_pixel = 4;
    uint32_t tile_viewer_first_row = 0;
    uint32_t tile_viewer_last_row = 0;
    uint32_t hovered_sprite = 0xffffffff;

    uint32_t cur_breakpoint_tab = BREAKPOINT_TYPE_EXECUTION;
    uint32_t add_breakpoint_type;
    char breakpoint_start_address_buffer[128];
    char breakpoint_end_address_buffer[128];
    char register_value_buffer[128] = {};
    char rom_name_buffer[512] = {"./rom/super_metroid_ntsc.sfc"};
    uint32_t breakpoint_start_address;
    uint32_t breakpoint_end_address;

    m_counter_frequency = SDL_GetPerformanceFrequency();
    m_prev_count = SDL_GetPerformanceCounter();

    while(!in_ReadInput())
    {
        ui_Begin();

        if(igBeginMainMenuBar())
        {
            // if(igBeginMenu("Emulation", 1))
            // {
            //     if(igMenuItem_Bool("Run", NULL, 0, 1))
            //     {
            //         run_emulation = 1;
            //     }

            //     if(igMenuItem_Bool("Halt", NULL, 0, 1))
            //     {
            //         run_emulation = 0;
            //     }
            //     igEndMenu();
            // }

            igEndMainMenuBar();   
        }

        uint32_t status = 0;

        switch(run_mode)
        {
            case M_RUN_MODE_NEXT_INSTRUCTION:
            {
                do
                {
                    status = emu_Step(4);
                }
                while(!(status & (EMU_STATUS_BREAKPOINT | EMU_STATUS_END_OF_INSTRUCTION)));

                run_mode = M_RUN_MODE_STOP;
            }
            break;

            case M_RUN_MODE_NEXT_CYCLE:
                emu_Step(1);
                run_mode = M_RUN_MODE_STOP;
            break;

            case M_RUN_MODE_RUN:
                do
                {
                    status = emu_Step(4);
                }
                while(!(status & (EMU_STATUS_END_OF_FRAME | EMU_STATUS_BREAKPOINT)));

                if(status & EMU_STATUS_BREAKPOINT)
                {
                    run_mode = M_RUN_MODE_STOP;
                }
            break;
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

            igDockSpace(main_dockspace, dockspace_size, ImGuiDockNodeFlags_AutoHideTabBar, NULL);

            if(igBegin("##data_viewer", NULL, ImGuiWindowFlags_NoScrollbar))
            {
                ImVec2 window_size;
                igGetContentRegionAvail(&window_size);
                if(igBeginTabBar("##views", 0))
                {
                    if(igBeginTabItem("CPU", NULL, 0))
                    {
                        if(igBeginTable("##registers0", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_NoKeepColumnsVisible, (ImVec2){0, 0}, 0))
                        {
                            igTableNextRow(ImGuiTableRowFlags_Headers, 0);
                            igTableNextColumn();
                            igText("ACCUM");
                            igTableNextColumn();
                            igText("X");
                            igTableNextColumn();
                            igText("Y");
                            igTableNextColumn();
                            igText("S");
                            igTableNextColumn();
                            igText("D");
                            igTableNextColumn();
                            igText("PC");
                            igTableNextColumn();
                            igText("PBR");
                            igTableNextColumn();
                            igText("DBR");

                            igTableNextRow(0, 0);
                            igTableNextColumn();
                            igText("%04x", cpu_state.regs[REG_ACCUM].word);
                            igTableNextColumn();
                            igText("%04x", cpu_state.regs[REG_X].word);
                            igTableNextColumn();
                            igText("%04x", cpu_state.regs[REG_Y].word);
                            igTableNextColumn();
                            igText("%04x", cpu_state.regs[REG_S].word);
                            igTableNextColumn();
                            igText("%04x", cpu_state.regs[REG_D].word);
                            igTableNextColumn();
                            igText("%04x", cpu_state.instruction_address & 0xffff);
                            igText("(%04x)", cpu_state.regs[REG_PC].word);
                            igTableNextColumn();
                            igText("%02x", cpu_state.regs[REG_PBR].byte[0]);
                            igTableNextColumn();
                            igText("%02x", cpu_state.regs[REG_DBR].byte[0]);

                            igEndTable();
                        }

                        if(igBeginTable("##registers1", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_NoKeepColumnsVisible, (ImVec2){0, 0}, 0))
                        {
                            igTableNextRow(ImGuiTableRowFlags_Headers, 0);
                            igTableNextColumn();
                            igText("N");
                            igTableNextColumn();
                            igText("V");
                            igTableNextColumn();
                            igText(cpu_state.reg_p.e ? "--" : "M");
                            igTableNextColumn();
                            igText(cpu_state.reg_p.e ? "B" : "X");
                            igTableNextColumn();
                            igText("D");
                            igTableNextColumn();
                            igText("I");
                            igTableNextColumn();
                            igText("Z");
                            igTableNextColumn();
                            igText("C - E");
                            
                            igTableNextRow(0, 0);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.n);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.v);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.m);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.e ? cpu_state.reg_p.b : cpu_state.reg_p.x);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.d);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.i);
                            igTableNextColumn();
                            igText("%d", cpu_state.reg_p.z);
                            igTableNextColumn();
                            igText("%d - %d", cpu_state.reg_p.c, cpu_state.reg_p.e);
                            
                            igEndTable();
                        }

                        if(igBeginChild_Str("##disasm", (ImVec2){0, 0}, 1, 0))
                        {
                            struct disasm_state_t disasm_state;
                            init_disasm(&disasm_state, &cpu_state);

                            igPushStyleColor_Vec4(ImGuiCol_TableRowBgAlt, (ImVec4){0.2, 0.2, 0.2, 1.0});

                            if(igBeginTable("##disasm", 3, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_RowBg, (ImVec2){0, 0}, 0.0))
                            {
                                igTableNextRow(ImGuiTableRowFlags_Headers, 0);
                                igTableNextColumn();
                                igText("Addr");
                                igTableNextColumn();
                                igText("Bytes");
                                igTableNextColumn();
                                igText("Instruction");

                                for(uint32_t index = 0; index < 20; index++)
                                {   
                                    uint16_t pc = disasm_state.reg_pc;
                                    uint8_t pbr = disasm_state.reg_pbr;
                                    struct disasm_inst_t instruction;
                                    disasm(&disasm_state, &instruction);

                                    igTableNextRow(0, 0);
                                    igTableNextColumn();
                                    igText("[0x%02x:0x%04x]", pbr, pc);

                                    igTableNextColumn();
                                    for(uint32_t index = 0; index < instruction.width; index++)
                                    {
                                        igText("%02x", instruction.bytes[index]);
                                        igSameLine(0, -1);
                                    }
                                    
                                    igTableNextColumn();
                                    igText("%s %s", instruction.opcode_str, instruction.addr_mode_str);
                                }
                                igEndTable();

                                igPopStyleColor(1);
                            }
                        }
                        igEndChild();
                        
                        igEndTabItem();
                    }

                    if(igBeginTabItem("PPU", NULL, 0))
                    {
                        if(igBeginTable("##ppu", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_NoKeepColumnsVisible, (ImVec2){0, 0}, 0))
                        {
                            igTableNextRow(ImGuiTableRowFlags_Headers, 0);
                            igTableNextColumn();
                            igText("H counter");
                            igTableNextColumn();
                            igText("V counter");
                            igTableNextColumn();
                            igText("VRAM address");

                            igTableNextRow(0, 0);
                            igTableNextColumn();
                            igText("%d", hcounter);
                            igTableNextColumn();
                            igText("%d", vcounter);
                            igTableNextColumn();
                            igText("%04x", vram_addr);
                            igEndTable();
                        }

                        if(igBeginChild_Str("##vram", (ImVec2){0, 0}, 1, 0))
                        {
                            if(igBeginTabBar("##views", 0))
                            {
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
                                    tile_bits_per_pixel = 4;
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
                                                    struct col_t color = pal16_col(&mode0_cgram->bg2_colors, 2, color_index);
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
                                            igText("Screen data base address: 0x%04x", ((uintptr_t)background->data_base[0]) - (uintptr_t)vram);
                                        }
                                        igEndChild();
                                        igPopID();

                                        chr_size >>= 1;
                                    }
                                    igEndTabItem();
                                }

                                hovered_sprite = 0xffffffff;
                                if(igBeginTabItem("OAM", NULL, 0))
                                {
                                    for(uint32_t index = 0; index < MAX_OBJ_COUNT; index++)
                                    {
                                        uint32_t name = oam.tables.table1[index].fpcn & OBJ_ATTR1_NAME_MASK;
                                        uint32_t flip = oam.tables.table1[index].fpcn & (OBJ_ATTR1_HFLIP | OBJ_ATTR1_VFLIP);
                                        uint32_t priority = (oam.tables.table1[index].fpcn >> OBJ_ATTR1_PRI_SHIFT) & OBJ_ATTR1_PRI_MASK;
                                        uint32_t v_pos = oam.tables.table1[index].v_pos;
                                        uint32_t h_pos = oam.tables.table1[index].h_pos;
                                        uint32_t size_pos = oam.tables.table2[index >> 3].size_hpos >> ((index & 0x3) << 1);

                                        if(size_pos & PPU_OBJ_ATTR2_HPOS_MASK)
                                        {
                                            h_pos |= 0xff00;
                                        }

                                        igPushID_Ptr(&oam.tables.table1[index]);
                                        if(igBeginChild_Str("Sprite", (ImVec2){0, 120}, 1, 0))
                                        {
                                            if(igIsWindowHovered(0))
                                            {
                                                hovered_sprite = index;
                                            }
                                            igText("Index: %d", index);
                                            igText("X: %d, Y: %d", h_pos, v_pos);
                                            igText("Size: %d", cur_obj_sizes[(size_pos >> OBJ_ATTR2_SIZE_SHIFT) & OBJ_ATTR2_SIZE_MASK]);
                                            igText("Flip H: %d, Flip V: %d", (flip & OBJ_ATTR1_HFLIP) && 1, (flip & OBJ_ATTR1_VFLIP) && 1);
                                            igText("Priority: %d", priority);
                                            igText("Name: %d", name);
                                        }
                                        igEndChild();
                                        igPopID();
                                    }
                                    igEndTabItem();
                                }

                                if(igBeginTabItem("Regs", NULL, 0))
                                {
                                    for(uint32_t reg = PPU_REG_INIDISP; reg < PPU_REG_WMADDH; reg++)
                                    {
                                        if(ppu_reg_strs[reg - PPU_REG_INIDISP] != NULL)
                                        {
                                            igText("%04x(%s): %02x", reg, ppu_reg_strs[reg - PPU_REG_INIDISP], ram1_regs[reg]);
                                        }
                                    }
                                    igEndTabItem();
                                }

                                igEndTabBar();
                            }
                        }

                        igEndChild();
                        igEndTabItem();
                    }

                    if(igBeginTabItem("Breakpoints", NULL, 0))
                    {
                        if(igButton("Add breakpoint", (ImVec2){0, 0}))
                        {
                            igOpenPopup_Str("Add breakpoint", 0);
                            add_breakpoint_type = cur_breakpoint_tab;
                            breakpoint_start_address_buffer[0] = '\0';
                            breakpoint_end_address_buffer[0] = '\0';
                            breakpoint_start_address = 0;
                            breakpoint_end_address = 0;
                        }

                        if(igIsPopupOpen_Str("Add breakpoint", 0))
                        {
                            igSetNextWindowSize((ImVec2){256, 132}, 0);
                            if(igBeginPopupModal("Add breakpoint", 0, 0))
                            {
                                if(igBeginCombo("Type", m_breakpoint_type_names[add_breakpoint_type], 0))
                                {
                                    for(uint32_t breakpoint_type = 0; breakpoint_type < BREAKPOINT_TYPE_LAST; breakpoint_type++)
                                    {
                                        if(igSelectable_Bool(m_breakpoint_type_names[breakpoint_type], add_breakpoint_type == breakpoint_type, 0, (ImVec2){0, 0}))
                                        {
                                            add_breakpoint_type = breakpoint_type;
                                        }
                                    }

                                    igEndCombo();
                                }

                                // uint32_t address = 0;

                                igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0, 0});
                                if(igBeginChild_Str("##breakpoint_config", (ImVec2){0, 48}, 0, 0))
                                {
                                    switch(add_breakpoint_type)
                                    {
                                        case BREAKPOINT_TYPE_VRAM_READ:
                                        case BREAKPOINT_TYPE_VRAM_WRITE:
                                        case BREAKPOINT_TYPE_MEM_READ:
                                        case BREAKPOINT_TYPE_MEM_WRITE:
                                        case BREAKPOINT_TYPE_REG_READ:
                                        case BREAKPOINT_TYPE_REG_WRITE:
                                            if(igInputText("Start address", breakpoint_start_address_buffer, sizeof(breakpoint_start_address_buffer), 
                                                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL))
                                            {
                                                breakpoint_start_address = strtoul(breakpoint_start_address_buffer, NULL, 16);
                                            }

                                            if(igInputText("End address", breakpoint_end_address_buffer, sizeof(breakpoint_end_address_buffer), 
                                                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL))
                                            {
                                                breakpoint_end_address = strtoul(breakpoint_end_address_buffer, NULL, 16);
                                            }
                                        break;

                                        case BREAKPOINT_TYPE_EXECUTION:
                                            if(igInputText("Address", breakpoint_start_address_buffer, sizeof(breakpoint_start_address_buffer), 
                                                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL))
                                            {
                                                breakpoint_start_address = strtoul(breakpoint_start_address_buffer, NULL, 16);
                                            }
                                        break;

                                        case BREAKPOINT_TYPE_DMA:
                                            if(igInputText("Channel", breakpoint_start_address_buffer, sizeof(breakpoint_start_address_buffer), 
                                                ImGuiInputTextFlags_CharsHexadecimal, NULL, NULL))
                                            {
                                                breakpoint_start_address = strtoul(breakpoint_start_address_buffer, NULL, 16);
                                            }
                                        break;
                                    }
                                }
                                igEndChild();
                                igPopStyleVar(1);
                                
                                if(igButton("Cancel", (ImVec2){48, 0}))
                                {
                                    igCloseCurrentPopup();
                                }

                                igSameLine(0, -1);

                                if(igButton("Add", (ImVec2){32, 0}))
                                {
                                    switch(add_breakpoint_type)
                                    {
                                        case BREAKPOINT_TYPE_EXECUTION:
                                            set_execution_breakpoint(breakpoint_start_address);       
                                        break;

                                        case BREAKPOINT_TYPE_VRAM_READ:
                                        case BREAKPOINT_TYPE_VRAM_WRITE:
                                        case BREAKPOINT_TYPE_MEM_READ:
                                        case BREAKPOINT_TYPE_MEM_WRITE:
                                        case BREAKPOINT_TYPE_REG_READ:
                                        case BREAKPOINT_TYPE_REG_WRITE:
                                            set_read_write_breakpoint(add_breakpoint_type, breakpoint_start_address, breakpoint_end_address);
                                        break;

                                        case BREAKPOINT_TYPE_DMA:
                                            set_dma_breakpoint(breakpoint_start_address);
                                        break;
                                    }
                                    igCloseCurrentPopup();
                                }

                                igEndPopup();
                            }
                        }
                        
                        if(igBeginTabBar("##breakpoints", 0))
                        {
                            for(uint32_t breakpoint_type = 0; breakpoint_type < BREAKPOINT_TYPE_LAST; breakpoint_type++)
                            {
                                struct breakpoint_list_t *list = emu_breakpoints + breakpoint_type;
                                if(igBeginTabItem(m_breakpoint_type_names[breakpoint_type], NULL, 0))
                                {
                                    cur_breakpoint_tab = breakpoint_type;

                                    igText("Count: %d", list->count);

                                    for(uint32_t index = 0; index < list->count; index++)
                                    {
                                        struct breakpoint_t *breakpoint = list->breakpoints + index;
                                        igPushID_Int(index);
                                        if(igBeginChild_Str("##breakpoint", (ImVec2){0, 64}, 1, 0))
                                        {
                                            ImVec4 color;
                                            if(emu_emulation_data.breakpoint == breakpoint)
                                            {
                                                color = (ImVec4){1, 1, 0, 1};
                                            }
                                            else
                                            {
                                                color = (ImVec4){1, 1, 1, 1};
                                            }

                                            switch(breakpoint->type)
                                            {
                                                case BREAKPOINT_TYPE_VRAM_READ:
                                                case BREAKPOINT_TYPE_VRAM_WRITE:
                                                    igTextColored(color, "VRAM range: [%06x - %06x]", breakpoint->start_address, breakpoint->end_address);
                                                break;

                                                case BREAKPOINT_TYPE_MEM_READ:
                                                case BREAKPOINT_TYPE_MEM_WRITE:
                                                    igTextColored(color, "Memory range: [%06x - %06x]", breakpoint->start_address, breakpoint->end_address);
                                                break;

                                                case BREAKPOINT_TYPE_DMA:
                                                    igTextColored(color, "Channel: %d", breakpoint->dma.channel);
                                                break;
                                            }
                                        }
                                        igEndChild();
                                        igPopID();
                                    }

                                    igEndTabItem();
                                }
                            }
                            igEndTabBar();
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

                igSetNextItemWidth(260.0);
                igInputText("ROM", rom_name_buffer, sizeof(rom_name_buffer), ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL);
                igSameLine(0, -1);

                if(igButton("Load", (ImVec2){32, 0}))
                {
                    unload_cart();

                    if(!load_cart(rom_name_buffer))
                    {
                        printf("couldn't load rom\n");
                    }
                    else
                    {
                        reset_emu();
                    }
                }
                igSameLine(0, -1);

                if(igButton("Run", (ImVec2){32, 0}))
                {
                    run_mode = M_RUN_MODE_RUN;
                }
                igSameLine(0, -1);
                if(igButton("Next instruction", (ImVec2){96, 0}))
                {
                    run_mode = M_RUN_MODE_NEXT_INSTRUCTION;
                }
                igSameLine(0, -1);
                if(igButton("Next cycle", (ImVec2){80, 0}))
                {
                    run_mode = M_RUN_MODE_NEXT_CYCLE;
                }
                igSameLine(0, -1);
                if(igButton("Halt", (ImVec2){48, 0}))
                {
                    run_mode = M_RUN_MODE_STOP;
                }

                if(status & EMU_STATUS_END_OF_FRAME)
                {
                    m_frame++;
                    uint64_t cur_count = SDL_GetPerformanceCounter();
                    m_accum_time += (float)(cur_count - m_prev_count) / (float)m_counter_frequency;
                    m_prev_count = cur_count;

                    if(m_frame >= 60)
                    {
                        m_frame = 0;
                        m_accum_time /= 60.0;
                        m_frame_time = m_accum_time;
                        m_accum_time = 0.0;
                    }
                }

                igSameLine(0, -1);
                igText("%.04f ms (%.02f fps)", m_frame_time * 1000.0, 1.0f / m_frame_time);
                if(igBeginChild_Str("blah", (ImVec2){0, 0}, 0, 0))
                {
                    igGetContentRegionAvail(&window_size);
                    ImVec2 cursor_pos;
                    ImVec2 image_size;
                    igGetCursorPos(&cursor_pos);
                    igPushStyleVar_Vec2(ImGuiStyleVar_FramePadding, (ImVec2){});
                    igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){});
                    igImage((ImTextureID)(uintptr_t)emu_framebuffer_texture, window_size, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){0, 0, 0, 0});
                    igGetItemRectSize(&image_size);

                    if(hovered_sprite != 0xffffffff)
                    {
                        uint16_t v_pos = oam.tables.table1[hovered_sprite].v_pos;
                        uint16_t h_pos = oam.tables.table1[hovered_sprite].h_pos;
                        uint16_t size_pos = oam.tables.table2[hovered_sprite >> 3].size_hpos >> ((hovered_sprite & 0x3) << 1);

                        if(size_pos & PPU_OBJ_ATTR2_HPOS_MASK)
                        {
                            h_pos |= 0xff00;
                        }

                        float highlight_x = (float)h_pos / 256.0f;
                        float highlight_y = (float)v_pos / 225.0f;
                        // ImVec2 cursor_pos = (ImVec2){window_size.x * highlight_x, window_size.y * highlight_y};

                        cursor_pos.x += highlight_x * image_size.x;
                        cursor_pos.y += highlight_y * image_size.y;
                        igSetCursorPos(cursor_pos);
                        igImage((ImTextureID)(uintptr_t)m_tile_viewer_texture, (ImVec2){16, 16}, (ImVec2){}, (ImVec2){}, (ImVec4){0, 0, 0, 0}, (ImVec4){1, 1, 1, 1});
                        // igPushStyleColor_Vec4(ImGuiCol_Border)
                        // igSelectable_Bool("Test", 0, 0, (ImVec2){8, 8});
                    }

                    igPopStyleVar(2);
                }
                igEndChild();
            }
            igEnd();

            if(igBegin("##trace", NULL, 0))
            {
                if(igBeginChild_Str("##trace_window",(ImVec2){0, 0}, 1, 0))
                {
                    for(uint32_t entry_index = emu_log_entry_count - 1; entry_index < 0xffffffff; entry_index--)
                    {
                        struct emu_log_t *log = emu_log_entries + entry_index;
                        igText("%s", log->message);
                    }
                }
                igEndChild();
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
