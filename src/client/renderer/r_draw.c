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
#include "client.h"

#define MAX_CHARS MAX_GL_ARRAY_LENGTH  // per font
#define MAX_CHAR_VERTS MAX_CHARS * 4
#define MAX_CHAR_ELEMENTS MAX_CHARS * 6

// characters are batched per frame and drawn in one shot
// accumulate coordinates and colors as vertex arrays
typedef struct r_char_arrays_s {
	vec3_t verts[MAX_CHAR_VERTS];
	uint32_t vert_index;
	r_buffer_t vert_buffer;

	vec2_t texcoords[MAX_CHAR_VERTS];
	r_buffer_t texcoord_buffer;

	u8vec4_t colors[MAX_CHAR_VERTS];
	r_buffer_t color_buffer;

	GLuint elements[MAX_CHAR_ELEMENTS];
	uint32_t element_index;
	r_buffer_t element_buffer;

	uint32_t num_chars;
} r_char_arrays_t;

#define MAX_FILLS 512
#define MAX_FILL_VERTS MAX_FILLS * 4
#define MAX_FILL_ELEMENTS MAX_FILLS * 6

// fills (alpha-blended quads) are also batched per frame
typedef struct r_fill_arrays_s {
	vec3_t verts[MAX_FILL_VERTS];
	uint32_t vert_index;
	r_buffer_t vert_buffer;

	u8vec4_t colors[MAX_FILL_VERTS];
	r_buffer_t color_buffer;

	GLuint elements[MAX_FILL_ELEMENTS];
	uint32_t element_index;
	r_buffer_t element_buffer;

	uint32_t num_fills;

	// buffer used for immediately-rendered fills
	r_buffer_t ui_vert_buffer;
} r_fill_arrays_t;

#define MAX_LINES 512
#define MAX_LINE_VERTS MAX_LINES * 4

// lines are batched per frame too
typedef struct r_line_arrays_s {
	vec3_t verts[MAX_LINE_VERTS];
	uint32_t vert_index;
	r_buffer_t vert_buffer;

	u8vec4_t colors[MAX_LINE_VERTS];
	r_buffer_t color_buffer;

	// buffer used for immediately-rendered lines
	r_buffer_t ui_vert_buffer;
} r_line_arrays_t;

// each font has vertex arrays of characters to draw each frame
typedef struct r_font_s {
	char name[MAX_QPATH];

	r_image_t *image;

	r_pixel_t char_width;
	r_pixel_t char_height;
} r_font_t;

#define MAX_FONTS 3

// pull it all together in one structure
typedef struct r_draw_s {

	// registered fonts
	uint16_t num_fonts;
	r_font_t fonts[MAX_FONTS];

	// active font
	r_font_t *font;

	// actual text colors as ABGR uint32_tegers
	uint32_t colors[MAX_COLORS];

	r_char_arrays_t char_arrays[MAX_FONTS];
	r_fill_arrays_t fill_arrays;
	r_line_arrays_t line_arrays;
} r_draw_t;

r_draw_t r_draw;

/**
 * @return An `r_color_t` from the given parameters.
 */
r_color_t R_MakeColor(byte r, byte g, byte b, byte a) {

	r_color_t color = {
		.bytes = {
			.r = r,
			.g = g,
			.b = b,
			.a = a,
		}
	};

	return color;
}

/**
 * @brief Make a quad by passing index to first vertex and last element ID.
 */
void R_MakeQuadU32(uint32_t *indices, const uint32_t vertex_id) {
	*(indices)++ = vertex_id + 0;
	*(indices)++ = vertex_id + 1;
	*(indices)++ = vertex_id + 2;

	*(indices)++ = vertex_id + 0;
	*(indices)++ = vertex_id + 2;
	*(indices)++ = vertex_id + 3;
}

/**
 * @brief
 */
