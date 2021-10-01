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
#include <chrono>
#include <vector>
#include <map>

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
        R"(#version 330 core
uniform mat4 view;
uniform mat4 transform;
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_color;
out vec4 color;
void main()
{
	gl_Position = view * transform * vec4(in_position, 1.0);
	color = in_color;
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core
in vec4 color;
layout (location = 0) out vec4 out_color;
void main()
{
	out_color = color;
}
)";

GLuint create_shader(GLenum type, const char *source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct vec3 {
    float x;
    float y;
    float z;
};

struct vertex {
    vec3 position;
    std::uint8_t color[4];
};


float f_trig1(float x, float y, float t = 0.f) {
    return (std::sin(5 * x + 2 * y) + std::cos(6 * y - 10 * t)) / 3.f;
}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 4",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint transform_location = glGetUniformLocation(program, "transform");

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    std::uint32_t x_size = 100;
    std::uint32_t y_size = 100;

    std::vector<std::uint32_t> indices;
    std::vector<vertex> vertices;

    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(3);
    indices.push_back(4);
    indices.push_back(5);

    for (std::uint32_t i = 0; i < y_size - 1; ++i) {
        std::uint32_t offset = 6 + x_size * i;
        for (std::uint32_t j = 0; j < x_size - 1; ++j) {
            indices.push_back(offset + j);
            indices.push_back(offset + j + 1);
            indices.push_back(offset + x_size + j);

            indices.push_back(offset + j + 1);
            indices.push_back(offset + x_size + j);
            indices.push_back(offset + x_size + j + 1);
        }
    }

    auto x_range = std::make_pair(-1.f, 1.f);
    float x_period = (float) (x_range.second - x_range.first) / (x_size - 1);
    std::vector<float> x_values;

    for (int i = 0; i < x_size; ++i) {
        x_values.push_back(x_range.first + x_period * i);
    }

    auto y_range = std::make_pair(-1.f, 1.f);
    float y_period = (float) (y_range.second - y_range.first) / (y_size - 1);
    std::vector<float> y_values;

    for (int i = 0; i < y_size; ++i) {
        y_values.push_back(y_range.first + y_period * i);
    }


    GLuint vao, vbo, ebo;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);


    float near = 0.01;
    float far = 100.0;
    float right = near * tan(M_PI / 4.f);
    float top = height / (float) width * right;

    float cube_x = 0.f;
    float cube_y = 0.f;

    float d_angle_x = 0.f;
    float d_angle_y = 0.f;

    float speed = 4.f;


    bool running = true;
    while (running) {

        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
                            glViewport(0, 0, width, height);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    break;
                case SDL_KEYUP:
                    button_down[event.key.keysym.sym] = false;
                    break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        top = height / (float) width * right;

        float view[16] =
                {
                        near / right, 0.f, 0.f, 0.f,
                        0.f, near / top, 0.f, 0.f,
                        0.f, 0.f, -(far + near) / (far - near), -(2.f * far * near) / (far - near),
                        0.f, 0.f, -1.f, 0.f,
                };


        float scale = 1.8f;


        float d = speed * dt;

        if (button_down[SDLK_LEFT]) {
            d_angle_y -= d;
        }

        if (button_down[SDLK_RIGHT]) {
            d_angle_y += d;
        }

        if (button_down[SDLK_DOWN]) {
            d_angle_x -= d;
        }

        if (button_down[SDLK_UP]) {
            d_angle_x += d;
        }

        /*
        if (button_down[SDLK_MINUS]) {
            x_size -= d;
        }
         */


        float cos_x = cos(d_angle_x);
        float sin_x = sin(d_angle_x);

        float cos_y = cos(d_angle_y);
        float sin_y = sin(d_angle_y);

        float transform[16] =
                {
                        scale * cos_y, 0.f, sin_y * scale, 0.f,
                        0.f, scale * cos_x, sin_x * scale, 0.f,
                        -sin_y * scale, -sin_x * scale, scale * cos_x * cos_y, -5.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform);

        vertices.clear();

        vertices.push_back({{-1.f, 0.f, 0.f},
                            {0,    0,   0, 100}});
        vertices.push_back({{1.f, 0.f, 0.f},
                            {0,   0,   0, 100}});
        vertices.push_back({{0.f, 1.f, 0.f},
                            {0,   0,   0, 100}});
        vertices.push_back({{0.f, -1.f, 0.f},
                            {0,   0,    0, 100}});
        vertices.push_back({{0.f, 0.f, 1.f},
                            {0,   0,   0, 100}});
        vertices.push_back({{0.f, 0.f, -1.f},
                            {0,   0,   0, 100}});


        for (auto x: x_values) {
            for (auto y: y_values) {
                float z = f_trig1(x, y);
                auto r = (std::uint8_t) (255 * (z + 1.f) / 2.f);
                auto g = (std::uint8_t) (255 * (1.f - z) / 2.f);
                vertex v = {
                        {x,f_trig1(x, y, 0.f), y},
                        {r, g, 0, 255}
                };
                vertices.push_back(v);
            }
        }

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(vertices[0]) * vertices.size(),
                vertices.data(),
                GL_STATIC_DRAW
        );

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                sizeof(indices[0]) * indices.size(),
                indices.data(),
                GL_STATIC_DRAW
        );


        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) (offsetof(vertex, position)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *) (offsetof(vertex, color)));


        glDrawElements(GL_TRIANGLES, indices.size() - 6, GL_UNSIGNED_INT, (void *) (6 * sizeof(indices[0])));

        glDisable(GL_DEPTH_TEST);

        glPointSize(5.f);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, (void *) (1 * sizeof(indices[0])));
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, (void *) (3 * sizeof(indices[0])));
        glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, (void *) (0));
        glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, (void *) (2 * sizeof(indices[0])));
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, (void *) (5 * sizeof(indices[0])));
        glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, (void *) (4 * sizeof(indices[0])));

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
