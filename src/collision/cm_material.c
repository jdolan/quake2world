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

#include <SDL2/SDL_opengl.h>
#include "cm_local.h"
#include "parse.h"

/**
 * @brief Free the memory for the specified material. This does not free the whole
 * list, so if this contains linked materials, be sure to rid them as well.
 */
void Cm_FreeMaterial(cm_material_t *material) {

	Mem_Free(material);
}

/**
 * @brief Frees the material list allocated by LoadMaterials. This doesn't
 * free the actual materials, so be sure they're safe!
 */
void Cm_FreeMaterialList(cm_material_t **materials) {

	g_free(materials);
}

/**
 * @brief
 */
static uint32_t Cm_ParseContents(const char *c) {
	uint32_t contents = 0;

	if (strstr(c, "window")) {
		contents |= CONTENTS_WINDOW;
	}

	if (strstr(c, "lava")) {
		contents |= CONTENTS_LAVA;
	}

	if (strstr(c, "slime")) {
		contents |= CONTENTS_SLIME;
	}

	if (strstr(c, "water")) {
		contents |= CONTENTS_WATER;
	}

	if (strstr(c, "mist")) {
		contents |= CONTENTS_MIST;
	}

	if (strstr(c, "detail")) {
		contents |= CONTENTS_DETAIL;
	}

	if (strstr(c, "ladder")) {
		contents |= CONTENTS_LADDER;
	}

	return contents;
}

/**
 * @brief
 */
static char *Cm_UnparseContents(uint32_t contents) {
	static char s[MAX_STRING_CHARS] = "";

	if (contents & CONTENTS_WINDOW) {
		g_strlcat(s, "window ", sizeof(s));
	}

	if (contents & CONTENTS_LAVA) {
		g_strlcat(s, "lava ", sizeof(s));
	}

	if (contents & CONTENTS_SLIME) {
		g_strlcat(s, "slime ", sizeof(s));
	}

	if (contents & CONTENTS_WATER) {
		g_strlcat(s, "water ", sizeof(s));
	}

	if (contents & CONTENTS_MIST) {
		g_strlcat(s, "mist ", sizeof(s));
	}

	if (contents & CONTENTS_DETAIL) {
		g_strlcat(s, "detail ", sizeof(s));
	}

	if (contents & CONTENTS_LADDER) {
		g_strlcat(s, "ladder ", sizeof(s));
	}

	return g_strchomp(s);
}

/**
 * @brief
 */
static uint32_t Cm_ParseSurface(const char *c) {

	int surface = 0;

	if (strstr(c, "light")) {
		surface |= SURF_LIGHT;
	}

	if (strstr(c, "slick")) {
		surface |= SURF_SLICK;
	}

	if (strstr(c, "sky")) {
		surface |= SURF_SKY;
	}

	if (strstr(c, "warp")) {
		surface |= SURF_WARP;
	}

	if (strstr(c, "blend_33")) {
		surface |= SURF_BLEND_33;
	}

	if (strstr(c, "blend_66")) {
		surface |= SURF_BLEND_66;
	}

	if (strstr(c, "no_draw")) {
		surface |= SURF_NO_DRAW;
	}

	if (strstr(c, "hint")) {
		surface |= SURF_HINT;
	}

	if (strstr(c, "skip")) {
		surface |= SURF_SKIP;
	}

	if (strstr(c, "alpha_test")) {
		surface |= SURF_ALPHA_TEST;
	}

	if (strstr(c, "phong")) {
		surface |= SURF_PHONG;
	}

	if (strstr(c, "material")) {
		surface |= SURF_MATERIAL;
	}

	return surface;
}

/**
 * @brief
 */
