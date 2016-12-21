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

static cvar_t *r_get_error;

r_state_t r_state;

const vec2_t default_texcoords[4] = { // useful for particles, pics, etc..
	{ 0.0, 0.0 },
	{ 1.0, 0.0 },
	{ 1.0, 1.0 },
	{ 0.0, 1.0 }
};

/**
 * @brief The active matrices are private, to prevent being overwritten
 * without dirtyage.
 */
static matrix4x4_t active_matrices[R_MATRIX_TOTAL];

/**
 * @brief Queries OpenGL for any errors and prints them as warnings.
 */
void R_GetError_(const char *function, const char *msg) {
	GLenum err;
	char *s;

	if (!r_get_error->integer) {
		return;
	}

	while (true) {

		if ((err = glGetError()) == GL_NO_ERROR) {
			return;
		}

		switch (err) {
			case GL_INVALID_ENUM:
				s = "GL_INVALID_ENUM";
				break;
			case GL_INVALID_VALUE:
				s = "GL_INVALID_VALUE";
				break;
			case GL_INVALID_OPERATION:
				s = "GL_INVALID_OPERATION";
				break;
			case GL_OUT_OF_MEMORY:
				s = "GL_OUT_OF_MEMORY";
				break;
			default:
				s = va("%" PRIx32, err);
				break;
		}
		
		if (r_get_error->integer >= 2) {
			Sys_Backtrace();
		}

		Com_Warn("%s threw %s: %s.\n", function, s, msg);
	}
}

/**
 * @brief Sets the current color. Pass NULL to reset to default.
 */
void R_Color(const vec4_t color) {
	static const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color) {
		Vector4Copy(color, r_state.current_color);
	} else {
		Vector4Copy(white, r_state.current_color);
	}
}

/**
 * @brief Set the active texture unit.
 */
void R_SelectTexture(r_texunit_t *texunit) {

	if (texunit == r_state.active_texunit) {
		return;
	}

	r_state.active_texunit = texunit;

	glActiveTexture(texunit->texture);

	r_view.num_state_changes[R_STATE_ACTIVE_TEXTURE]++;
}

/**
 * @brief Request that a texnum be bound to the specified texture unit.
 * returns true if it was indeed bound (for statistical analysis)
 */
void R_BindUnitTexture(r_texunit_t *texunit, GLuint texnum) {

	if (texnum == texunit->texnum) {
		return;
	}

	R_SelectTexture(texunit);

	// bind the texture
	glBindTexture(GL_TEXTURE_2D, texnum);

	r_view.num_state_changes[R_STATE_BIND_TEXTURE]++;

	texunit->texnum = texnum;

	r_view.num_binds[texunit - r_state.texunits]++;
}

/**
 * @brief
 */
void R_BlendFunc(GLenum src, GLenum dest) {

	if (r_state.blend_src == src && r_state.blend_dest == dest) {
		return;
	}

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);

	r_view.num_state_changes[R_STATE_BLEND_FUNC]++;
}

/**
 * @brief
 */
void R_EnableDepthMask(_Bool enable) {

	if (r_state.depth_mask_enabled == enable) {
		return;
	}

	r_state.depth_mask_enabled = enable;

	if (enable) {
		glDepthMask(GL_TRUE);
	} else {
		glDepthMask(GL_FALSE);
	}

	r_view.num_state_changes[R_STATE_DEPTHMASK]++;
}

/**
 * @brief
 */
void R_EnableBlend(_Bool enable) {

	if (r_state.blend_enabled == enable) {
		return;
	}

	r_state.blend_enabled = enable;

	if (enable) {
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}

	r_view.num_state_changes[R_STATE_ENABLE_BLEND]++;
}

/**
 * @brief
 */
void R_EnableAlphaTest(vec_t threshold) {

	if (r_state.alpha_threshold == threshold) {
		return;
	}

	r_state.alpha_threshold = threshold;
}

/**
 * @brief Enables the stencil test for e.g. rendering shadow volumes.
 */
