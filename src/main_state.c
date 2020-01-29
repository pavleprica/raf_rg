#include <main_state.h>
#include <glad/glad.h>
#include <math.h>


#include <rafgl.h>

#include <game_constants.h>

#define TERRAIN_SIZE 3.0
#define TERRAIN_COUNT 9
#define TERRAIN_LOD_DEFAULT 3

#define TERRAIN_LOD_LOW 3
#define TERRAIN_LOD_MID 2
#define TERRAIN_LOD_HIGH 1

#define GLOBAL_TERRAIN_SIZE (3.0 * TERRAIN_SIZE)

typedef struct middle_coordinate {
    float x, z;
} mid_coordinate;

typedef struct terrain_matrix_tile {
    float x,z;
    int LOD;
    mid_coordinate mid;
    mat4_t model;
} tile;



static rafgl_texture_t tex_skybox;

static GLuint skybox_shader, skybox_uni_V, skybox_uni_P, skybox_uni_skybox_sampler;
static GLuint object_shader, object_uni_V, object_uni_P, object_uni_M;
static GLuint object_uni_light_colour, object_uni_light_direction, object_uni_camera_position, object_uni_object_colour;

static GLuint uni_terrain_size, uni_heightmap_tex;

static GLuint uni_time, uni_x_offset;

static int screen_width, screen_height;

rafgl_raster_t heightmap;
rafgl_texture_t heightmap_tex;

static GLuint terrain_shader, terrain_uni_V, terrain_uni_P, terrain_uni_M;

GLuint uni_light_direction, uni_camera_position;


static rafgl_meshPUN_t terrain_lod1, terrain_lod2, terrain_lod3;
tile tiles[TERRAIN_COUNT];

tile init_tile( int x, int z ) {

    tile t;
    t.x = x * TERRAIN_SIZE;
    t.z = z * TERRAIN_SIZE;
    t.LOD = TERRAIN_LOD_DEFAULT;
    t.model = m4_mul(m4_identity(), m4_translation( vec3m(t.x, 0.0, t.z) ));

    mid_coordinate mid;
    mid.x =  t.x + TERRAIN_SIZE  / 2;
    mid.z =  t.z + TERRAIN_SIZE  / 2;

    t.mid = mid;
    return t;
}

void init_terrain() {

    int i,j;
    int pointer = 0;
    for(i = 0 ; i < 3 ; i++){
        for(j = 0 ; j < 3 ; j++) {
            tiles[pointer++] = init_tile(j, i);
        }
    }
}

