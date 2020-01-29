#version 330

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;

out vec3 pass_world_position;

uniform float uni_time;

uniform mat4 uni_V;
uniform mat4 uni_P;
uniform mat4 uni_M;

uniform float uni_x_offset;

uniform sampler2D heightmap;

void main()
{


	float y_offset = 0.35 * sin(uni_time / 2 + ( (position.x + uni_x_offset) ));

	vec3 vertex = vec3(position.x, texture(heightmap, uv).r * 2 + y_offset, position.z);

	vec4 world_position = uni_M * vec4(vertex, 1.0);
	
	pass_world_position = world_position.xyz;
	
	gl_Position = uni_P * uni_V * world_position;

}