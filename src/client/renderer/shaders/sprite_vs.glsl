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

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_diffusemap;
layout (location = 2) in vec4 in_color;
layout (location = 3) in vec2 in_next_diffusemap;
layout (location = 4) in float in_next_lerp;

uniform mat4 projection;
uniform mat4 view;

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec4 color;
	vec2 next_diffusemap;
	float next_lerp;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * view * vec4(in_position.xyz, 1.0);

	vertex.position = vec3(view * vec4(in_position.xyz, 1.0));
	vertex.diffusemap = in_diffusemap;
	vertex.color = in_color;
	vertex.next_diffusemap = in_next_diffusemap;
	vertex.next_lerp = in_next_lerp;
}