#include "ui.h"
#include "GL/glew.h"
#include "SDL2/SDL.h"
#include "ppu.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>

#define UI_VERTEX_BUFFER_SIZE   1000000
#define UI_INDEX_BUFFER_SIZE    1000000



extern uint32_t     emu_window_width;
extern uint32_t     emu_window_height;
extern GLuint       emu_framebuffer_texture;
extern SDL_Window * emu_window;

float           ui_model_view_projection_matrix[16] = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
};

ImGuiContext *  ui_context;
GLuint          ui_vao;
GLuint          ui_vertex_buffer;
GLuint          ui_index_buffer;
GLuint          ui_atlas_texture;
GLuint          ui_shader;
GLuint          ui_shader_model_view_projection_matrix;
GLuint          ui_shader_texture;

uint32_t        ui_mouse_button[] = {
    [SDL_BUTTON_LEFT]   = ImGuiMouseButton_Left,
    [SDL_BUTTON_MIDDLE] = ImGuiMouseButton_Middle,
    [SDL_BUTTON_RIGHT]  = ImGuiMouseButton_Right,
};

uint32_t ui_keyboard_map[SDL_NUM_SCANCODES] = {
    [SDL_SCANCODE_1]            = ImGuiKey_1,
    [SDL_SCANCODE_2]            = ImGuiKey_2,
    [SDL_SCANCODE_3]            = ImGuiKey_3,
    [SDL_SCANCODE_4]            = ImGuiKey_4,
    [SDL_SCANCODE_5]            = ImGuiKey_5,
    [SDL_SCANCODE_6]            = ImGuiKey_6,
    [SDL_SCANCODE_7]            = ImGuiKey_7,
    [SDL_SCANCODE_8]            = ImGuiKey_8,
    [SDL_SCANCODE_9]            = ImGuiKey_9,
    [SDL_SCANCODE_0]            = ImGuiKey_0,

    [SDL_SCANCODE_Q]            = ImGuiKey_Q,
    [SDL_SCANCODE_W]            = ImGuiKey_W,
    [SDL_SCANCODE_E]            = ImGuiKey_E,
    [SDL_SCANCODE_R]            = ImGuiKey_R,
    [SDL_SCANCODE_T]            = ImGuiKey_T,
    [SDL_SCANCODE_U]            = ImGuiKey_U,
    [SDL_SCANCODE_I]            = ImGuiKey_I,
    [SDL_SCANCODE_O]            = ImGuiKey_O,
    [SDL_SCANCODE_P]            = ImGuiKey_P,
    [SDL_SCANCODE_A]            = ImGuiKey_A,
    [SDL_SCANCODE_S]            = ImGuiKey_S,
    [SDL_SCANCODE_D]            = ImGuiKey_D,
    [SDL_SCANCODE_F]            = ImGuiKey_F,
    [SDL_SCANCODE_G]            = ImGuiKey_G,
    [SDL_SCANCODE_H]            = ImGuiKey_H,
    [SDL_SCANCODE_J]            = ImGuiKey_J,
    [SDL_SCANCODE_K]            = ImGuiKey_K,
    [SDL_SCANCODE_L]            = ImGuiKey_L,
    [SDL_SCANCODE_Z]            = ImGuiKey_Z,
    [SDL_SCANCODE_X]            = ImGuiKey_X,
    [SDL_SCANCODE_C]            = ImGuiKey_C,
    [SDL_SCANCODE_V]            = ImGuiKey_V,
    [SDL_SCANCODE_B]            = ImGuiKey_B,
    [SDL_SCANCODE_N]            = ImGuiKey_N,
    [SDL_SCANCODE_M]            = ImGuiKey_M,
    [SDL_SCANCODE_BACKSPACE]    = ImGuiKey_Backspace,
    [SDL_SCANCODE_RETURN]       = ImGuiKey_Enter
};

