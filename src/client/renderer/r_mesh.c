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

/**
 * @brief
 */
void R_ApplyMeshConfig(r_entity_t *e) {

	assert(IS_MESH_MODEL(e->model));

	const r_mesh_config_t *c;
	
	if (e->parent) {
		c = &e->model->mesh->config.link;
	} else if (e->effects & EF_WEAPON) {
		c = &e->model->mesh->config.view;
	} else {
		c = &e->model->mesh->config.world;
	}

	Matrix4x4_Concat(&e->matrix, &e->matrix, &c->transform);

	e->effects |= c->flags;
}

/**
 * @brief Returns the desired tag structure, or `NULL`.
 * @param mod The model to check for the specified tag.
 * @param frame The frame to fetch the tag on.
 * @param name The name of the tag.
 * @return The tag structure.
*/
static const r_mesh_tag_t *R_MeshTag(const r_model_t *mod, const char *name, const int32_t frame) {

	if (frame > mod->mesh->num_frames) {
		Com_Warn("%s: Invalid frame: %d\n", mod->media.name, frame);
		return NULL;
	}

	const r_mesh_model_t *model = mod->mesh;
	const r_mesh_tag_t *tag = &model->tags[frame * model->num_tags];

	for (int32_t i = 0; i < model->num_tags; i++, tag++) {
		if (!g_strcmp0(name, tag->name)) {
			return tag;
		}
	}

	Com_Warn("%s: Tag not found: %s\n", mod->media.name, name);
	return NULL;
}

/**
 * @brief Applies transformation and rotation for the specified linked entity. The matrix
 * component of the parent and child must be set up already. The child's matrix will be modified
 * by this function.
 */
void R_ApplyMeshTag(r_entity_t *e) {

	// interpolate the tag over the frames of the parent entity

	const r_mesh_tag_t *t1 = R_MeshTag(e->parent->model, e->tag, e->parent->old_frame);
	const r_mesh_tag_t *t2 = R_MeshTag(e->parent->model, e->tag, e->parent->frame);

	if (!t1 || !t2) {
		Com_Warn("Invalid tag %s\n", e->tag);
		return;
	}

	mat4_t tag_transform;

	Matrix4x4_Interpolate(&tag_transform, &t2->matrix, &t1->matrix, e->parent->back_lerp);
	Matrix4x4_Normalize(&tag_transform, &tag_transform);

	// add local origins to the local offset
	Matrix4x4_Concat(&tag_transform, &tag_transform, &e->matrix);

	// move by parent matrix
	Matrix4x4_Concat(&e->matrix, &e->parent->matrix, &tag_transform);

	// calculate final origin/angles
	vec3_t forward;
	
	Matrix4x4_ToVectors(&e->matrix, forward.xyz, NULL, NULL, e->origin.xyz);

	e->angles = Vec3_Euler(forward);

	e->scale = Matrix4x4_ScaleFromMatrix(&e->matrix);
}

/**
 * @return True if the specified entity was frustum-culled and can be skipped.
 */
_Bool R_CullMeshEntity(const r_entity_t *e) {

	assert(IS_MESH_MODEL(e->model));

	if (e->effects & EF_WEAPON) { // never cull the weapon
		return false;
	}

	// calculate scaled bounding box in world space

	const vec3_t mins = Vec3_Add(e->origin, Vec3_Scale(e->model->mins, e->scale));
	const vec3_t maxs = Vec3_Add(e->origin, Vec3_Scale(e->model->maxs, e->scale));

	return R_CullBox(mins, maxs);
}