static char *Cm_UnparseSurface(uint32_t surface) {
	static char s[MAX_STRING_CHARS] = "";

	if (surface & SURF_LIGHT) {
		g_strlcat(s, "light ", sizeof(s));
	}

	if (surface & SURF_SLICK) {
		g_strlcat(s, "slick ", sizeof(s));
	}

	if (surface & SURF_SKY) {
		g_strlcat(s, "sky ", sizeof(s));
	}

	if (surface & SURF_WARP) {
		g_strlcat(s, "warp ", sizeof(s));
	}

	if (surface & SURF_BLEND_33) {
		g_strlcat(s, "blend_33 ", sizeof(s));
	}

	if (surface & SURF_BLEND_66) {
		g_strlcat(s, "blend_66 ", sizeof(s));
	}

	if (surface & SURF_NO_DRAW) {
		g_strlcat(s, "no_draw ", sizeof(s));
	}

	if (surface & SURF_HINT) {
		g_strlcat(s, "hint ", sizeof(s));
	}

	if (surface & SURF_SKIP) {
		g_strlcat(s, "skip ", sizeof(s));
	}

	if (surface & SURF_ALPHA_TEST) {
		g_strlcat(s, "alpha_test ", sizeof(s));
	}

	if (surface & SURF_PHONG) {
		g_strlcat(s, "phong ", sizeof(s));
	}

	if (surface & SURF_MATERIAL) {
		g_strlcat(s, "material ", sizeof(s));
	}

	return g_strchomp(s);
}

/**
 * @brief
 */
static inline GLenum Cm_BlendConstByName(const char *c) {

	if (!g_strcmp0(c, "GL_ONE")) {
		return GL_ONE;
	}
	if (!g_strcmp0(c, "GL_ZERO")) {
		return GL_ZERO;
	}
	if (!g_strcmp0(c, "GL_SRC_ALPHA")) {
		return GL_SRC_ALPHA;
	}
	if (!g_strcmp0(c, "GL_ONE_MINUS_SRC_ALPHA")) {
		return GL_ONE_MINUS_SRC_ALPHA;
	}
	if (!g_strcmp0(c, "GL_SRC_COLOR")) {
		return GL_SRC_COLOR;
	}
	if (!g_strcmp0(c, "GL_DST_COLOR")) {
		return GL_DST_COLOR;
	}
	if (!g_strcmp0(c, "GL_ONE_MINUS_SRC_COLOR")) {
		return GL_ONE_MINUS_SRC_COLOR;
	}

	// ...
	Com_Warn("Failed to resolve: %s\n", c);
	return GL_INVALID_ENUM;
}

/**
 * @brief
 */
static inline const char *Cm_BlendNameByConst(const GLenum c) {

	switch (c) {
		case GL_ONE:
			return "GL_ONE";
		case GL_ZERO:
			return "GL_ZERO";
		case GL_SRC_ALPHA:
			return "GL_SRC_ALPHA";
		case GL_ONE_MINUS_SRC_ALPHA:
			return "GL_ONE_MINUS_SRC_ALPHA";
		case GL_SRC_COLOR:
			return "GL_SRC_COLOR";
		case GL_DST_COLOR:
			return "GL_DST_COLOR";
		case GL_ONE_MINUS_SRC_COLOR:
			return "GL_ONE_MINUS_SRC_COLOR";
	}

	// ...
	Com_Warn("Failed to resolve: %u\n", c);
	return "GL_INVALID_ENUM";
}

/**
 * @brief
 */
static void Cm_MaterialWarn(const char *path, const parser_t *parser, const char *message) {
	Com_Warn("%s: Syntax error (Ln %u Col %u)\n", path, parser->position.row + 1, parser->position.col);

	if (message) {
		Com_Warn("  %s\n", message);
	}
}

/**
 * @brief
 */