const char *    ui_vertex_shader =
    "#version 400 core\n"
    "layout (location = 0) in vec4 ui_position;\n"
    "layout (location = 1) in vec2 ui_tex_coords;\n"
    "layout (location = 2) in vec4 ui_color;\n"
    "out vec4 color;\n"
    "out vec2 tex_coords;\n"
    "uniform mat4 ui_model_view_projection_matrix;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = ui_model_view_projection_matrix * ui_position;\n"
    "   color = ui_color;\n"
    "   tex_coords = ui_tex_coords;\n"
    "}\n";

const char *    ui_fragment_shader = 
    "#version 400 core\n"
    "in vec4 color;\n"
    "in vec2 tex_coords;\n"
    "uniform sampler2D ui_texture;\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = texture(ui_texture, tex_coords) * color;\n"
    "}\n";

void ui_Init()
{
    glGenVertexArrays(1, &ui_vao);
    glBindVertexArray(ui_vao);

    glGenBuffers(1, &ui_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, UI_VERTEX_BUFFER_SIZE * sizeof(struct ImDrawVert), NULL, GL_STREAM_DRAW);

    glGenBuffers(1, &ui_index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ui_index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, UI_INDEX_BUFFER_SIZE * sizeof(uint32_t), NULL, GL_STREAM_DRAW);

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &ui_vertex_shader, NULL);
    glCompileShader(vertex_shader);

    GLint status;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        GLint log_length;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
        char *info_log = calloc(log_length, 1);
        glGetShaderInfoLog(vertex_shader, log_length, NULL, info_log);
        printf("vertex shader error:\n%s\n", info_log);
        free(info_log);
        exit(-1);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &ui_fragment_shader, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        GLint log_length;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
        char *info_log = calloc(log_length, 1);
        glGetShaderInfoLog(fragment_shader, log_length, NULL, info_log);
        printf("fragment shader error:\n%s\n", info_log);
        free(info_log);
        exit(-1);
    }

    ui_shader = glCreateProgram();
    glAttachShader(ui_shader, vertex_shader);
    glAttachShader(ui_shader, fragment_shader);
    glLinkProgram(ui_shader);
    glGetProgramiv(ui_shader, GL_LINK_STATUS, &status);
    if(!status)
    {
        GLint log_length;
        glGetProgramiv(ui_shader, GL_INFO_LOG_LENGTH, &log_length);
        char *info_log = calloc(log_length, 1);
        glGetProgramInfoLog(ui_shader, log_length, NULL, info_log);
        printf("shader link error:\n%s\n", info_log);
        free(info_log);
        exit(-1);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    ui_shader_model_view_projection_matrix = glGetUniformLocation(ui_shader, "ui_model_view_projection_matrix");
    ui_shader_texture = glGetUniformLocation(ui_shader, "ui_texture");

    glUseProgram(ui_shader);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct ImDrawVert), (void *)(offsetof(struct ImDrawVert, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct ImDrawVert), (void *)(offsetof(struct ImDrawVert, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct ImDrawVert), (void *)(offsetof(struct ImDrawVert, col)));
    

    ui_context = igCreateContext(NULL);
    ImGuiIO *io = igGetIO();
    io->DisplaySize.x = emu_window_width;
    io->DisplaySize.y = emu_window_height;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io->IniFilename = NULL;

    unsigned char *atlas_pixels = NULL;
    int atlas_width;
    int atlas_height;
    int pixel_size;
    ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &atlas_pixels, &atlas_width, &atlas_height, &pixel_size);

    glGenTextures(1, &ui_atlas_texture);
    glBindTexture(GL_TEXTURE_2D, ui_atlas_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlas_width, atlas_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas_pixels);
    ImFontAtlas_SetTexID(io->Fonts, (ImTextureID)(uintptr_t)ui_atlas_texture);

    glActiveTexture(0);
}

void ui_Shutdown()
{
    glDeleteProgram(ui_shader);
    glDeleteTextures(1, &ui_atlas_texture);
}

void ui_MouseMoveEvent(float mouse_x, float mouse_y)
{
    ImGuiIO *io = igGetIO();
    ImGuiIO_AddMousePosEvent(io, mouse_x, mouse_y);
}