void R_EnableStencilTest(GLenum pass, _Bool enable) {

	if (r_state.stencil_test_enabled == enable) {
		return;
	}

	r_state.stencil_test_enabled = enable;

	if (enable) {
		glEnable(GL_STENCIL_TEST);
	} else {
		glDisable(GL_STENCIL_TEST);
	}

	r_view.num_state_changes[R_STATE_ENABLE_STENCIL]++;

	if (r_state.stencil_op_pass != pass) {

		glStencilOp(GL_KEEP, GL_KEEP, pass);
		r_state.stencil_op_pass = pass;

		r_view.num_state_changes[R_STATE_STENCIL_OP]++;
	}
}

/**
 * @brief Sets stencil func parameters.
 */
void R_StencilFunc(GLenum func, GLint ref, GLuint mask) {

	if (r_state.stencil_func_func == func &&
	        r_state.stencil_func_ref == ref &&
	        r_state.stencil_func_mask == mask) {
		return;
	}

	r_state.stencil_func_func = func;
	r_state.stencil_func_ref = ref;
	r_state.stencil_func_mask = mask;

	glStencilFunc(func, ref, mask);

	r_view.num_state_changes[R_STATE_STENCIL_FUNC]++;
}

/**
 * @brief
 */
void R_PolygonOffset(GLfloat factor, GLfloat units) {

	if (r_state.polygon_offset_factor == factor &&
	        r_state.polygon_offset_units == units) {
		return;
	}

	glPolygonOffset(factor, units);

	r_state.polygon_offset_factor = factor;
	r_state.polygon_offset_units = units;

	r_view.num_state_changes[R_STATE_POLYGON_OFFSET]++;
}

/**
 * @brief Enables polygon offset fill for decals, etc.
 */
