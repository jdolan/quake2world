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

#if 0

/**
 * @brief Allocates an initializes the flare for the specified surface, if one
 * should be applied. The flare is linked to the provided BSP model and will
 * be freed automatically.
 */
void R_CreateBspSurfaceFlare(r_bsp_model_t *bsp, r_bsp_face_t *surf) {
	vec3_t center, span;

	const r_material_t *m = surf->texinfo->material;

	if (!(m->cm->flags & STAGE_FLARE)) { // surface is not flared
		return;
	}

	surf->flare = Mem_LinkMalloc(sizeof(*surf->flare), bsp);

	// move the flare away from the surface, into the level
	center = Vec3_Mix(surf->mins, surf->maxs, 0.5);
	surf->flare->particle.org = vec3_add(center, Vec3_Scale(surf->plane->normal, 2.0);

	// calculate the flare radius based on surface size
	span = Vec3_Subtract(surf->maxs, surf->mins);
	surf->flare->radius = Vec3_Length(span);

	const r_stage_t *s = m->stages; // resolve the flare stage
	while (s) {
		if (s->cm->flags & STAGE_FLARE) {
			break;
		}
		s = s->next;
	}

	if (!s) {
		return;
	}

	// resolve flare color
	if (s->cm->flags & STAGE_COLOR) {
		surf->flare->particle.color = s->cm->color;
	} else {
		surf->flare->particle.color = surf->texinfo->material->diffuse->color;
	}

	// and scaled radius
	if (s->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
		surf->flare->radius *= (s->cm->scale.s ? s->cm->scale.s : s->cm->scale.t);
	}

	// and image
	surf->flare->particle.type = PARTICLE_FLARE;
	surf->flare->particle.media = (r_media_t *) s->image;
	surf->flare->particle.blend = GL_ONE;
}

/**
 * @brief Flares are batched by their texture. Usually, this means one draw operation
 * for all flares in view. Flare visibility is calculated every few millis, and
 * flare alpha is ramped up or down depending on the results of the visibility
 * trace. Flares are also faded according to the angle of their surface to the
 * view origin.
 */
void R_AddFlareBspFaces(const r_bsp_faces_t *surfs) {
	if (!r_flares->value || r_draw_wireframe->value) {
		return;
	}

	if (!surfs->count) {
		return;
	}

	for (size_t i = 0; i < surfs->count; i++) {
		const r_bsp_face_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame) {
			continue;
		}

		r_bsp_flare_t *f = surf->flare;

		// periodically test visibility to ramp alpha
		if (r_view.ticks - f->time > 15) {

			if (r_view.ticks - f->time > 500) { // reset old flares
				f->alpha = 0.0;
			}

			cm_trace_t tr = Cl_Trace(r_view.origin, f->particle.org, NULL, NULL, 0, CONTENTS_MASK_CLIP_PROJECTILE);

			f->alpha += (tr.fraction == 1.0) ? 0.03 : -0.15; // ramp
			f->alpha = clampf(f->alpha, 0.0, 1.0); // clamp

			f->time = r_view.ticks;
		}

		vec3_t view;
		view = Vec3_Subtract(f->particle.org, r_view.origin);
		const float dist = view = Vec3_Normalize(view);

		// fade according to angle
		const float cos = vec3_dot(surf->plane->normal, view);
		if (cos > 0.0) {
			continue;
		}

		float alpha = 0.1 + -cos * r_flares->value;

		if (alpha > 1.0) {
			alpha = 1.0;
		}

		alpha = f->alpha * alpha;

		if (alpha <= FLT_EPSILON) {
			continue;
		}

		// scale according to distance
		f->particle.scale = f->radius + (f->radius * dist * .0005);
		f->particle.color[3] = alpha;

		R_AddParticle(&f->particle);
	}
}

/**
 * @brief
 */
void R_AddFlares(void) {
	const r_sorted_bsp_faces_t *surfs = r_world_model->bsp->sorted_surfaces;

	R_AddFlareBspFaces(&surfs->flare);

	R_AddBspInlineModelFlares(&r_sorted_entities.bsp_inline_entities);
}

#endif