void ui_MouseClickEvent(uint32_t button, uint32_t down)
{
    ImGuiIO *io = igGetIO();
    ImGuiIO_AddMouseButtonEvent(io, ui_mouse_button[button], down);
}

void ui_MouseWheelEvent(int32_t direction)
{
    ImGuiIO *io = igGetIO();
    ImGuiIO_AddMouseWheelEvent(io, 0, direction);
}

void ui_KeyboardEvent(uint32_t key, uint32_t key_down)
{
    ImGuiIO *io = igGetIO();
    if(ui_keyboard_map[key] > 0)
    {
        ImGuiIO_AddKeyEvent(io, ui_keyboard_map[key], (bool)key_down);
    }
}

void ui_TextInputEvent(uint32_t c)
{
    ImGuiIO *io = igGetIO();
    ImGuiIO_AddInputCharacter(io, c);
}

void ui_Begin()
{
    ImGuiIO *io = igGetIO();
    io->DisplaySize.x = emu_window_width;
    io->DisplaySize.y = emu_window_height;

    ui_model_view_projection_matrix[0] = 2.0f / (float)emu_window_width;
    ui_model_view_projection_matrix[5] = -2.0f / (float)emu_window_height;
    ui_model_view_projection_matrix[12] = -1.0f;
    ui_model_view_projection_matrix[13] = 1.0f;

    // ui_model_view_projection_matrix[10] = 1.0f;
    // ui_model_view_projection_matrix[15] = 1.0f;
    igNewFrame();
    
    in_SetTextInput(io->WantTextInput);
}

void ui_End()
{
    // igShowDemoWindow(NULL);
    igRender();
    ImDrawData *draw_data = igGetDrawData();
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(ui_shader);
    glUniformMatrix4fv(ui_shader_model_view_projection_matrix, 1, GL_FALSE, ui_model_view_projection_matrix);
    // uint32_t index_offset = 0;
    // uint32_t vertex_offset = 0;
    // for(uint32_t list_index = 0; list_index < draw_data->CmdListsCount; list_index++)
    // {
    //     ImDrawList *draw_list = draw_data->CmdLists[list_index];
    //     glBufferSubData(GL_ARRAY_BUFFER, vertex_offset, sizeof(struct ImDrawVert) * draw_list->VtxBuffer.Size, draw_list->VtxBuffer.Data);
    //     glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, index_offset, sizeof(uint32_t) * draw_list->IdxBuffer.Size, draw_list->IdxBuffer.Data);

    //     index_offset += draw_list->IdxBuffer.Size * sizeof(uint32_t);
    //     vertex_offset += draw_list->VtxBuffer.Size * sizeof(struct ImDrawVert);
    // }

    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    glActiveTexture(0);
    
    for(uint32_t list_index = 0; list_index < draw_data->CmdListsCount; list_index++)
    {
        ImDrawList *draw_list = draw_data->CmdLists[list_index];

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(struct ImDrawVert) * draw_list->VtxBuffer.Size, draw_list->VtxBuffer.Data);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(ImDrawIdx) * draw_list->IdxBuffer.Size, draw_list->IdxBuffer.Data);
        ImTextureID cur_texture_id = NULL;
        for(uint32_t cmd_index = 0; cmd_index < draw_list->CmdBuffer.Size; cmd_index++)
        {
            ImDrawCmd *draw_cmd = draw_list->CmdBuffer.Data + cmd_index;
            if(cur_texture_id != draw_cmd->TextureId)
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)draw_cmd->TextureId);
                cur_texture_id = draw_cmd->TextureId;
                glUniform1i(ui_shader_texture, 0);
            }
            glScissor(draw_cmd->ClipRect.x, (float)emu_window_height - draw_cmd->ClipRect.w, draw_cmd->ClipRect.z - draw_cmd->ClipRect.x, draw_cmd->ClipRect.w - draw_cmd->ClipRect.y);
            glDrawElements(GL_TRIANGLES, draw_cmd->ElemCount, GL_UNSIGNED_INT, (void *)(sizeof(uint32_t) * draw_cmd->IdxOffset));
        }
    }

    SDL_GL_SwapWindow(emu_window);
}