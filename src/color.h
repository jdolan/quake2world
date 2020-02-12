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

#pragma once

#include "vector.h"

/**
 * @brief Color constants.
 */
#define color_black     color3bv(0x000000)
#define color_red       color3bv(0xff0000)
#define color_green     color3bv(0x00ff00)
#define color_yellow    color3bv(0xffff00)
#define color_blue      color3bv(0x0000ff)
#define color_magenta   color3bv(0xff00ff)
#define color_cyan      color3bv(0x00ffff)
#define color_white     color3bv(0xffffff)

/**
 * @brief A 32-bit RGBA color
 */
typedef union {
	/**
	 * @brief Component accessors.
	 */
	struct {
		byte r, g, b, a;
	};

	/**
	 * @brief Array accessor.
	 */
	byte bytes[4];

	/**
	 * @brief Integer accessor.
	 */
	uint32_t rgba;
} color_t;

/**
 * @return A color with the specified RGB byte values.
 */
color_t color3b(byte r, byte g, byte b);

/**
 * @return A color with the specified RGB integer, e.g. `0xbbaadd`.
 */
color_t color3bv(uint32_t rgb);

/**
 * @return A color with the specified RGB floating point values.
 */
color_t color3f(float r, float g, float b);

/**
 * @return A color with the specified RGB vector.
 */
color_t color3fv(const vec3_t rgb);

/**
 * @return A color with the specified RGBA bytes.
 */
color_t color4b(byte r, byte g, byte b, byte a);

/**
 * @return A color with the specified RGBA integer, e.g. `0xdeadbeef`.
 */
color_t color4bv(uint32_t rgba);

/**
 * @return A color with the specified RGBA floating point values.
 */
color_t color4f(float r, float g, float b, float a);

/**
 * @return A color with the specified RGBA vector.
 */
color_t color4fv(const vec4_t rgba);

/**
 * @return A color for the specified escape sequence.
 */
color_t color_esc(int32_t esc);

/**
 * @brief Parses a color from a hexadecimal string, e.g. `"deadbeef"`.
 * @return True if a color was parsed, false otherwise.
 */
_Bool color_parse(const char *s, color_t *color);

/**
 * @return The hexadecimal string for the specified color.
 */
const char *color_unparse(const color_t color);

/**
 * @return A floating point vector for the specified color;
 */
vec3_t color_to_vec3(const color_t color);

/**
 * @return A floating point vector for the specified color.
 */
vec4_t color_to_vec4(const color_t color);