static int32_t Cm_ParseStage(cm_material_t *m, cm_stage_t *s, parser_t *parser, const char *path) {
	char token[MAX_TOKEN_CHARS];

	while (true) {

		if (!Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "texture") || !g_strcmp0(token, "diffuse")) {
			
			if (!Parse_Token(parser, PARSE_NO_WRAP, s->image, sizeof(s->image))) {
				Cm_MaterialWarn(path, parser, "Missing path or too many characters");
				continue;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!g_strcmp0(token, "envmap")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->image, sizeof(s->image))) {
				Cm_MaterialWarn(path, parser, "Missing path or too many characters");
				continue;
			}

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &s->image_index, 1)) {
				Cm_MaterialWarn(path, parser, "Missing image index");
				continue;
			}

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if (!g_strcmp0(token, "blend")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, parser, "Missing blend src");
				continue;
			}

			s->blend.src = Cm_BlendConstByName(token);

			if (s->blend.src == GL_INVALID_ENUM) {
				Cm_MaterialWarn(path, parser, "Invalid blend src");
			}

			if (!Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, parser, "Missing blend dest");
				continue;
			}

			s->blend.dest = Cm_BlendConstByName(token);

			if (s->blend.dest == GL_INVALID_ENUM) {
				Cm_MaterialWarn(path, parser, "Invalid blend dest");
			}

			if (s->blend.src != GL_INVALID_ENUM && 
				s->blend.dest != GL_INVALID_ENUM) {
				s->flags |= STAGE_BLEND;
			}

			continue;
		}

		if (!g_strcmp0(token, "color")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, s->color, 3) != 3) {
				Cm_MaterialWarn(path, parser, "Need 3 values for color");
				continue;
			}

			for (int32_t i = 0; i < 3; i++) {

				if (s->color[i] < 0.0 || s->color[i] > 1.0) {
					Cm_MaterialWarn(path, parser, "Invalid value for color, must be between 0.0 and 1.0");
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (!g_strcmp0(token, "pulse")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->pulse.hz, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for pulse");
				continue;
			}

			if (s->pulse.hz == 0.0) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			} else {
				s->flags |= STAGE_PULSE;
			}

			continue;
		}

		if (!g_strcmp0(token, "stretch")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->stretch.amp, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for amplitude");
				continue;
			}

			if (s->stretch.amp == 0.0) {
				Cm_MaterialWarn(path, parser, "Amplitude must not be zero");
			}

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->stretch.hz, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for frequency");
				continue;
			}

			if (s->stretch.hz == 0.0) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			}

			if (s->stretch.amp != 0.0 &&
				s->stretch.hz != 0.0) {
				s->flags |= STAGE_STRETCH;
			}

			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->rotate.hz, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for rotate");
				continue;
			}

			if (s->rotate.hz == 0.0) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			} else {
				s->flags |= STAGE_ROTATE;
			}

			continue;
		}

		if (!g_strcmp0(token, "scroll.s")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scroll.s, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for scroll.s");
				continue;
			}

			if (s->scroll.s == 0.0) {
				Cm_MaterialWarn(path, parser, "scroll.s must not be zero");
			} else {
				s->flags |= STAGE_SCROLL_S;
			}

			continue;
		}

		if (!g_strcmp0(token, "scroll.t")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scroll.t, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for scroll.t");
				continue;
			}

			if (s->scroll.t == 0.0) {
				Cm_MaterialWarn(path, parser, "scroll.t must not be zero");
			} else {
				s->flags |= STAGE_SCROLL_T;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale.s")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scale.s, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for scale.s");
				continue;
			}

			if (s->scale.s == 0.0) {
				Cm_MaterialWarn(path, parser, "scale.s must not be zero");
			} else {
				s->flags |= STAGE_SCALE_S;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale.t")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scale.t, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for scale.t");
				continue;
			}

			if (s->scale.t == 0.0) {
				Cm_MaterialWarn(path, parser, "scale.t must not be zero");
			} else {
				s->flags |= STAGE_SCALE_T;
			}

			continue;
		}

		if (!g_strcmp0(token, "terrain")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->terrain.floor, 1) ||
				!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->terrain.ceil, 1)) {
				Cm_MaterialWarn(path, parser, "Need two values for terrain");
				continue;
			}

			if (s->terrain.ceil <= s->terrain.floor) {
				Cm_MaterialWarn(path, parser, "Terrain ceiling must be > the floor");
			} else {
				s->terrain.height = s->terrain.ceil - s->terrain.floor;
				s->flags |= STAGE_TERRAIN;
			}

			continue;
		}

		if (!g_strcmp0(token, "dirtmap")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->dirt.intensity, 1)) {
				Cm_MaterialWarn(path, parser, "Need a value for dirtmap");
				continue;
			}

			if (s->dirt.intensity <= 0.0 || s->dirt.intensity > 1.0) {
				Cm_MaterialWarn(path, parser, "Dirt intensity must be between 0.0 and 1.0");
			} else {
				s->flags |= STAGE_DIRTMAP;
			}

			continue;
		}

		if (!g_strcmp0(token, "anim")) {

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_UINT16, &s->anim.num_frames, 1)) {
				Cm_MaterialWarn(path, parser, "Need number of frames");
				continue;
			}

			if (s->anim.num_frames < 1) {
				Cm_MaterialWarn(path, parser, "Invalid number of frames");
			}

			if (!Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->anim.fps, 1)) {
				Cm_MaterialWarn(path, parser, "Need FPS value");
				continue;
			}

			if (s->anim.fps < 0.0) {
				Cm_MaterialWarn(path, parser, "Invalid FPS value, must be >= 0.0");
			}

			// the frame images are loaded once the stage is parsed completely
			if (s->anim.num_frames && s->anim.fps >= 0.0) {
				s->flags |= STAGE_ANIM;
			}

			continue;
		}

		if (!g_strcmp0(token, "lightmap")) {
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if (!g_strcmp0(token, "fog")) {
			s->flags |= STAGE_FOG;
			continue;
		}

		if (!g_strcmp0(token, "flare")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->image, sizeof(s->image))) {
				Cm_MaterialWarn(path, parser, "Missing flare image or ID");
				continue;
			}

			s->image_index = (int32_t) strtol(s->image, NULL, 0);
			s->flags |= STAGE_FLARE;
			continue;
		}

		if (!g_strcmp0(token, "mesh_color")) {
			s->mesh_color = true;
			continue;
		}

		if (*token == '}') {

			// a texture, or envmap means render it
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP)) {
				s->flags |= STAGE_DIFFUSE;

				// a terrain blend or dirtmap means light it
				if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
					s->flags |= STAGE_LIGHTING;
				}
			}

			// a blend dest other than GL_ONE should use fog by default
			if (s->blend.dest != GL_ONE) {
				s->flags |= STAGE_FOG;
			}

			Com_Debug(DEBUG_COLLISION,
			          "Parsed stage\n"
			          "  flags: %d\n"
			          "  texture: %s\n"
			          "   -> material: %s\n"
			          "  blend: %d %d\n"
			          "  color: %3f %3f %3f\n"
			          "  pulse: %3f\n"
			          "  stretch: %3f %3f\n"
			          "  rotate: %3f\n"
			          "  scroll.s: %3f\n"
			          "  scroll.t: %3f\n"
			          "  scale.s: %3f\n"
			          "  scale.t: %3f\n"
			          "  terrain.floor: %5f\n"
			          "  terrain.ceil: %5f\n"
			          "  anim.num_frames: %d\n"
			          "  anim.fps: %3f\n", s->flags, (*s->image ? s->image : "NULL"),
			          ((s->flags & STAGE_LIGHTING) ? "true" : "false"), s->blend.src,
			          s->blend.dest, s->color[0], s->color[1], s->color[2], s->pulse.hz,
			          s->stretch.amp, s->stretch.hz, s->rotate.hz, s->scroll.s, s->scroll.t,
			          s->scale.s, s->scale.t, s->terrain.floor, s->terrain.ceil, s->anim.num_frames,
			          s->anim.fps);

			return 0;
		}
	}

	Com_Warn("Malformed stage\n");
	return -1;
}