void R_DrawImage(r_pixel_t x, r_pixel_t y, vec_t scale, const r_image_t *image) {

	R_BindTexture(image->texnum);

	// our texcoords are already setup, just set verts and draw

	VectorSet(r_state.vertex_array[0], x, y, 0);
	VectorSet(r_state.vertex_array[1], x + image->width * scale, y, 0);
	VectorSet(r_state.vertex_array[2], x + image->width * scale, y + image->height * scale, 0);
	VectorSet(r_state.vertex_array[3], x, y + image->height * scale, 0);

	R_UploadToBuffer(&r_state.buffer_vertex_array, 4 * sizeof(vec3_t), r_state.vertex_array);

	R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/**
 * @brief
 */
void R_DrawImageResized(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const r_image_t *image) {

	R_BindTexture(image->texnum);

	// our texcoords are already setup, just set verts and draw

	VectorSet(r_state.vertex_array[0], x, y, 0);
	VectorSet(r_state.vertex_array[1], x + w, y, 0);
	VectorSet(r_state.vertex_array[2], x + w, y + h, 0);
	VectorSet(r_state.vertex_array[3], x, y + h, 0);

	R_UploadToBuffer(&r_state.buffer_vertex_array, 4 * sizeof(vec3_t), r_state.vertex_array);

	R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/**
 * @brief Binds the specified font, returning the character width and height.
 */
void R_BindFont(const char *name, r_pixel_t *cw, r_pixel_t *ch) {

	if (name == NULL) {
		name = "medium";
	}

	uint16_t i;
	for (i = 0; i < r_draw.num_fonts; i++) {
		if (!g_strcmp0(name, r_draw.fonts[i].name)) {
			if (r_context.high_dpi && i < r_draw.num_fonts - 1) {
				r_draw.font = &r_draw.fonts[i + 1];
			} else {
				r_draw.font = &r_draw.fonts[i];
			}
			break;
		}
	}

	if (i == r_draw.num_fonts) {
		Com_Warn("Unknown font: %s\n", name);
	}

	if (cw) {
		*cw = r_draw.font->char_width;
	}

	if (ch) {
		*ch = r_draw.font->char_height;
	}
}

/**
 * @brief Return the width of the specified string in pixels. This will vary based
 * on the currently bound font. Color escapes are omitted.
 */
r_pixel_t R_StringWidth(const char *s) {
	return StrColorLen(s) * r_draw.font->char_width;
}

/**
 * @brief
 */
size_t R_DrawString(r_pixel_t x, r_pixel_t y, const char *s, int32_t color) {
	return R_DrawSizedString(x, y, s, UINT16_MAX, UINT16_MAX, color);
}

/**
 * @brief
 */
size_t R_DrawBytes(r_pixel_t x, r_pixel_t y, const char *s, size_t size, int32_t color) {
	return R_DrawSizedString(x, y, s, size, size, color);
}

/**
 * @brief Draws at most len chars or size bytes of the specified string. Color escape
 * sequences are not visible chars. Returns the number of chars drawn.
 */
size_t R_DrawSizedString(r_pixel_t x, r_pixel_t y, const char *s, size_t len, size_t size,
                         int32_t color) {
	size_t i, j;

	i = j = 0;
	while (*s && i < len && j < size) {

		if (IS_COLOR(s)) { // color escapes
			color = *(s + 1) - '0';
			j += 2;
			s += 2;
			continue;
		}

		if (IS_LEGACY_COLOR(s)) { // legacy colors
			color = CON_COLOR_ALT;
			j++;
			s++;
			continue;
		}

		R_DrawChar(x, y, *s, color);
		x += r_draw.font->char_width; // next char position in line

		i++;
		j++;
		s++;
	}

	return i;
}

/**
 * @brief
 */
void R_DrawChar(r_pixel_t x, r_pixel_t y, char c, int32_t color) {

	if (x > r_context.width || y > r_context.height) {
		return;
	}

	if (c == ' ') {
		return;    // small optimization for space
	}

	r_char_arrays_t *chars = &r_draw.char_arrays[r_draw.font - r_draw.fonts];

	const uint32_t row = (uint32_t) c >> 4;
	const uint32_t col = (uint32_t) c & 15;

	const vec_t frow = row * 0.1250;
	const vec_t fcol = col * 0.0625;

	// resolve ABGR color
	const uint32_t *abgr = &r_draw.colors[color & (MAX_COLORS - 1)];

	// copy to all 4 verts
	for (uint32_t i = 0; i < 4; ++i) {
		memcpy(&chars->colors[chars->vert_index + i], abgr, sizeof(u8vec4_t));
	}

	Vector2Set(chars->texcoords[chars->vert_index], fcol, frow);
	Vector2Set(chars->texcoords[chars->vert_index + 1], fcol + 0.0625, frow);
	Vector2Set(chars->texcoords[chars->vert_index + 2], fcol + 0.0625, frow + 0.1250);
	Vector2Set(chars->texcoords[chars->vert_index + 3], fcol, frow + 0.1250);

	VectorSet(chars->verts[chars->vert_index], x, y, 0);
	VectorSet(chars->verts[chars->vert_index + 1], x + r_draw.font->char_width, y, 0);
	VectorSet(chars->verts[chars->vert_index + 2], x + r_draw.font->char_width, y + r_draw.font->char_height, 0);
	VectorSet(chars->verts[chars->vert_index + 3], x, y + r_draw.font->char_height, 0);

	chars->vert_index += 4;

	const GLuint char_index = chars->num_chars * 4;

	R_MakeQuadU32(&chars->elements[chars->element_index], char_index);
	chars->element_index += 6;

	chars->num_chars++;
}

/**
 * @brief
 */
static void R_DrawChars(void) {

	for (uint16_t i = 0; i < r_draw.num_fonts; i++) {
		r_char_arrays_t *chars = &r_draw.char_arrays[i];

		if (!chars->vert_index) {
			continue;
		}

		R_UploadToBuffer(&r_draw.char_arrays[i].vert_buffer, r_draw.char_arrays[i].vert_index * sizeof(vec3_t),
		                 r_draw.char_arrays[i].verts);
		R_UploadToBuffer(&r_draw.char_arrays[i].texcoord_buffer, r_draw.char_arrays[i].vert_index * sizeof(vec2_t),
		                 r_draw.char_arrays[i].texcoords);
		R_UploadToBuffer(&r_draw.char_arrays[i].color_buffer, r_draw.char_arrays[i].vert_index * sizeof(u8vec4_t),
		                 r_draw.char_arrays[i].colors);

		R_UploadToBuffer(&r_draw.char_arrays[i].element_buffer, r_draw.char_arrays[i].element_index * sizeof(GLuint),
		                 r_draw.char_arrays[i].elements);

		R_BindTexture(r_draw.fonts[i].image->texnum);

		R_EnableColorArray(true);

		// alter the array pointers
		R_BindArray(R_ARRAY_COLOR, &chars->color_buffer);
		R_BindArray(R_ARRAY_TEX_DIFFUSE, &chars->texcoord_buffer);
		R_BindArray(R_ARRAY_VERTEX, &chars->vert_buffer);

		R_BindArray(R_ARRAY_ELEMENTS, &chars->element_buffer);

		R_DrawArrays(GL_TRIANGLES, 0, chars->element_index);

		chars->vert_index = 0;
		chars->element_index = 0;
		chars->num_chars = 0;
	}

	// restore array pointers
	R_BindDefaultArray(R_ARRAY_COLOR);
	R_BindDefaultArray(R_ARRAY_TEX_DIFFUSE);
	R_BindDefaultArray(R_ARRAY_VERTEX);

	R_BindDefaultArray(R_ARRAY_ELEMENTS);

	R_EnableColorArray(false);

	// restore draw color
	R_Color(NULL);
}

/**
 * @brief The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_DrawFill(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int32_t c, vec_t a) {

	if (a > 1.0) {
		Com_Warn("Bad alpha %f\n", a);
		return;
	}

	if (a >= 0.0) { // palette index
		c = img_palette[c] & 0x00FFFFFF;
		c |= (((u8vec_t) (a * 255.0)) & 0xFF) << 24;
	}

	// duplicate color data to all 4 verts
	for (uint32_t i = 0; i < 4; ++i) {
		memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.vert_index + i], &c, sizeof(u8vec4_t));
	}

	// populate verts
	VectorSet(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index], x + 0.5, y + 0.5, 0);
	VectorSet(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 1], (x + w) + 0.5, y + 0.5, 0);
	VectorSet(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 2], (x + w) + 0.5, (y + h) + 0.5, 0);
	VectorSet(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 3], x + 0.5, (y + h) + 0.5, 0);

	r_draw.fill_arrays.vert_index += 4;

	const GLuint fill_index = r_draw.fill_arrays.num_fills * 4;

	R_MakeQuadU32(&r_draw.fill_arrays.elements[r_draw.fill_arrays.element_index], fill_index);
	r_draw.fill_arrays.element_index += 6;

	r_draw.fill_arrays.num_fills++;
}

/**
 * @brief
 */
static void R_DrawFills(void) {

	if (!r_draw.fill_arrays.vert_index) {
		return;
	}

	// upload the changed data
	R_UploadToBuffer(&r_draw.fill_arrays.vert_buffer, r_draw.fill_arrays.vert_index * sizeof(vec3_t),
	                 r_draw.fill_arrays.verts);
	R_UploadToBuffer(&r_draw.fill_arrays.color_buffer, r_draw.fill_arrays.vert_index * sizeof(u8vec4_t),
	                 r_draw.fill_arrays.colors);

	R_UploadToBuffer(&r_draw.fill_arrays.element_buffer, r_draw.fill_arrays.element_index * sizeof(GLuint),
	                 r_draw.fill_arrays.elements);

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(R_ARRAY_VERTEX, &r_draw.fill_arrays.vert_buffer);
	R_BindArray(R_ARRAY_COLOR, &r_draw.fill_arrays.color_buffer);
	R_BindArray(R_ARRAY_ELEMENTS, &r_draw.fill_arrays.element_buffer);

	R_DrawArrays(GL_TRIANGLES, 0, r_draw.fill_arrays.element_index);

	// and restore them
	R_BindDefaultArray(R_ARRAY_VERTEX);
	R_BindDefaultArray(R_ARRAY_COLOR);
	R_BindDefaultArray(R_ARRAY_ELEMENTS);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	r_draw.fill_arrays.vert_index = r_draw.fill_arrays.element_index = r_draw.fill_arrays.num_fills = 0;
}

/**
 * @brief
 */
void R_DrawLine(r_pixel_t x1, r_pixel_t y1, r_pixel_t x2, r_pixel_t y2, int32_t c, vec_t a) {

	if (a > 1.0) {
		Com_Warn("Bad alpha %f\n", a);
		return;
	}

	if (a >= 0.0) { // palette index
		c = img_palette[c] & 0x00FFFFFF;
		c |= (((u8vec_t) (a * 255.0)) & 0xFF) << 24;
	}

	// duplicate color data to 2 verts
	for (uint32_t i = 0; i < 4; ++i) {
		memcpy(&r_draw.line_arrays.colors[r_draw.line_arrays.vert_index + i], &c, sizeof(u8vec4_t));
	}

	// populate verts
	VectorSet(r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 0], x1 + 0.5, y1 + 0.5, 0.0);
	VectorSet(r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 1], x2 + 0.5, y2 + 0.5, 0.0);

	r_draw.line_arrays.vert_index += 2;
}

