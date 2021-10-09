#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>
#include "glm/glm.hpp"
#include "glm/ext.hpp"

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
// view + transform
uniform mat4 view;
uniform mat4 transform_scale;
uniform mat4 transform_move;
uniform mat4 transform_OY;
uniform mat4 transform_OX;

layout (location = 0) in vec2 in_position_xz;
layout (location = 1) in float in_position_y;
layout (location = 2) in vec3 in_color_rba;
layout (location = 3) in float in_color_g;

out vec4 color;

void main()
{
    mat4 transform =  transform_move * transform_scale * transform_OY * transform_OX;
    vec4 position = vec4(in_position_xz[0], in_position_y, in_position_xz[1], 1.0);
	gl_Position = view * transform * position;
	color = vec4(in_color_rba[0], in_color_g, in_color_rba[1], in_color_rba[2]);
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

struct vec2 {
    float x;
    float z;
};

struct vec3 {
    float x;
    float y;
    float z;
};

struct rba {
    std::uint8_t r;
    std::uint8_t b;
    std::uint8_t a;
};

struct vertex {
    vec2 position_xz;
    float position_y;
    std::uint8_t color_rba[3];
    std::uint8_t color_g;
};

struct plot_data {
    // detailing level = grid side size
    std::uint32_t grid_size = 50;

    std::uint32_t isoline_number = 5;
    std::vector<float> isoline_values;

    // triangle indices
    std::vector<std::uint32_t> indices;

    // vertices 3d positions
    std::vector<vec2> positions_xz;
    std::vector<float> positions_y;

    // grid values
    std::vector<float> values;

    //color components of vertices
    std::vector<rba> colors_rba;
    std::vector<std::uint8_t> colors_g;

    std::vector<vertex> isolines_vertices;

    void update_grid() {
        indices.clear();
        for (std::uint32_t i = 0; i < grid_size - 1; ++i) {
            std::uint32_t offset = 0 + grid_size * i;
            for (std::uint32_t j = 0; j < grid_size - 1; ++j) {
                indices.push_back(offset + j);
                indices.push_back(offset + j + 1);
                indices.push_back(offset + grid_size + j);

                indices.push_back(offset + j + 1);
                indices.push_back(offset + grid_size + j);
                indices.push_back(offset + grid_size + j + 1);
            }
        }

        values.clear();
        float period = (float) 2.f / (float) (grid_size - 1);
        for (int i = 0; i < grid_size; ++i) {
            values.push_back(-1.f + period * (float) i);
        }

        colors_rba.clear();
        positions_xz.clear();
        for (auto x: values) {
            for (auto z: values) {
                colors_rba.push_back({255, 0, 255});
                positions_xz.push_back({x, z});
            }
        }

    }

    float f_trig1(float x, float y, float t = 0.f) {
        return (std::sin(x * 5 + 2 * y) + std::cos(6 * y - 10 * t)) / 2.f;
    }

    void update_vertices(float time) {
        positions_y.clear();
        colors_g.clear();
        for (auto x: values) {
            for (auto z: values) {
                float y = f_trig1(x, z, time);
                positions_y.push_back(y);
                auto g = (std::uint8_t) (255 * (1.f - y) / 2.f);
                colors_g.push_back(g);
            }
        }
    }

    void update_isoline_number(){
        isoline_values.clear();
        float period = (float) 2.f / (float) (isoline_number - 1);
        for (int i = 1; i < isoline_number - 1; ++i) {
            isoline_values.push_back(-1.f + period * (float) i);
        }
    }

    void update_all_isolines(){
        isolines_vertices.clear();
        for (float val: isoline_values) {
            update_isoline(val);
        }
    }

    void update_isoline(float isoline_value) {

        for (int i = 0; i < indices.size(); i += 3) {

            auto v0 = std::make_pair(
                    positions_y[indices[i]],
                    std::make_pair(
                            positions_xz[indices[i]].x,
                            positions_xz[indices[i]].z
                    ));
            auto v1 = std::make_pair(positions_y[indices[i+1]], std::make_pair(
                    positions_xz[indices[i+1]].x,
                    positions_xz[indices[i+1]].z
            ));
            auto v2 = std::make_pair(positions_y[indices[i+2]], std::make_pair(
                    positions_xz[indices[i+2]].x,
                    positions_xz[indices[i+2]].z
            ));
            std::array v = {v0, v1, v2};
            std::sort(v.begin(), v.end());

            float x0 = v[0].second.first;
            float y0 = v[0].first;
            float z0 = v[0].second.second;
            float x1 = v[1].second.first;
            float y1 = v[1].first;
            float z1 = v[1].second.second;
            float x2 = v[2].second.first;
            float y2 = v[2].first;
            float z2 = v[2].second.second;

            if (y0 > isoline_value || y2 < isoline_value) {
                continue;
            }

            float a_02 = (isoline_value - y0) / (y2 - y0);
            float x_02 = std::lerp(x0, x2, a_02);
            float z_02 = std::lerp(z0, z2, a_02);
            isolines_vertices.push_back({{x_02, z_02}, isoline_value, {0, 200, 255}, 0});

            float x_other;
            float z_other;
            if (v[1].first > isoline_value) {
                float a = (isoline_value - y0) / (y1 - y0);
                x_other = std::lerp(x0, x1, a);
                z_other = std::lerp(z0, z1, a);
            } else {
                float a = (isoline_value - y1) / (y2 - y1);
                x_other = std::lerp(x1, x2, a);
                z_other = std::lerp(z1, z2, a);
            }
            isolines_vertices.push_back({{x_other, z_other}, isoline_value, {0, 0, 255}, 0});

        }

    }
};


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
    GLuint transform_scale_location = glGetUniformLocation(program, "transform_scale");
    GLuint transform_move_location = glGetUniformLocation(program, "transform_move");
    GLuint transform_OX_location = glGetUniformLocation(program, "transform_OX");
    GLuint transform_OY_location = glGetUniformLocation(program, "transform_OY");

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    plot_data data;
    data.update_grid();
    data.update_vertices(0.f);
    data.update_isoline_number();

    GLuint vao_main;
    glGenVertexArrays(1, &vao_main);
    glBindVertexArray(vao_main);

    GLuint ebo_vertices;
    glGenBuffers(1, &ebo_vertices);
    //static if detailing levels hasn't changed
    GLuint vbo_xz;
    glGenBuffers(1, &vbo_xz);
    //always dynamic
    GLuint vbo_y;
    glGenBuffers(1, &vbo_y);
    //static if detailing levels hasn't changed
    GLuint vbo_rba;
    glGenBuffers(1, &vbo_rba);
    //always dynamic
    GLuint vbo_g;
    glGenBuffers(1, &vbo_g);


    GLuint vao_axes;
    glGenVertexArrays(1, &vao_axes);
    glBindVertexArray(vao_axes);
    GLuint vbo_axes, ebo_axes;
    glGenBuffers(1, &vbo_axes);
    glGenBuffers(1, &ebo_axes);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_axes);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_axes);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) (offsetof(vertex, position_xz)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) (offsetof(vertex, position_y)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *) (offsetof(vertex, color_rba)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *) (offsetof(vertex, color_g)));

    vertex vertices_axes[6] = {
            //OX
            {{-1.f, -1.f}, -1.f,
                    {0, 0, 255}, 0},
            {{1.f,  -1.f}, -1.f,
                    {0, 0, 255}, 0},
            //OY
            {{-1.f, -1.f}, -1.f,
                    {0, 0, 255}, 0},
            {{-1.f, -1.f}, 1.f,
                    {0, 0, 255}, 0},
            //OZ
            {{-1.f, -1.f}, -1.f,
                    {0, 0, 255}, 0},
            {{-1.f, 1.f},  -1.f,
                    {0, 0, 255}, 0},
    };

    std::uint32_t indices_axes[6] = {0, 1, 2, 3, 4, 5};

    glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(vertices_axes),
            vertices_axes,
            GL_STATIC_DRAW
    );

    glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            sizeof(indices_axes),
            indices_axes,
            GL_STATIC_DRAW
    );

    glBindVertexArray(vao_main);

    // GRID

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_vertices);

    glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            sizeof(data.indices[0]) * data.indices.size(),
            data.indices.data(),
            GL_STATIC_DRAW
    );

    glBindBuffer(GL_ARRAY_BUFFER, vbo_xz);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void *) (0));

    glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(data.positions_xz[0]) * data.positions_xz.size(),
            data.positions_xz.data(),
            GL_STATIC_DRAW
    );

    glBindBuffer(GL_ARRAY_BUFFER, vbo_y);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void *) (0));

    glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(data.positions_y[0]) * data.positions_y.size(),
            data.positions_y.data(),
            GL_STATIC_DRAW
    );

    glBindBuffer(GL_ARRAY_BUFFER, vbo_rba);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(rba), (void *) (0));

    glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(data.colors_rba[0]) * data.colors_rba.size(),
            data.colors_rba.data(),
            GL_STATIC_DRAW
    );

    glBindBuffer(GL_ARRAY_BUFFER, vbo_g);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(std::uint8_t), (void *) (0));

    glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(data.colors_g[0]) * data.colors_g.size(),
            data.colors_g.data(),
            GL_STATIC_DRAW
    );

    GLuint vao_il;
    glGenVertexArrays(1, &vao_il);
    glBindVertexArray(vao_il);

    //always static
    GLuint vbo_il, ebo_il;
    glGenBuffers(1, &vbo_il);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_il);
    //glGenBuffers(1, &ebo_il);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_il);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) (offsetof(vertex, position_xz)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) (offsetof(vertex, position_y)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *) (offsetof(vertex, color_rba)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *) (offsetof(vertex, color_g)));


    float near = 0.05;
    float far = 200.0;
    float right = near * tan(M_PI / 4.f);
    float top = height / (float) width * right;

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
        glBindVertexArray(vao_main);
        top = ((float) height / (float) width) * right;

        float view[16] =
                {
                        near / right, 0.f, 0.f, 0.f,
                        0.f, near / top, 0.f, 0.f,
                        0.f, 0.f, -(far + near) / (far - near), -(2.f * far * near) / (far - near),
                        0.f, 0.f, -1.f, 0.f,
                };


        float scale = 2.1f;


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


        if (button_down[SDLK_MINUS] || button_down[SDLK_EQUALS]) {
            if (button_down[SDLK_MINUS]) {
                data.grid_size -= 1;
            }

            if (button_down[SDLK_EQUALS]) {
                data.grid_size += 1;
            }

            data.update_grid();
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_vertices);
            glBufferData(
                    GL_ELEMENT_ARRAY_BUFFER,
                    sizeof(data.indices[0]) * data.indices.size(),
                    data.indices.data(),
                    GL_STATIC_DRAW
            );

            glBindBuffer(GL_ARRAY_BUFFER, vbo_xz);
            glBufferData(
                    GL_ARRAY_BUFFER,
                    sizeof(data.positions_xz[0]) * data.positions_xz.size(),
                    data.positions_xz.data(),
                    GL_STATIC_DRAW
            );

            glBindBuffer(GL_ARRAY_BUFFER, vbo_rba);
            glBufferData(
                    GL_ARRAY_BUFFER,
                    sizeof(data.colors_rba[0]) * data.colors_rba.size(),
                    data.colors_rba.data(),
                    GL_STATIC_DRAW
            );
        }

        if (button_down[SDLK_1]) {
            if (data.isoline_number > 2) {
                data.isoline_number -= 1;
                data.update_isoline_number();
            }
        }

        if (button_down[SDLK_2]) {
            data.isoline_number += 1;
            data.update_isoline_number();
        }


        float cos_x = cos(d_angle_x);
        float sin_x = sin(d_angle_x);

        float cos_y = cos(d_angle_y);
        float sin_y = sin(d_angle_y);

        float transform_scale[16] =
                {
                        scale, 0.f, 0.f, 0.f,
                        0.f, scale, 0.f, 0.f,
                        0.f, -0.f, scale, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        float transform_move[16] =
                {
                        1.f, 0.f, 0.f, 0.f,
                        0.f, 1.f, 0.f, 0.f,
                        0.f, 0.f, 1.f, -8.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        float transform_OX[16] =
                {
                        1.f, 0.f, 0.f, 0.f,
                        0.f, cos_x, sin_x, 0.f,
                        0.f, -sin_x, cos_x, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        float transform_OY[16] =
                {
                        cos_y, 0.f, sin_y, 0.f,
                        0.f, 1.f, 0.f, 0.f,
                        -sin_y, 0.f, cos_y, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glUniformMatrix4fv(transform_scale_location, 1, GL_TRUE, transform_scale);
        glUniformMatrix4fv(transform_move_location, 1, GL_TRUE, transform_move);
        glUniformMatrix4fv(transform_OX_location, 1, GL_TRUE, transform_OX);
        glUniformMatrix4fv(transform_OY_location, 1, GL_TRUE, transform_OY);

        data.update_vertices(time);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_y);
        glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(data.positions_y[0]) * data.positions_y.size(),
                data.positions_y.data(),
                GL_STATIC_DRAW
        );

        glBindBuffer(GL_ARRAY_BUFFER, vbo_g);
        glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(data.colors_g[0]) * data.colors_g.size(),
                data.colors_g.data(),
                GL_STATIC_DRAW
        );

        data.update_all_isolines();

        glBindBuffer(GL_ARRAY_BUFFER, vbo_il);
        glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(data.isolines_vertices[0]) * data.isolines_vertices.size(),
                data.isolines_vertices.data(),
                GL_STATIC_DRAW
        );

        glBindVertexArray(vao_main);
        glDrawElements(GL_TRIANGLES, data.indices.size(), GL_UNSIGNED_INT, (void *) (0));

        glBindVertexArray(vao_il);
        glLineWidth(3.5f);
        glDrawArrays(GL_LINES, 0, data.isolines_vertices.size());
        glLineWidth(1.f);

        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(vao_axes);
        glPointSize(5.f);
        auto offset = sizeof(indices_axes[0]);
        glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, (void *) (0 * offset));
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, (void *) (1 * offset));
        glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, (void *) (2 * offset));
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, (void *) (3 * offset));
        glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, (void *) (4 * offset));
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, (void *) (5 * offset));


        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