/**
 * @brief Normalizes a material's input name and fills the buffer with the base name.
 */
void Cm_NormalizeMaterialName(const char *in, char *out, size_t len) {

	if (out != in) {
		g_strlcpy(out, in, len);
	}

	if (g_str_has_suffix(out, "_d")) {
		out[strlen(out) - 2] = '\0';
	}
}

/**
 * @brief
 */
static void Cm_AttachStage(cm_material_t *m, cm_stage_t *s) {

	m->num_stages++;

	if (!m->stages) {
		m->stages = s;
		return;
	}

	cm_stage_t *ss = m->stages;
	while (ss->next) {
		ss = ss->next;
	}
	ss->next = s;
}

/**
 * @brief Allocates a material, setting up the diffuse stage.
 */
cm_material_t *Cm_AllocMaterial(const char *diffuse) {

	if (!diffuse || !diffuse[0]) {
		Com_Error(ERROR_DROP, "NULL diffuse name\n");
	}

	cm_material_t *mat = Mem_TagMalloc(sizeof(cm_material_t), MEM_TAG_MATERIAL);

	StripExtension(diffuse, mat->diffuse);
	Cm_NormalizeMaterialName(mat->diffuse, mat->base, sizeof(mat->base));

	mat->bump = DEFAULT_BUMP;
	mat->hardness = DEFAULT_HARDNESS;
	mat->parallax = DEFAULT_PARALLAX;
	mat->specular = DEFAULT_SPECULAR;

	return mat;
}