void main_state_init(GLFWwindow *window, void *args, int width, int height)
{
    rafgl_raster_t raster;
    rafgl_raster_load_from_image(&heightmap, "res/images/heightmap1.png");
    rafgl_texture_init(&heightmap_tex);
    rafgl_texture_load_from_raster(&heightmap_tex, &heightmap);

    glBindTexture(GL_TEXTURE_2D, heightmap_tex.tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    rafgl_texture_init(&tex_skybox);
    rafgl_texture_load_cubemap_named(&tex_skybox, "above_the_sea", "jpg");

    skybox_shader = rafgl_program_create_from_name("skybox_shader");
    skybox_uni_V = glGetUniformLocation(skybox_shader, "uni_V");
    skybox_uni_P = glGetUniformLocation(skybox_shader, "uni_P");
    skybox_uni_skybox_sampler = glGetUniformLocation(skybox_shader, "skybox_sampler");

    object_shader = rafgl_program_create_from_name("lambert_shader");
    object_uni_P = glGetUniformLocation(object_shader, "uni_P");
    object_uni_V = glGetUniformLocation(object_shader, "uni_V");
    object_uni_M = glGetUniformLocation(object_shader, "uni_M");
    object_uni_camera_position = glGetUniformLocation(object_shader, "camera_position");
    object_uni_light_colour = glGetUniformLocation(object_shader, "light_colour");
    object_uni_light_direction = glGetUniformLocation(object_shader, "light_direction");
    object_uni_object_colour = glGetUniformLocation(object_shader, "object_colour");


    rafgl_meshPUN_load_plane(&terrain_lod3, TERRAIN_SIZE, TERRAIN_SIZE, 10, 10);
    rafgl_meshPUN_load_plane(&terrain_lod2, TERRAIN_SIZE, TERRAIN_SIZE, 15, 15);
    rafgl_meshPUN_load_plane(&terrain_lod1, TERRAIN_SIZE, TERRAIN_SIZE, 20, 20);
    init_terrain();

    terrain_shader = rafgl_program_create_from_name("terrain_shader");
    terrain_uni_P = glGetUniformLocation(terrain_shader, "uni_P");
    terrain_uni_V = glGetUniformLocation(terrain_shader, "uni_V");
    terrain_uni_M = glGetUniformLocation(terrain_shader, "uni_M");
    uni_terrain_size = glGetUniformLocation(terrain_shader, "uni_terrain_size");
    uni_heightmap_tex = glGetUniformLocation(terrain_shader, "uni_heightmap_tex");
    uni_time = glGetUniformLocation(terrain_shader, "uni_time");
    uni_light_direction = glGetUniformLocation(terrain_shader, "uni_light_direction");
    uni_camera_position = glGetUniformLocation(terrain_shader, "uni_camera_position");
    uni_x_offset = glGetUniformLocation(terrain_shader, "uni_x_offset");

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
}

mat4_t model, view, projection, view_projection;

mat4_t terrain_model;

/* field of view */
float fov = 75.0f;

vec3_t camera_position = vec3m(0.0f, 3.0f, 5.0f);
vec3_t camera_up = vec3m(0.0f, 1.0f, 0.0f);
vec3_t aim_dir = vec3m(0.0f, 0.0f, -1.0f);

float camera_angle = -M_PIf * 0.5f;
float angle_speed = 0.2f * M_PIf;
float move_speed = 2.4f;

float hoffset = 0;

int rotate = 0;
float model_angle = 0.0f;

float time = 0.0f;
int reshow_cursor_flag = 0;
int last_lmb = 0;

float sensitivity = 1.0f;


static vec3_t light_direction = vec3m(-1.0f, -1.0f, -1.0f);
static vec3_t light_colour = vec3m(1.0f, 0.9f, 0.7f);

int polygon = 0;

float camera_angle_speed = 25.0f;

/*
 * Euklidova distanca, pitati Mareta da li je to precizno??
 */
float calcDistance( vec3_t v1, vec3_t v2 ) {
    return v3_length(v3_sub(v1, v2));
}

float time;

mat4_t baseModel;

void main_state_update(GLFWwindow *window, float delta_time, rafgl_game_data_t *game_data, void *args)
{
//    time += delta_time * 2;

    if(game_data->keys_pressed['R']) {
        polygon = polygon == 1 ? 0 : 1;
    }

    //UP
    if(game_data->keys_down['I']) {
        hoffset += sensitivity * camera_angle_speed / game_data->raster_height;
    }
    //DOWN
    if(game_data->keys_down['K']) {
        hoffset -= sensitivity * camera_angle_speed / game_data->raster_height;
    }
    //LEFT
    if(game_data->keys_down['J']) {
        camera_angle -= sensitivity * camera_angle_speed / game_data->raster_width;
    }
    //RIGHT
    if(game_data->keys_down['L']) {
        camera_angle += sensitivity * camera_angle_speed / game_data->raster_width;
    }

    screen_width = game_data->raster_width;
    screen_height = game_data->raster_height;

    time += delta_time;
    model_angle += delta_time * rotate;

    if(!game_data->keys_down[RAFGL_KEY_LEFT_SHIFT])
    {
        angle_speed = 0.2f * M_PIf;
        move_speed = 2.4f;
        sensitivity = 1.0f;
    }
    else
    {
        angle_speed = 5 * 0.2f * M_PIf;
        move_speed = 5 * 2.4f;
        sensitivity = 10.0f;
    }


    if(game_data->is_lmb_down)
    {

        if(reshow_cursor_flag == 0)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        }

        float ydelta = game_data->mouse_pos_y - game_data->raster_height / 2;
        float xdelta = game_data->mouse_pos_x - game_data->raster_width / 2;

        if(!last_lmb)
        {
            ydelta = 0;
            xdelta = 0;
        }

        hoffset -= sensitivity * ydelta / game_data->raster_height;
        camera_angle += sensitivity * xdelta / game_data->raster_width;

        glfwSetCursorPos(window, game_data->raster_width / 2, game_data->raster_height / 2);
        reshow_cursor_flag = 1;
    }
    else if(reshow_cursor_flag)
    {
        reshow_cursor_flag = 0;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    last_lmb = game_data->is_lmb_down;

    aim_dir = v3_norm(vec3(cosf(camera_angle), hoffset, sinf(camera_angle)));

    if(game_data->keys_down['W']) camera_position = v3_add(camera_position, v3_muls(aim_dir, move_speed * delta_time));
    if(game_data->keys_down['S']) camera_position = v3_add(camera_position, v3_muls(aim_dir, -move_speed * delta_time));

    vec3_t right = v3_cross(aim_dir, vec3(0.0f, 1.0f, 0.0f));
    if(game_data->keys_down['D']) camera_position = v3_add(camera_position, v3_muls(right, move_speed * delta_time));
    if(game_data->keys_down['A']) camera_position = v3_add(camera_position, v3_muls(right, -move_speed * delta_time));

//    if(game_data->keys_pressed['R']) rotate = !rotate;

    if(game_data->keys_down[RAFGL_KEY_ESCAPE]) glfwSetWindowShouldClose(window, GLFW_TRUE);

    if(game_data->keys_down[RAFGL_KEY_SPACE]) camera_position.y += 1.0f * delta_time * move_speed;
    if(game_data->keys_down[RAFGL_KEY_LEFT_CONTROL]) camera_position.y -= 1.0f * delta_time * move_speed;

    float aspect = ((float)(game_data->raster_width)) / game_data->raster_height;
    projection = m4_perspective(fov, aspect, 0.1f, 100.0f);

    view = m4_look_at(camera_position, v3_add(camera_position, aim_dir), camera_up);

//    printf("DISTANCE: %f\n", calcDistance(camera_position, vec3m(tiles[0].mid.x, 0.0, tiles[0].mid.z)));

    int ter;
    for(ter = 0 ; ter < TERRAIN_COUNT ; ter++) {
        float dist = calcDistance(camera_position, vec3m(tiles[ter].mid.x, 0.0, tiles[ter].mid.z));
        if(dist > 10.0)
            tiles[ter].LOD = TERRAIN_LOD_LOW;
        if(dist > 4.0 && dist <= 10.0)
            tiles[ter].LOD = TERRAIN_LOD_MID;
        if(dist <= 4.0)
            tiles[ter].LOD = TERRAIN_LOD_HIGH;
    }

}

void main_state_render(GLFWwindow *window, void *args)
{

    // glBindFramebuffer(GL_FRAMsEBUFFER, 0);
    // glViewport(0, 0, screen_width, screen_height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(terrain_shader);

    if(polygon) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightmap_tex.tex_id);

    glUniform1i(glGetUniformLocation(terrain_shader, "heightmap"), 0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    int i;
    for(i = 0 ; i < TERRAIN_COUNT ; i++) {
        rafgl_meshPUN_t terrain_meshPUN = tiles[i].LOD == 3 ? terrain_lod3 : tiles[i].LOD == 2 ? terrain_lod2 : terrain_lod1;
        glBindVertexArray(terrain_meshPUN.vao_id);


        glUniform1f(uni_terrain_size, GLOBAL_TERRAIN_SIZE);
        glUniform1f(uni_time, time);
        glUniform1f(uni_x_offset, tiles[i].x);

        glUniformMatrix4fv(terrain_uni_V, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(terrain_uni_P, 1, GL_FALSE, projection.m);
        glUniformMatrix4fv(terrain_uni_M, 1, GL_FALSE, tiles[i].model.m);
        glDrawArrays(GL_TRIANGLES, 0, terrain_meshPUN.vertex_count);

        glBindVertexArray(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

//    rafgl_texture_show(&heightmap_tex, 0);

}


void main_state_cleanup(GLFWwindow *window, void *args)
{

}