/**
 * @brief
 */
static void R_DrawLines(void) {

	if (!r_draw.line_arrays.vert_index) {
		return;
	}

	// upload the changed data
	R_UploadToBuffer(&r_draw.line_arrays.vert_buffer, r_draw.line_arrays.vert_index * sizeof(vec3_t),
	                 r_draw.line_arrays.verts);
	R_UploadToBuffer(&r_draw.line_arrays.color_buffer, r_draw.line_arrays.vert_index * sizeof(u8vec4_t),
	                 r_draw.line_arrays.colors);

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(R_ARRAY_VERTEX, &r_draw.line_arrays.vert_buffer);
	R_BindArray(R_ARRAY_COLOR, &r_draw.line_arrays.color_buffer);

	R_DrawArrays(GL_LINES, 0, r_draw.line_arrays.vert_index);

	// and restore them
	R_BindDefaultArray(R_ARRAY_VERTEX);
	R_BindDefaultArray(R_ARRAY_COLOR);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	r_draw.line_arrays.vert_index = 0;
}

/**
 * @brief Draws a filled rect, for MVC. Uses current color.
 */
void R_DrawFillUI(const SDL_Rect *rect) {

	const vec3_t verts[] = {
		{ rect->x - 1.0, rect->y - 1.0, 0.0 },
		{ rect->x + rect->w + 1.0, rect->y - 1.0, 0.0 },
		{ rect->x + rect->w + 1.0, rect->y + rect->h + 1.0, 0.0 },
		{ rect->x - 1.0, rect->y + rect->h + 1.0, 0.0 }
	};

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, false);

	// upload the changed data
	R_UploadToBuffer(&r_draw.fill_arrays.ui_vert_buffer, sizeof(verts), verts);

	// alter the array pointers
	R_BindArray(R_ARRAY_VERTEX, &r_draw.fill_arrays.ui_vert_buffer);

	// draw!
	R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// and restore them
	R_BindDefaultArray(R_ARRAY_VERTEX);

	R_EnableTexture(&texunit_diffuse, true);
}

