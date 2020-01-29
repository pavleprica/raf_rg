#version 330


in vec3 pass_world_position;

out vec4 out_colour;

void main()
{


	vec3 final_color = pass_world_position / 2;

	out_colour = vec4(final_color, 1.0);

}