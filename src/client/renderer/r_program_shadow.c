/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

// these are the variables defined in the GLSL shader
typedef struct r_shadow_program_s {
	r_uniform_matrix4fv_t matrix;
	r_uniform4fv_t light;
	r_uniform4fv_t plane;

	r_uniformfog_t fog;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;

	r_uniform4fv_t current_color;
} r_shadow_program_t;

static r_shadow_program_t r_shadow_program;

/**
 * @brief
 */
void R_InitProgram_shadow(void) {
	r_shadow_program_t *p = &r_shadow_program;

	const vec4_t light = { 0.0, 0.0, 0.0, 1.0 };
	const vec4_t plane = { 0.0, 0.0, 1.0, 0.0 };

	R_ProgramVariable(&p->matrix, R_UNIFORM_MAT4, "MATRIX");
	R_ProgramVariable(&p->light, R_UNIFORM_VEC4, "LIGHT");
	R_ProgramVariable(&p->plane, R_UNIFORM_VEC4, "PLANE");
	
	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START");
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END");
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY");

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR");

	R_ProgramParameterMatrix4fv(&p->matrix, (GLfloat *) matrix4x4_identity.m);
	R_ProgramParameter4fv(&p->light, light);
	R_ProgramParameter4fv(&p->plane, plane);

	R_ProgramParameter1f(&p->fog.density, 0.0);
	
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	R_ProgramParameter4fv(&p->current_color, white);
}

/**
 * @brief Calculates a perspective shearing matrix for the current shadow and
 * uploads it as a uniform variable to the shader.
 *
 * ftp://ftp.sgi.com/opengl/contrib/blythe/advanced99/notes/node192.html
 */
void R_UseProgram_shadow(void) {
	r_shadow_program_t *p = &r_shadow_program;

	if (!r_view.current_entity || !r_view.current_shadow)
		return;

	const r_entity_t *e = r_view.current_entity;
	const r_shadow_t *s = r_view.current_shadow;

	// transform the light position and shadow plane into model space
	vec4_t light, plane;

	Matrix4x4_Transform(&e->inverse_matrix, s->illumination->light.origin, light);
	light[3] = 1.0;

	Matrix4x4_TransformQuakePlane(&e->inverse_matrix, s->plane.normal, s->plane.dist, plane);
	plane[3] = -plane[3];

	// calculate the perspective-shearing matrix
	const vec_t dot = DotProduct(light, plane) + light[3] * plane[3];

	matrix4x4_t matrix;
	matrix.m[0][0] = dot - light[0] * plane[0];
	matrix.m[1][0] = 0.0 - light[0] * plane[1];
	matrix.m[2][0] = 0.0 - light[0] * plane[2];
	matrix.m[3][0] = 0.0 - light[0] * plane[3];
	matrix.m[0][1] = 0.0 - light[1] * plane[0];
	matrix.m[1][1] = dot - light[1] * plane[1];
	matrix.m[2][1] = 0.0 - light[1] * plane[2];
	matrix.m[3][1] = 0.0 - light[1] * plane[3];
	matrix.m[0][2] = 0.0 - light[2] * plane[0];
	matrix.m[1][2] = 0.0 - light[2] * plane[1];
	matrix.m[2][2] = dot - light[2] * plane[2];
	matrix.m[3][2] = 0.0 - light[2] * plane[3];
	matrix.m[0][3] = 0.0 - light[3] * plane[0];
	matrix.m[1][3] = 0.0 - light[3] * plane[1];
	matrix.m[2][3] = 0.0 - light[3] * plane[2];
	matrix.m[3][3] = dot - light[3] * plane[3];

	R_ProgramParameterMatrix4fv(&p->matrix, (GLfloat *) matrix.m);

	Matrix4x4_Transform(&r_view.matrix, s->illumination->light.origin, light);
	light[3] = s->illumination->light.radius;

	R_ProgramParameter4fv(&p->light, light);

	Matrix4x4_TransformQuakePlane(&r_view.matrix, s->plane.normal, s->plane.dist, plane);
	plane[3] = -plane[3];

	R_ProgramParameter4fv(&p->plane, plane);
}

/**
 * @brief
 */
void R_UseFog_shadow(const r_fog_parameters_t *fog) {

	r_shadow_program_t *p = &r_shadow_program;

	if (fog && fog->density)
	{
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
	}
	else
		R_ProgramParameter1f(&p->fog.density, 0.0);
}

/**
 * @brief
 */
void R_UseMatrices_shadow(const matrix4x4_t *projection, const matrix4x4_t *modelview, const matrix4x4_t *texture) {
	
	r_shadow_program_t *p = &r_shadow_program;

	if (projection)
		R_ProgramParameterMatrix4fv(&p->projection_mat, (const GLfloat *) projection->m);

	if (modelview)
		R_ProgramParameterMatrix4fv(&p->modelview_mat, (const GLfloat *) modelview->m);
}

/**
 * @brief
 */
void R_UseCurrentColor_shadow(const vec4_t color) {

	r_shadow_program_t *p = &r_shadow_program;
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color)
		R_ProgramParameter4fv(&p->current_color, color);
	else
		R_ProgramParameter4fv(&p->current_color, white);
}