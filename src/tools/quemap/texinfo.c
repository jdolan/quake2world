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

#include "bsp.h"
#include "map.h"
#include "texinfo.h"

/**
 * @brief
 */
static void TextureAxisFromPlane(const plane_t *plane, vec3_t *xv, vec3_t *yv) {
	static const vec3_t base_axis[18] = { // base texture axis
		{ {  0,  0,  1 } },
		{ {  1,  0,  0 } },
		{ {  0, -1,  0 } }, // floor
		{ {  0,  0, -1 } },
		{ {  1,  0,  0 } },
		{ {  0, -1,  0 } }, // ceiling
		{ {  1,  0,  0 } },
		{ {  0,  1,  0 } },
		{ {  0,  0, -1 } }, // west wall
		{ { -1,  0,  0 } },
		{ {  0,  1,  0 } },
		{ {  0,  0, -1 } }, // east wall
		{ {  0,  1,  0 } },
		{ {  1,  0,  0 } },
		{ {  0,  0, -1 } }, // south wall
		{ {  0, -1,  0 } },
		{ {  1,  0,  0 } },
		{ {  0,  0, -1 } }, // north wall
	};

	int32_t best_axis = 0;
	float best = 0.0;

	for (int32_t i = 0; i < 6; i++) {
		const float dot = Vec3_Dot(plane->normal, base_axis[i * 3]);
		if (dot > best) {
			best = dot;
			best_axis = i;
		}
	}

	*xv = base_axis[best_axis * 3 + 1];
	*yv = base_axis[best_axis * 3 + 2];
}

/**
 * @brief Resolve the texinfo with identical properties to the one specified, or
 * allocate a new one.
 */
static int32_t FindTexinfo(const bsp_texinfo_t *tx) {

	const bsp_texinfo_t *tc = bsp_file.texinfo;
	for (int32_t i = 0; i < bsp_file.num_texinfo; i++, tc++) {

		if (tc->flags != tx->flags) {
			continue;
		}

		if (tc->value != tx->value) {
			continue;
		}

		if (strncmp(tc->texture, tx->texture, sizeof(tc->texture))) {
			continue;
		}

		if (!Vec4_EqualEpsilon(tc->vecs[0], tx->vecs[0], .01f) ||
			!Vec4_EqualEpsilon(tc->vecs[1], tx->vecs[1], .01f)) {
			continue;
		}

		return i;
	}

	if (bsp_file.num_texinfo == MAX_BSP_TEXINFO) {
		Com_Error(ERROR_FATAL, "MAX_BSP_TEXINFO\n");
	}

	bsp_file.texinfo[bsp_file.num_texinfo] = *tx;
	bsp_file.num_texinfo++;

	return bsp_file.num_texinfo - 1;
}

/**
 * @brief
 */
int32_t TexinfoForBrushSide(const brush_side_t *side, const vec3_t origin) {

	if (!side->texture[0]) {
		return 0;
	}

	bsp_texinfo_t tx;
	memset(&tx, 0, sizeof(tx));

	g_strlcpy(tx.texture, side->texture, sizeof(tx.texture));

	vec3_t vecs[2];
	TextureAxisFromPlane(&planes[side->plane_num], &vecs[0], &vecs[1]);

	vec2_t offset;
	offset.x = Vec3_Dot(origin, vecs[0]);
	offset.y = Vec3_Dot(origin, vecs[1]);

	vec2_t scale;
	scale.x = side->scale.x ?: 1.0;
	scale.y = side->scale.y ?: 1.0;

	// rotate axis
	float sinv, cosv;
	if (side->rotate == 0.0) {
		sinv = 0.0;
		cosv = 1.0;
	} else if (side->rotate == 90.0) {
		sinv = 1.0;
		cosv = 0.0;
	} else if (side->rotate == 180.0) {
		sinv = 0.0;
		cosv = -1.0;
	} else if (side->rotate == 270.0) {
		sinv = -1.0;
		cosv = 0.0;
	} else {
		sinv = sinf(Radians(side->rotate));
		cosv = cosf(Radians(side->rotate));
	}

	int32_t sv;
	if (vecs[0].x) {
		sv = 0;
	} else if (vecs[0].y) {
		sv = 1;
	} else {
		sv = 2;
	}

	int32_t tv;
	if (vecs[1].x) {
		tv = 0;
	} else if (vecs[1].y) {
		tv = 1;
	} else {
		tv = 2;
	}

	for (int32_t i = 0; i < 2; i++) {
		const float ns = cosv * vecs[i].xyz[sv] - sinv * vecs[i].xyz[tv];
		const float nt = sinv * vecs[i].xyz[sv] + cosv * vecs[i].xyz[tv];
		vecs[i].xyz[sv] = ns;
		vecs[i].xyz[tv] = nt;
	}

	for (int32_t i = 0; i < 2; i++)
		for (int32_t j = 0; j < 3; j++) {
			tx.vecs[i].xyzw[j] = vecs[i].xyz[j] / scale.xy[i];
		}

	tx.vecs[0].w = side->shift.x + offset.x;
	tx.vecs[1].w = side->shift.y + offset.y;
	
	tx.flags = side->surf;
	tx.value = side->value;

	return FindTexinfo(&tx);
}