void R_DrawLinesUI(const SDL_Point *points, const size_t count, const _Bool loop) {

	vec_t point_buffer[count * 3];

	for (size_t i = 0; i < count; ++i) {
		const size_t px = i * 3;

		point_buffer[px + 0] = points[i].x + 0.5;
		point_buffer[px + 1] = points[i].y + 0.5;
		point_buffer[px + 2] = 0.0;
	}

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, false);

	// upload the changed data
	R_UploadToBuffer(&r_draw.line_arrays.ui_vert_buffer, sizeof(point_buffer), point_buffer);

	// alter the array pointers
	R_BindArray(R_ARRAY_VERTEX, &r_draw.line_arrays.ui_vert_buffer);

	// draw!
	R_DrawArrays(loop ? GL_LINE_LOOP : GL_LINE_STRIP, 0, (GLsizei) count);

	// and restore them
	R_BindDefaultArray(R_ARRAY_VERTEX);

	R_EnableTexture(&texunit_diffuse, true);
}

/**
 * @brief Draw all 2D geometry accumulated for the current frame.
 */
void R_Draw2D(void) {

	R_DrawLines();

	R_DrawFills();

	R_DrawChars();
}

/**
 * @brief Initializes the specified bitmap font. The layout of the font is square,
 * 2^n (e.g. 256x256, 512x512), and 8 rows by 16 columns. See below:
 *
 *
 *  !"#$%&'()*+,-./
 * 0123456789:;<=>?
 * @ABCDEFGHIJKLMNO
 * PQRSTUVWXYZ[\]^_
 * 'abcdefghijklmno
 * pqrstuvwxyz{|}"
 */