/**
 * @brief Loads all of the materials for the specified .mat file. This must be
 * a full, absolute path to a .mat file WITH extension. Returns a pointer to the
 * start of the list of materials loaded by this function.
 */
cm_material_t **Cm_LoadMaterials(const char *path, size_t *count) {
	void *buf;

	if (count) {
		*count = 0;
	}

	if (Fs_Load(path, &buf) == -1) {
		Com_Debug(DEBUG_COLLISION, "Couldn't load %s\n", path);
		return NULL;
	}

	char token[MAX_TOKEN_CHARS];
	cm_material_t *m = NULL;
	_Bool in_material = false, parsing_material = false;
	GArray *materials = g_array_new(false, false, sizeof(cm_material_t *));
	parser_t parser;

	Parse_Init(&parser, (const char *) buf, PARSER_C_LINE_COMMENTS | PARSER_C_BLOCK_COMMENTS);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (*token == '{' && !in_material) {
			in_material = true;
			continue;
		}

		if (!g_strcmp0(token, "material")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, MAX_QPATH)) {
				Cm_MaterialWarn(path, &parser, "Too many characters in path");
				break;
			}

			cm_material_t *mat;

			if (*token == '#') {
				mat = Cm_AllocMaterial(token + 1);
			} else {
				mat = Cm_AllocMaterial(va("textures/%s", token));
			}

			m = mat;
			parsing_material = true;
			continue;
		}

		if (!m && !parsing_material) {
			continue;
		}

		if (!g_strcmp0(token, "normalmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->normalmap, sizeof(m->normalmap))) {
				Cm_MaterialWarn(path, &parser, "Missing path or too many characters");
				continue;
			}
		}

		if (!g_strcmp0(token, "specularmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->specularmap, sizeof(m->specularmap))) {
				Cm_MaterialWarn(path, &parser, "Missing path or too many characters");
				continue;
			}
		}

		if (!g_strcmp0(token, "tintmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->tintmap, sizeof(m->tintmap))) {
				Cm_MaterialWarn(path, &parser, "Missing path or too many characters");
				continue;
			}
		}

		if (!strncmp(token, "tintmap.", strlen("tintmap."))) {
			vec_t *color = NULL;
			
			if (!g_strcmp0(token, "tintmap.shirt_default")) {
				color = m->tintmap_defaults[TINT_SHIRT];
			} else if (!g_strcmp0(token, "tintmap.pants_default")) {
				color = m->tintmap_defaults[TINT_PANTS];
			} else if (!g_strcmp0(token, "tintmap.head_default")) {
				color = m->tintmap_defaults[TINT_HEAD];
			} else {
				Cm_MaterialWarn(path, &parser, va("Invalid token \"%s\"", token));
			}

			if (color) {
				size_t num_parsed = Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, color, 4);

				if (num_parsed < 3 || num_parsed > 4) {
					Cm_MaterialWarn(path, &parser, "Too many numbers for color (must be 3 or 4)");
				} else {
					if (num_parsed != 4) {
						color[3] = 1.0;
					}

					for (size_t i = 0; i < num_parsed; i++) {
						if (color[i] < 0.0 || color[i] > 1.0) {
							Cm_MaterialWarn(path, &parser, "Color number out of range (must be between 0.0 and 1.0)");
						}
					}
				}
			}

			continue;
		}
		
		if (!g_strcmp0(token, "bump")) {

			if (!Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->bump, 1)) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->bump < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid bump value, must be > 0.0\n");
				m->bump = DEFAULT_BUMP;
			}
		}

		if (!g_strcmp0(token, "parallax")) {

			if (!Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->parallax, 1)) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->parallax < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid parallax value, must be > 0.0\n");
				m->parallax = DEFAULT_PARALLAX;
			}
		}

		if (!g_strcmp0(token, "hardness")) {

			if (!Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->hardness, 1)) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->hardness < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid hardness value, must be > 0.0\n");
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(token, "specular")) {

			if (!Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->specular, 1)) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->specular < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid specular value, must be > 0.0\n");
				m->specular = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(token, "contents")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, &parser, "No contents specified\n");
			} else {
				m->contents = Cm_ParseContents(token);
			}
		}

		if (!g_strcmp0(token, "surface")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, &parser, "No surface flags specified\n");
			} else {
				m->surface = Cm_ParseSurface(token);
			}
		}

		if (!g_strcmp0(token, "light")) {

			if (!Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->light, 1)) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->light < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid light value, must be > 0.0\n");
				m->light = DEFAULT_LIGHT;
			}
		}

		if (!g_strcmp0(token, "footsteps")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->footsteps, sizeof(m->footsteps))) {
				Cm_MaterialWarn(path, &parser, "Invalid footsteps value\n");
			}
		}

		if (!g_strcmp0(token, "stages_only")) {
			m->only_stages = true;
		}

		if (*token == '{' && in_material) {

			cm_stage_t *s = (cm_stage_t *) Mem_LinkMalloc(sizeof(*s), m);

			if (Cm_ParseStage(m, s, &parser, path) == -1) {
				Com_Debug(DEBUG_COLLISION, "Couldn't load a stage in %s\n", m->base);
				Mem_Free(s);
				continue;
			}

			// append the stage to the chain
			Cm_AttachStage(m, s);

			m->flags |= s->flags;
			continue;
		}

		if (*token == '}' && in_material) {
			g_array_append_val(materials, m);

			Com_Debug(DEBUG_COLLISION, "Parsed material %s with %d stages\n", m->diffuse, m->num_stages);
			in_material = false;
			parsing_material = false;

			if (count) {
				(*count)++;
			}
		}
	}

	Fs_Free(buf);
	return (cm_material_t **) g_array_free(materials, false);
}

