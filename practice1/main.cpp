#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

GLuint create_shader(GLenum shader_type, const char *shader_source) {
    GLuint shader;
    shader = glCreateShader(shader_type);
    const GLchar *shader_src = shader_source;
    glShaderSource(shader, 1, &shader_src, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        GLchar err_log[log_length];
        glGetShaderInfoLog(shader, log_length, nullptr, err_log);
        throw std::runtime_error(err_log);
    }
    return shader;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    auto program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        GLchar err_log[log_length];
        glGetShaderInfoLog(program, log_length, nullptr, err_log);
        throw std::runtime_error(err_log);
    }
    return program;
}

int main() try {

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 1",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);


    auto src_fragment = R"(
            #version 330 core
            layout (location = 0) out vec4 out_color;
            flat in vec3 color;
            void main()
            {
                out_color = vec4(color, 1.0);
            }
    )";
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, src_fragment);

    auto src_vertex = R"(
            #version 330 core
            const vec2 VERTICES[3] = vec2[3](
                vec2(0.5, -0.5),
                vec2(-0.5, -0.5),
                vec2(0.0, 0.5)
            );
            flat out vec3 color;
            void main()
            {
                gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);
                color = vec3(VERTICES[gl_VertexID], 1.0);
            }
    )";
    auto vertex_shader = create_shader(GL_VERTEX_SHADER, src_vertex);

    auto program = create_program(vertex_shader, fragment_shader);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);


    glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);

    bool running = true;

    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }

        if (!running)
            break;
        glUseProgram(program);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);


        SDL_GL_SwapWindow(window);
    }


    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
