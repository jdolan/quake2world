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

#define MAX_LIGHTS 64

struct light {
	vec4 origin;
	vec4 color;
};

layout (std140) uniform lights_block {
	light lights[MAX_LIGHTS];
};

/**
 * @brief
 */
vec3 dynamic_light(in vec3 position) {

	vec3 diffuse = vec3(0);

	for (int i = 0; i < MAX_LIGHTS; i++) {

		if (lights[i].origin.w == 0.0) {
			break;
		}

		float dist = distance(lights[i].origin.xyz, position);
		if (dist < lights[i].origin.w) {
			diffuse += lights[i].color.rgb;
		}
	}

	return diffuse;
}