/**
 * @brief Serialize the given stage.
 */
static void Cm_WriteStage(const cm_material_t *material, const cm_stage_t *stage, file_t *file) {
	Fs_Print(file, "\t{\n");

	if (stage->flags & STAGE_TEXTURE) {
		Fs_Print(file, "\t\ttexture %s\n", stage->image);
	} else if (stage->flags & STAGE_ENVMAP) {
		Fs_Print(file, "\t\tenvmap %s\n", stage->image);
	} else if (stage->flags & STAGE_FLARE) {
		Fs_Print(file, "\t\tflare %s\n", stage->image);
	} else {
		Com_Warn("Material %s has a stage with no texture?\n", material->diffuse);
	}

	if (stage->flags & STAGE_BLEND) {
		Fs_Print(file, "\t\tblend %s %s\n", Cm_BlendNameByConst(stage->blend.src), Cm_BlendNameByConst(stage->blend.dest));
	}

	if (stage->flags & STAGE_COLOR) {
		Fs_Print(file, "\t\tcolor %g %g %g\n", stage->color[0], stage->color[1], stage->color[2]);
	}

	if (stage->flags & STAGE_PULSE) {
		Fs_Print(file, "\t\tpulse %g\n", stage->pulse.hz);
	}

	if (stage->flags & STAGE_STRETCH) {
		Fs_Print(file, "\t\tstretch %g %g\n", stage->stretch.amp, stage->stretch.hz);
	}

	if (stage->flags & STAGE_ROTATE) {
		Fs_Print(file, "\t\trotate %g\n", stage->rotate.hz);
	}

	if (stage->flags & STAGE_SCROLL_S) {
		Fs_Print(file, "\t\tscroll.s %g\n", stage->scroll.s);
	}

	if (stage->flags & STAGE_SCROLL_T) {
		Fs_Print(file, "\t\tscroll.t %g\n", stage->scroll.t);
	}

	if (stage->flags & STAGE_SCALE_S) {
		Fs_Print(file, "\t\tscale.s %g\n", stage->scale.s);
	}

	if (stage->flags & STAGE_SCALE_T) {
		Fs_Print(file, "\t\tscale.t %g\n", stage->scale.t);
	}

	if (stage->flags & STAGE_TERRAIN) {
		Fs_Print(file, "\t\tterrain %g %g\n", stage->terrain.floor, stage->terrain.ceil);
	}

	if (stage->flags & STAGE_DIRTMAP) {
		Fs_Print(file, "\t\tdirtmap %g\n", stage->dirt.intensity);
	}

	if (stage->flags & STAGE_ANIM) {
		Fs_Print(file, "\t\tanim %u %g\n", stage->anim.num_frames, stage->anim.fps);
	}

	if (stage->flags & STAGE_LIGHTMAP) {
		Fs_Print(file, "\t\tlightmap\n");
	}

	Fs_Print(file, "\t}\n");
}

