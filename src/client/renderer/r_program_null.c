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
typedef struct {
	r_sampler2d_t sampler0;

	r_uniformfog_t fog;

	r_uniform_matrix4fv_t projection_mat;
	r_uniform_matrix4fv_t modelview_mat;
	r_uniform_matrix4fv_t texture_mat;
} r_null_program_t;

static r_null_program_t r_null_program;

/**
 * @brief
 */
void R_InitProgram_null(void) {

	r_null_program_t *p = &r_null_program;

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START");
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END");
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR");
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY");

	R_ProgramVariable(&p->projection_mat, R_UNIFORM_MAT4, "PROJECTION_MAT");
	R_ProgramVariable(&p->modelview_mat, R_UNIFORM_MAT4, "MODELVIEW_MAT");
	R_ProgramVariable(&p->texture_mat, R_UNIFORM_MAT4, "TEXTURE_MAT");

	R_ProgramParameter1i(&p->sampler0, 0);

	R_ProgramParameter1f(&p->fog.density, 0.0);
}

/**
 * @brief
 */
void R_UseFog_null(const r_fog_parameters_t *fog) {

	r_null_program_t *p = &r_null_program;

	if (fog && fog->density)
	{
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
		R_ProgramParameter3fv(&p->fog.color, fog->color);
	}
	else
		R_ProgramParameter1f(&p->fog.density, 0.0);
}

/**
 * @brief
 */
void R_UseMatrices_null(const matrix4x4_t *projection, const matrix4x4_t *modelview, const matrix4x4_t *texture) {

	r_null_program_t *p = &r_null_program;

	if (projection)
		R_ProgramParameterMatrix4fv(&p->projection_mat, (const GLfloat *) projection->m);

	if (modelview)
		R_ProgramParameterMatrix4fv(&p->modelview_mat, (const GLfloat *) modelview->m);

	if (texture)
		R_ProgramParameterMatrix4fv(&p->texture_mat, (const GLfloat *) texture->m);
}
