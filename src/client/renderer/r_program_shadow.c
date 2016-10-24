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
	r_uniform4fv_t light;
	r_uniform4fv_t plane;
	r_uniform1f_t time_fraction;

	r_uniform_fog_t fog;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;
	r_uniform_matrix4fv_t shadow_mat;

	r_uniform4fv_t current_color;
} r_shadow_program_t;

static r_shadow_program_t r_shadow_program;

/**
 * @brief
 */
void R_PreLink_shadow(const r_program_t *program) {
	
	R_BindAttributeLocation(program, "POSITION", R_ARRAY_VERTEX);
	R_BindAttributeLocation(program, "NEXT_POSITION", R_ARRAY_NEXT_VERTEX);
}

/**
 * @brief
 */
void R_InitProgram_shadow(r_program_t *program) {
	r_shadow_program_t *p = &r_shadow_program;

	R_ProgramVariable(&program->attributes[R_ARRAY_VERTEX], R_ATTRIBUTE, "POSITION");
	R_ProgramVariable(&program->attributes[R_ARRAY_NEXT_VERTEX], R_ATTRIBUTE, "NEXT_POSITION");

	const vec4_t light = { 0.0, 0.0, 0.0, 1.0 };
	const vec4_t plane = { 0.0, 0.0, 1.0, 0.0 };

	R_ProgramVariable(&p->shadow_mat, R_UNIFORM_MAT4, "SHADOW_MAT");
	R_ProgramVariable(&p->light, R_UNIFORM_VEC4, "LIGHT");
	R_ProgramVariable(&p->plane, R_UNIFORM_VEC4, "PLANE");
	
	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START");
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END");
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY");

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");

	R_ProgramVariable(&p->current_color, R_UNIFORM_VEC4, "GLOBAL_COLOR");

	R_ProgramVariable(&p->time_fraction, R_UNIFORM_FLOAT, "TIME_FRACTION");

	R_ProgramParameterMatrix4fv(&p->shadow_mat, (GLfloat *) matrix4x4_identity.m);
	R_ProgramParameter4fv(&p->light, light);
	R_ProgramParameter4fv(&p->plane, plane);

	R_ProgramParameter1f(&p->fog.density, 0.0);
	
	const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	R_ProgramParameter4fv(&p->current_color, white);

	R_ProgramParameter1f(&p->time_fraction, 0.0f);
}

/**
 * @brief
 */
void R_UseFog_shadow(const r_fog_parameters_t *fog) {

	r_shadow_program_t *p = &r_shadow_program;

	if (fog && fog->density) {
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
	} else {
		R_ProgramParameter1f(&p->fog.density, 0.0);
	}
}

/**
 * @brief
 */
void R_UseMatrices_shadow(const matrix4x4_t *matrices) {
	
	r_shadow_program_t *p = &r_shadow_program;

	R_ProgramParameterMatrix4fv(&p->projection_mat, (const GLfloat *) matrices[R_MATRIX_PROJECTION].m);
	R_ProgramParameterMatrix4fv(&p->modelview_mat, (const GLfloat *) matrices[R_MATRIX_MODELVIEW].m);

	R_ProgramParameterMatrix4fv(&p->shadow_mat, (const GLfloat *) matrices[R_MATRIX_SHADOW].m);
	R_ProgramParameter4fv(&p->light, r_view.current_shadow_light);
	R_ProgramParameter4fv(&p->plane, r_view.current_shadow_plane);
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

/**
 * @brief
 */
void R_UseInterpolation_shadow(const float time_fraction) {

	r_shadow_program_t *p = &r_shadow_program;

	R_ProgramParameter1f(&p->time_fraction, time_fraction);
}