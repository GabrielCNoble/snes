#include "ui.h"
#include "GL/glew.h"

#define UI_VERTEX_BUFFER_SIZE   100000
#define UI_INDEX_BUFFER_SIZE    100000

ImGuiContext *  ui_context;
GLuint          ui_vao;
GLuint          ui_vertex_buffer;
GLuint          ui_index_buffer;
GLuint          ui_atlas_texture;
GLuint          ui_shader;

void ui_Init()
{
    glGenVertexArrays(1, &ui_vao);
    glBindVertexArray(ui_vao);

    glGenBuffers(1, &ui_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, UI_VERTEX_BUFFER_SIZE * sizeof(struct ImDrawVert), NULL, GL_STREAM_DRAW);

    glGenBuffers(1, &ui_index_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, UI_INDEX_BUFFER_SIZE * sizeof(ImDrawIdx), NULL, GL_STREAM_DRAW);

    

    ui_context = igCreateContext(NULL);
}

void ui_Shutdown()
{

}