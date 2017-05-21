/**
 * @brief Null vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "matrix_inc.glsl"
#include "fog_inc.glsl"

uniform vec4 GLOBAL_COLOR;

in vec3 POSITION;
in vec2 TEXCOORD0;
in vec2 TEXCOORD1;
in vec4 COLOR;
in float SCALE;
in float ROLL;
in vec3 END;
in int TYPE;

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

	texcoord0 = TEXCOORD0;
	texcoord1 = TEXCOORD1;
	scale = SCALE;
	roll = ROLL;
	end = END;
	type = TYPE;

	// pass the color through as well
	color = COLOR * GLOBAL_COLOR;

	fog = FogVertex();
}