static void R_InitFont(char *name) {

	if (r_draw.num_fonts == MAX_FONTS) {
		Com_Error(ERR_DROP, "MAX_FONTS reached\n");
		return;
	}

	r_font_t *font = &r_draw.fonts[r_draw.num_fonts++];

	g_strlcpy(font->name, name, sizeof(font->name));

	font->image = R_LoadImage(va("fonts/%s", name), IT_FONT);

	font->char_width = font->image->width / 16;
	font->char_height = font->image->height / 8;

	Com_Debug("%s (%dx%d)\n", font->name, font->char_width, font->char_height);
}

/**
 * @brief
 */
void R_InitDraw(void) {

	memset(&r_draw, 0, sizeof(r_draw));

	R_InitFont("small");
	R_InitFont("medium");
	R_InitFont("large");

	R_BindFont(NULL, NULL, NULL);

	// set ABGR color values
	r_draw.colors[CON_COLOR_BLACK] = 0xff000000;
	r_draw.colors[CON_COLOR_RED] = 0xff0000ff;
	r_draw.colors[CON_COLOR_GREEN] = 0xff00ff00;
	r_draw.colors[CON_COLOR_YELLOW] = 0xff00ffff;
	r_draw.colors[CON_COLOR_BLUE] = 0xffff0000;
	r_draw.colors[CON_COLOR_CYAN] = 0xffffff00;
	r_draw.colors[CON_COLOR_MAGENTA] = 0xffff00ff;
	r_draw.colors[CON_COLOR_WHITE] = 0xffffffff;

	for (int32_t i = 0; i < MAX_FONTS; ++i) {

		R_CreateBuffer(&r_draw.char_arrays[i].vert_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_draw.char_arrays[i].verts),
		               NULL);
		R_CreateBuffer(&r_draw.char_arrays[i].texcoord_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA,
		               sizeof(r_draw.char_arrays[i].texcoords), NULL);
		R_CreateBuffer(&r_draw.char_arrays[i].color_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA,
		               sizeof(r_draw.char_arrays[i].colors), NULL);

		R_CreateBuffer(&r_draw.char_arrays[i].element_buffer, GL_DYNAMIC_DRAW, R_BUFFER_ELEMENT,
		               sizeof(r_draw.char_arrays[i].elements), NULL);
	}

	R_CreateBuffer(&r_draw.fill_arrays.vert_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_draw.fill_arrays.verts), NULL);
	R_CreateBuffer(&r_draw.fill_arrays.color_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_draw.fill_arrays.colors),
	               NULL);

	R_CreateBuffer(&r_draw.fill_arrays.element_buffer, GL_DYNAMIC_DRAW, R_BUFFER_ELEMENT,
	               sizeof(r_draw.fill_arrays.elements), NULL);

	R_CreateBuffer(&r_draw.line_arrays.vert_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_draw.line_arrays.verts), NULL);
	R_CreateBuffer(&r_draw.line_arrays.color_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_draw.line_arrays.colors),
	               NULL);

	// fill buffer only needs 4 verts
	R_CreateBuffer(&r_draw.fill_arrays.ui_vert_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(vec3_t) * 4, NULL);
	R_CreateBuffer(&r_draw.line_arrays.ui_vert_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(vec3_t) * MAX_LINE_VERTS,
	               NULL);
}

/**
 * @brief
 */
void R_ShutdownDraw(void) {

	for (int32_t i = 0; i < MAX_FONTS; ++i) {

		R_DestroyBuffer(&r_draw.char_arrays[i].vert_buffer);
		R_DestroyBuffer(&r_draw.char_arrays[i].texcoord_buffer);
		R_DestroyBuffer(&r_draw.char_arrays[i].color_buffer);
	}

	R_DestroyBuffer(&r_draw.fill_arrays.vert_buffer);
	R_DestroyBuffer(&r_draw.fill_arrays.color_buffer);

	R_DestroyBuffer(&r_draw.line_arrays.vert_buffer);
	R_DestroyBuffer(&r_draw.line_arrays.color_buffer);

	R_DestroyBuffer(&r_draw.fill_arrays.ui_vert_buffer);
	R_DestroyBuffer(&r_draw.line_arrays.ui_vert_buffer);
}
