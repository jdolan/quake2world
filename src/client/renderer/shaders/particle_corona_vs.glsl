/**
 * @brief Corona vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

in vec3 POSITION;
in vec4 COLOR;
in float SCALE;

out VertexData {
	vec4 color;
	vec2 texcoord0;
	vec2 texcoord1;
	float scale;
	float roll;
	vec3 end;
	int type;
	FOG_VARIABLE;
};

/**
 * @brief Shader entry point.
 */
void main(void) {

	// mvp transform into clip space
	gl_Position = vec4(POSITION, 1.0);

	texcoord0 = vec2(1.0, 1.0);
	texcoord1 = vec2(0.0, 0.0);
	scale = SCALE;
	roll = 0;
	end = vec3(0.0);
	type = 0;

	// pass the color through as well
	color = COLOR;

	fog = FogVertex();
}