void R_EnablePolygonOffset(_Bool enable) {

	if (r_state.polygon_offset_enabled == enable) {
		return;
	}

	r_state.polygon_offset_enabled = enable;

	if (enable) {
		glEnable(GL_POLYGON_OFFSET_FILL);
	} else {
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	r_view.num_state_changes[R_STATE_ENABLE_POLYGON_OFFSET]++;

	R_PolygonOffset(-1.0, 1.0);
}

/**
 * @brief Enable the specified texture unit for multi-texture operations. This is not
 * necessary for texture units only accessed by GLSL shaders.
 */
void R_EnableTexture(r_texunit_t *texunit, _Bool enable) {

	if (enable == texunit->enabled) {
		return;
	}

	texunit->enabled = enable;
}

/**
 * @brief
 */
void R_EnableColorArray(_Bool enable) {

	if (r_state.color_array_enabled == enable) {
		return;
	}

	r_state.color_array_enabled = enable;
}

/**
 * @brief Enables hardware-accelerated lighting with the specified program. This
 * should be called after any texture units which will be active for lighting
 * have been enabled.
 */
void R_EnableLighting(const r_program_t *program, _Bool enable) {

	if (program == NULL) {
		program = program_null;
	}

	// if the program can't use lights, lighting can't really
	// be enabled on it.
	if (enable && !program->UseLight) {
		Com_Debug(DEBUG_RENDERER, "Attempted to use lights on a non-lightable program\n");
		return;
	}

	// use the program here. this is done regardless
	// of if lighting is supported.
	R_UseProgram(program);

	// enable fog if supported by the program.
	R_EnableFog(enable);

	R_GetError(NULL);

	// if we don't actually have lighting support,
	// don't bother turning on the lights.
	if (!r_lighting->value || r_state.lighting_enabled == enable) {
		return;
	}

	r_state.lighting_enabled = enable;

	// set up all the lights if we're enabling
	if (enable) {
		R_ResetLights();
		R_GetError(NULL);
	}
}

/**
 * @brief Enables alpha-blended, stencil-test shadows.
 */
void R_EnableShadow(const r_program_t *program, _Bool enable) {

	if (enable && (!program || !program->id)) {
		return;
	}

	if (!r_shadows->value || r_state.shadow_enabled == enable) {
		return;
	}

	r_state.shadow_enabled = enable;

	if (enable) {
		R_UseProgram(program);
	} else {
		R_UseProgram(program_null);
	}

	R_EnableFog(enable);

	R_GetError(NULL);
}

/**
 * @brief Enables the warp shader for drawing liquids and other effects.
 */
void R_EnableWarp(const r_program_t *program, _Bool enable) {

	if (enable && (!program || !program->id)) {
		return;
	}

	if (!r_warp->value || r_state.warp_enabled == enable) {
		return;
	}

	r_state.warp_enabled = enable;

	if (enable) {
		R_UseProgram(program);
	} else {
		R_UseProgram(program_null);
	}

	R_EnableFog(enable);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_EnableShell(const r_program_t *program, _Bool enable) {

	if (enable && (!program || !program->id)) {
		return;
	}

	if (!r_shell->value || r_state.shell_enabled == enable) {
		return;
	}

	r_state.shell_enabled = enable;

	if (enable) {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

		R_BindDiffuseTexture(r_image_state.shell->texnum);

		R_UseProgram(program);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		R_UseProgram(program_null);
	}

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_EnableFog(_Bool enable) {

	if (!r_state.active_program) {
		return;
	}

	if (!r_fog->value || r_state.fog_enabled == enable || !r_state.active_program->UseFog) {
		return;
	}

	r_state.fog_enabled = false;

	if (enable) {
		if ((r_view.weather & WEATHER_FOG) || r_fog->integer == 2) {

			r_state.fog_enabled = true;

			r_state.active_fog_parameters.start = FOG_START;
			r_state.active_fog_parameters.end = FOG_END;
			VectorCopy(r_view.fog_color, r_state.active_fog_parameters.color);
			r_state.active_fog_parameters.density = r_view.fog_color[3];
		} else {
			r_state.active_fog_parameters.density = 0.0;
		}
	} else {
		r_state.active_fog_parameters.density = 0.0;
	}
}

/**
 * @brief Setup the GLSL program for the specified material. If no program is
 * bound, this function simply returns.
 */
void R_UseMaterial(const r_material_t *material) {

	if (!r_state.active_program) {
		return;
	}

	if (r_state.active_material == material) {
		return;
	}

	r_state.active_material = material;

	if (r_state.active_program->UseMaterial) {
		r_state.active_program->UseMaterial(material);
	}

	R_GetError(material ? material->diffuse->media.name : r_state.active_program->name);
}

/**
 * @brief Push the active matrix into the stack
 */
void R_PushMatrix(const r_matrix_id_t id) {

	if (r_state.matrix_stacks[id].depth == MAX_MATRIX_STACK) {
		Com_Error(ERROR_DROP, "Matrix stack overflow");
	}

	Matrix4x4_Copy(&r_state.matrix_stacks[id].matrices[r_state.matrix_stacks[id].depth++], &active_matrices[id]);
}

/**
 * @brief Pop a saved matrix from the stack
 */
void R_PopMatrix(const r_matrix_id_t id) {

	if (r_state.matrix_stacks[id].depth == 0) {
		Com_Error(ERROR_DROP, "Matrix stack underflow");
	}

	R_SetMatrix(id, &r_state.matrix_stacks[id].matrices[--r_state.matrix_stacks[id].depth]);
}

/**
 * @brief Change the matrix and mark it as being dirty.
 */
void R_SetMatrix(const r_matrix_id_t id, const matrix4x4_t *matrix) {

	Matrix4x4_Copy(&active_matrices[id], matrix);

	for (r_program_id_t i = 0; i < R_PROGRAM_TOTAL; i++) {

		if (r_state.programs[i].matrix_uniforms[id].location != -1) {
			r_state.programs[i].matrix_dirty[id] = true;
		}
	}
}

/**
 * @brief Fetch a matrix from the current view state.
 */
void R_GetMatrix(const r_matrix_id_t id, matrix4x4_t *matrix) {

	Matrix4x4_Copy(matrix, &active_matrices[id]);
}

/**
 * @brief Fetch a const pointer to the active matrix. This should
 * only be used if you only need to read from the matrix and not write.
 * If you need to both read and write, use R_GetMatrix.
 */
const matrix4x4_t *R_GetMatrixPtr(const r_matrix_id_t id) {

	return &active_matrices[id];
}

/**
 * @brief Uploads matrices to the currently loaded program.
 */
void R_UseMatrices(void) {

	_Bool any_changed = false;

	for (r_matrix_id_t i = 0; i < R_MATRIX_TOTAL; i++) {

		if (!r_state.active_program->matrix_dirty[i]) {
			continue;
		}

		if (R_ProgramParameterMatrix4fv(&((r_program_t *) r_state.active_program)->matrix_uniforms[i],
		                                (const GLfloat *) active_matrices[i].m)) {
			any_changed = true;
		}
	}

	if (any_changed) {

		if (r_state.active_program->MatricesChanged) {
			r_state.active_program->MatricesChanged();
		}

		memset(((r_program_t *) r_state.active_program)->matrix_dirty, 0, sizeof(r_state.active_program->matrix_dirty));
	}
}

/**
 * @brief Uploads the alpha threshold to the currently loaded program.
 */
void R_UseAlphaTest(void) {

	if (r_state.active_program->UseAlphaTest) {
		r_state.active_program->UseAlphaTest(r_state.alpha_threshold);
	}
}

/**
 * @brief Uploads the current global color to the currently loaded program.
 */
void R_UseCurrentColor(void) {

	if (r_state.active_program->UseCurrentColor) {
		r_state.active_program->UseCurrentColor(r_state.current_color);
	}
}

/**
 * @brief Uploads the current fog data to the currently loaded program.
 */
void R_UseFog(void) {

	if (r_state.active_program->UseFog) {
		r_state.active_program->UseFog(&r_state.active_fog_parameters);
	}
}

/**
 * @brief Uploads the interpolation value to the currently loaded program.
 */
void R_UseInterpolation(const vec_t lerp) {

	if (r_state.active_program->UseInterpolation) {
		r_state.active_program->UseInterpolation(lerp);
	}
}

/**
 * @brief Change the rendering viewport.
 */
void R_SetViewport(GLint x, GLint y, GLsizei width, GLsizei height, _Bool force) {

	if (!force &&
	        r_state.current_viewport.x == x &&
	        r_state.current_viewport.y == y &&
	        r_state.current_viewport.w == width &&
	        r_state.current_viewport.h == height) {
		return;
	}

	r_state.current_viewport.x = x;
	r_state.current_viewport.y = y;
	r_state.current_viewport.w = width;
	r_state.current_viewport.h = height;

	glViewport(x, y, width, height);

	r_view.num_state_changes[R_STATE_VIEWPORT]++;
}

/**
 * @brief Prepare OpenGL for drawing the 3D scene. Update the view-port definition
 * and load our projection and model-view matrices.
 */
void R_Setup3D(void) {

	if (!r_context.context) {
		return;
	}

	const SDL_Rect *viewport = &r_view.viewport_3d;
	R_SetViewport(viewport->x, viewport->y, viewport->w, viewport->h, !!r_state.supersample_fbo);

	// copy projection matrix
	R_SetMatrix(R_MATRIX_PROJECTION, &r_view.matrix_base_3d);

	// setup the model-view matrix
	Matrix4x4_CreateIdentity(&r_view.matrix);

	Matrix4x4_ConcatRotate(&r_view.matrix, -90.0, 1.0, 0.0, 0.0);	 // put Z going up
	Matrix4x4_ConcatRotate(&r_view.matrix,  90.0, 0.0, 0.0, 1.0);	 // put Z going up

	Matrix4x4_ConcatRotate(&r_view.matrix, -r_view.angles[ROLL],  1.0, 0.0, 0.0);
	Matrix4x4_ConcatRotate(&r_view.matrix, -r_view.angles[PITCH], 0.0, 1.0, 0.0);
	Matrix4x4_ConcatRotate(&r_view.matrix, -r_view.angles[YAW],   0.0, 0.0, 1.0);

	Matrix4x4_ConcatTranslate(&r_view.matrix, -r_view.origin[0], -r_view.origin[1], -r_view.origin[2]);

	R_SetMatrix(R_MATRIX_MODELVIEW, &r_view.matrix);

	Matrix4x4_Invert_Simple(&r_view.inverse_matrix, &r_view.matrix);

	// bind default vertex array
	R_UnbindAttributeBuffer(R_ARRAY_POSITION);

	R_EnableBlend(false);

	R_EnableDepthTest(true);
}

/**
 * @brief Toggle the state of depth testing.
 */
void R_EnableDepthTest(_Bool enable) {

	if (r_state.depth_test_enabled == enable) {
		return;
	}

	r_state.depth_test_enabled = enable;

	if (enable) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}

	r_view.num_state_changes[R_STATE_ENABLE_DEPTH_TEST]++;
}

/**
 * @brief Set the range of depth testing.
 */
void R_DepthRange(GLdouble znear, GLdouble zfar) {

	if (r_state.depth_near == znear &&
	        r_state.depth_far == zfar) {
		return;
	}

	glDepthRange(znear, zfar);
	r_state.depth_near = znear;
	r_state.depth_far = zfar;

	r_view.num_state_changes[R_STATE_DEPTH_RANGE]++;
}

/**
 * @brief Shortcut to toggling texunits by ID, for cgame.
 */
void R_EnableTextureID(const r_texunit_id_t texunit_id, _Bool enable) {

	R_EnableTexture(&r_state.texunits[texunit_id], enable);
}

/**
 * @brief Set the current window scissor.
 */
void R_EnableScissor(const SDL_Rect *bounds) {

	if (!bounds) {
		if (!r_state.scissor_enabled) {
			return;
		}

		glDisable(GL_SCISSOR_TEST);
		r_state.scissor_enabled = false;
		r_view.num_state_changes[R_STATE_ENABLE_SCISSOR]++;

		return;
	}

	if (!r_state.scissor_enabled) {
		glEnable(GL_SCISSOR_TEST);
		r_state.scissor_enabled = true;
		r_view.num_state_changes[R_STATE_ENABLE_SCISSOR]++;
	}

	glScissor(bounds->x, bounds->y, bounds->w, bounds->h);
	r_view.num_state_changes[R_STATE_SCISSOR]++;
}

/**
 * @brief Prepare OpenGL for drawing the 2D overlay. Update the view-port definition
 * and reset the project and model-view matrices.
 */
void R_Setup2D(void) {

	if (r_state.supersample_fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	R_SetViewport(0, 0, r_context.width, r_context.height, !!r_state.supersample_fbo);

	R_SetMatrix(R_MATRIX_PROJECTION, &r_view.matrix_base_2d);

	R_SetMatrix(R_MATRIX_MODELVIEW, &matrix4x4_identity);

	// bind default vertex array
	R_UnbindAttributeBuffer(R_ARRAY_POSITION);

	R_EnableBlend(true);

	R_EnableDepthTest(false);
}

/**
 * @brief
 */
static void R_ShutdownSupersample(void) {

	if (r_state.supersample_texture) {
		glDeleteTextures(1, &r_state.supersample_texture);
		r_state.supersample_texture = 0;
	}

	if (r_state.supersample_depth) {
		glDeleteRenderbuffers(1, &r_state.supersample_depth);
		r_state.supersample_depth = 0;
	}

	if (r_state.supersample_fbo) {
		glDeleteFramebuffers(1, &r_state.supersample_fbo);
		r_state.supersample_fbo = 0;
	}
}

/**
 * @brief Initialize supersample-related stuff
 */
static void R_InitSupersample(void) {

	// set supersample size if available
	if (!r_supersample->value) {
		return;
	}

	if (!GLAD_GL_ARB_framebuffer_object) {

		Com_Warn("r_supersample set but GL_ARB_framebuffer_object is unavailable.\n");
		Cvar_Set("r_supersample", "0");
		return;
	}

	if (!GLAD_GL_EXT_packed_depth_stencil) {

		Com_Warn("r_supersample set but GL_EXT_packed_depth_stencil is unavailable.\n");
		Cvar_Set("r_supersample", "0");
		return;
	}

	r_context.render_width = (r_pixel_t) (r_context.width * r_supersample->value);
	r_context.render_height = (r_pixel_t) (r_context.height * r_supersample->value);

	if (r_context.render_width == r_context.width ||
	        r_context.render_height == r_context.height) {

		Com_Warn("r_supersample set but won't change anything.\n");
		Cvar_ForceSet("r_supersample", "0");
		return;
	}

	// sanity checks
	if (r_context.render_width == 0 ||
	        r_context.render_height == 0 ||
	        r_context.render_width > r_config.max_texture_size ||
	        r_context.render_height > r_config.max_texture_size) {

		Com_Warn("r_supersample is too low or too high.\n");
		Cvar_Set("r_supersample", "0");

		r_context.render_width = r_context.width;
		r_context.render_height = r_context.height;

		return;
	}

	// try to create the FBO backing textures
	glGenTextures(1, &r_state.supersample_texture);
	glBindTexture(GL_TEXTURE_2D, r_state.supersample_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_context.render_width, r_context.render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
	             NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glGenRenderbuffers(1, &r_state.supersample_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, r_state.supersample_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_EXT, r_context.render_width, r_context.render_height);

	// attempt to gracefully recover from errors
	if (glGetError() != GL_NO_ERROR) {

		Com_Warn("Couldn't create supersample textures.\n");
		Cvar_Set("r_supersample", "0");

		r_context.render_width = r_context.width;
		r_context.render_height = r_context.height;

		R_ShutdownSupersample();
		return;
	}

	glGenFramebuffers(1, &r_state.supersample_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, r_state.supersample_fbo);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_state.supersample_texture, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r_state.supersample_depth);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, r_state.supersample_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	// attempt to gracefully recover from errors
	if (glGetError() != GL_NO_ERROR ||
	        glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {

		Com_Warn("Couldn't create supersample framebuffer.\n");
		Cvar_Set("r_supersample", "0");

		r_context.render_width = r_context.width;
		r_context.render_height = r_context.height;

		R_ShutdownSupersample();
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	Com_Print("  Supersampling @ %dx%d..\n", r_context.render_width, r_context.render_height);
}

/**
 * @brief Initializes the OpenGL state cache and sets OpenGL state parameters to
 * appropriate defaults.
 */
void R_InitState(void) {

	r_get_error = Cvar_Add("r_get_error", "0", 0, NULL);

	// See if we have any errors before state initialization.
	R_GetError("Pre-init");

	memset(&r_state, 0, sizeof(r_state));

	r_state.buffers_list = g_hash_table_new(g_direct_hash, g_direct_equal);

	r_state.depth_mask_enabled = true;
	Vector4Set(r_state.current_color, 1.0, 1.0, 1.0, 1.0);

	// setup texture units
	for (int32_t i = 0; i < R_TEXUNIT_TOTAL; i++) {
		r_texunit_t *texunit = &r_state.texunits[i];

		if (i < r_config.max_texunits) {
			texunit->texture = GL_TEXTURE0 + i;
		}
	}

	R_UnbindAttributeBuffers();

	// default texture unit
	R_SelectTexture(texunit_diffuse);

	R_EnableTexture(texunit_diffuse, true);

	// polygon offset parameters
	R_PolygonOffset(-1.0, 1.0);

	// alpha blend parameters
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// set num lights
	r_state.max_active_lights = Clamp(r_max_lights->integer, 0, MAX_LIGHTS);

	// set default alpha threshold
	r_state.alpha_threshold = ALPHA_TEST_DISABLED_THRESHOLD;

	// set default stencil pass operation
	r_state.stencil_op_pass = GL_KEEP;

	// set default stencil func arguments
	r_state.stencil_func_mask = ~0;
	r_state.stencil_func_func = GL_ALWAYS;
	r_state.stencil_func_ref = 0;

	// default depth range
	r_state.depth_near = 0.0;
	r_state.depth_far = 1.0;

	r_state.array_buffers_dirty = R_ARRAY_MASK_ALL;

	R_InitSupersample();

	glEnable(GL_CULL_FACE);

	glFrontFace(GL_CW);

	R_GetError("Post-init");
}

static void R_ShutdownState_PrintBuffers(gpointer       key,
        gpointer       value,
        gpointer       user_data) {

	const r_buffer_t *buffer = (r_buffer_t *) value;

	Com_Warn("Buffer not freed (%u type, %zd bytes), allocated from %s\n",
	         buffer->type, buffer->size, buffer->func);
}

/**
 * @brief
 */
void R_ShutdownState(void) {

	R_ShutdownSupersample();

	if (r_state.buffers_total) {
		g_hash_table_foreach(r_state.buffers_list, R_ShutdownState_PrintBuffers, NULL);
	}

	g_hash_table_destroy(r_state.buffers_list);
	r_state.buffers_list = NULL;

	memset(&r_state, 0, sizeof(r_state));
}