/**
 * @return The material name as it should appear in a materials file.
 */
static const char *Cm_MaterialName(const char *texture) {

	if (g_str_has_prefix(texture, "textures/")) {
		return texture + strlen("textures/");
	} else {
		return va("#%s", texture);
	}
}

/**
 * @brief Serialize the given material.
 */
static void Cm_WriteMaterial(const cm_material_t *material, file_t *file) {
	Fs_Print(file, "{\n");

	// write the innards
	Fs_Print(file, "\tmaterial %s\n", Cm_MaterialName(material->diffuse));

	if (*material->normalmap) {
		Fs_Print(file, "\tnormalmap %s\n", material->normalmap);
	}
	if (*material->specularmap) {
		Fs_Print(file, "\tspecularmap %s\n", material->specularmap);
	}

	Fs_Print(file, "\tbump %g\n", material->bump);
	Fs_Print(file, "\thardness %g\n", material->hardness);
	Fs_Print(file, "\tspecular %g\n", material->specular);
	Fs_Print(file, "\tparallax %g\n", material->parallax);

	if (material->contents) {
		Fs_Print(file, "\tcontents \"%s\"\n", Cm_UnparseContents(material->contents));
	}

	if (material->surface) {
		Fs_Print(file, "\tsurface \"%s\"\n", Cm_UnparseSurface(material->surface));
	}

	if (material->light) {
		Fs_Print(file, "\tlight %g\n", material->light);
	}

	// if not empty/default, write footsteps
	if (*material->footsteps && g_strcmp0(material->footsteps, "default")) {
		Fs_Print(file, "\tfootsteps %s\n", material->footsteps);
	}

	// write stages
	for (cm_stage_t *stage = material->stages; stage; stage = stage->next) {
		Cm_WriteStage(material, stage, file);
	}

	Fs_Print(file, "}\n");
}

/**
 * @brief Serialize the material(s) into the specified file.
 */
void Cm_WriteMaterials(const char *filename, const cm_material_t **materials, const size_t num_materials) {

	file_t *file = Fs_OpenWrite(filename);
	if (file) {
		for (size_t i = 0; i < num_materials; i++) {
			Cm_WriteMaterial(materials[i], file);
		}

		Fs_Close(file);
	} else {
		Com_Warn("Failed to open %s for write\n", filename);
	}
}
