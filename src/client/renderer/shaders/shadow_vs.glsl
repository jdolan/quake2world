/**
 * @brief Planar shadows vertex shader.
 */

#version 330

#define VERTEX_SHADER

#include "fog_inc.glsl"
#include "matrix_inc.glsl"

uniform mat4 SHADOW_MAT;
uniform vec4 LIGHT;
uniform float TIME_FRACTION;

in vec3 POSITION;
in vec3 NEXT_POSITION;

out VertexData {
	vec4 point;
	FOG_VARIABLE;
};

/**
 * @brief
 */
vec4 ShadowVertex(void) {
	return MODELVIEW_MAT * SHADOW_MAT * vec4(mix(POSITION, NEXT_POSITION, TIME_FRACTION), 1.0);
}

/**
 * @brief Calculate the fog mix factor. This is different for shadows.
 */
float FogShadowVertex(void) {
	return clamp(((gl_Position.z - FOG.START) / (FOG.END - FOG.START) / point.w) * FOG.DENSITY, 0.0, 1.0);
}

/**
 * @brief Program entry point.
 */
void main(void) {
	
	// mvp transform into clip space
	gl_Position = PROJECTION_MAT * point;

	point = ShadowVertex();
	    
	fog = FogShadowVertex();
}
