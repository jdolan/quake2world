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
static void R_LoadBspEntities(r_bsp_model_t *bsp) {

	bsp->luxel_size = Cm_EntityValue(Cm_Worldspawn(), "luxel_size")->integer;
	if (bsp->luxel_size <= 0) {
		bsp->luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;
	}

	Com_Debug(DEBUG_RENDERER, "Resolved luxel_size: %d\n", bsp->luxel_size);
}

/**
 * @brief
 */
static void R_LoadBspPlanes(r_bsp_model_t *bsp) {
	r_bsp_plane_t *out;

	const cm_bsp_plane_t *in = bsp->cm->planes;

	bsp->num_planes = bsp->cm->file.num_planes;
	bsp->planes = out = Mem_LinkMalloc(bsp->num_planes * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_planes; i++, out++, in++) {
		out->cm = in;
		out->blend_elements = g_ptr_array_new();
	}
}

/**
 * @brief
 */
static void R_LoadBspTexinfo(r_bsp_model_t *bsp) {
	r_bsp_texinfo_t *out;

	const bsp_texinfo_t *in = bsp->cm->file.texinfo;

	bsp->num_texinfo = bsp->cm->file.num_texinfo;
	bsp->texinfo = out = Mem_LinkMalloc(bsp->num_texinfo * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_texinfo; i++, in++, out++) {

		out->vecs[0] = in->vecs[0];
		out->vecs[1] = in->vecs[1];

		out->flags = in->flags;
		out->value = in->value;

		g_strlcpy(out->texture, in->texture, sizeof(out->texture));

		out->material = R_LoadMaterial(out->texture, ASSET_CONTEXT_TEXTURES);
	}
}

/**
 * @brief
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp) {

	bsp->num_vertexes = bsp->cm->file.num_vertexes;
	r_bsp_vertex_t *out = bsp->vertexes = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

	const bsp_vertex_t *in = bsp->cm->file.vertexes;
	for (int32_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {

		out->position = in->position;
		out->normal = in->normal;
		out->tangent = in->tangent;
		out->bitangent = in->bitangent;

		out->diffusemap = in->diffusemap;
		out->lightmap = in->lightmap;

		float alpha = 1.f;

		const r_bsp_texinfo_t *texinfo = bsp->texinfo + in->texinfo;
		switch (texinfo->flags & SURF_MASK_BLEND) {
			case SURF_BLEND_33:
				alpha = 0.333f;
				break;
			case SURF_BLEND_66:
				alpha = 0.666f;
				break;
		}

		out->color = Color_Color32(Color4f(1.f, 1.f, 1.f, alpha));
	}
}

/**
 * @brief
 */
static void R_LoadBspElements(r_bsp_model_t *bsp) {

	bsp->num_elements = bsp->cm->file.num_elements;
	GLuint *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

	const int32_t *in = bsp->cm->file.elements;
	for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
		*out = *in;
	}
}

/**
 * @brief Loads all r_bsp_face_t for the specified BSP model.
 */
static void R_LoadBspFaces(r_bsp_model_t *bsp) {

	const bsp_face_t *in = bsp->cm->file.faces;
	r_bsp_face_t *out;

	bsp->num_faces = bsp->cm->file.num_faces;
	bsp->faces = out = Mem_LinkMalloc(bsp->num_faces * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_faces; i++, in++, out++) {

		out->plane = bsp->planes + in->plane_num;
		out->plane_side = in->plane_num & 1;

		out->texinfo = bsp->texinfo + in->texinfo;
		out->contents = in->contents;

		out->bounds = in->bounds;

		out->vertexes = bsp->vertexes + in->first_vertex;
		out->num_vertexes = in->num_vertexes;

		out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));
		out->num_elements = in->num_elements;

		if (out->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		r_bsp_face_lightmap_t *lm = &out->lightmap;

		out->lightmap.s = in->lightmap.s;
		out->lightmap.t = in->lightmap.t;
		out->lightmap.w = in->lightmap.w;
		out->lightmap.h = in->lightmap.h;

		out->lightmap.matrix = in->lightmap.matrix;

		out->lightmap.st_mins = in->lightmap.st_mins;
		out->lightmap.st_maxs = in->lightmap.st_maxs;

		lm->stainmap = Mem_LinkMalloc(lm->w * lm->h * BSP_LIGHTMAP_BPP, bsp->faces);
		memset(lm->stainmap, 0xff, lm->w * lm->h * BSP_LIGHTMAP_BPP);
	}
}

/**
 * @brief
 */
static void R_LoadBspDrawElements(r_bsp_model_t *bsp) {
	r_bsp_draw_elements_t *out;

	bsp->num_draw_elements = bsp->cm->file.num_draw_elements;
	bsp->draw_elements = out = Mem_LinkMalloc(bsp->num_draw_elements * sizeof(*out), bsp);

	const bsp_draw_elements_t *in = bsp->cm->file.draw_elements;
	for (int32_t i = 0; i < bsp->num_draw_elements; i++, in++, out++) {

		out->plane = bsp->planes + in->plane_num;
		out->plane_side = in->plane_num & 1;

		out->texinfo = bsp->texinfo + in->texinfo;
		out->contents = in->contents;

		out->bounds = in->bounds;

		out->num_elements = in->num_elements;
		out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));

		if (out->texinfo->flags & SURF_MASK_BLEND) {

			r_bsp_plane_t *blend_plane;
			if (out->plane_side) {
				blend_plane = out->plane - 1;
			} else {
				blend_plane = out->plane;
			}

			g_ptr_array_add(blend_plane->blend_elements, out);
		}

		if (out->texinfo->material->cm->flags & (STAGE_STRETCH | STAGE_ROTATE)) {

			vec2_t st_mins = Vec2_Mins();
			vec2_t st_maxs = Vec2_Maxs();

			const GLuint *e = bsp->elements + in->first_element;
			for (int32_t j = 0; j < out->num_elements; j++, e++) {
				const r_bsp_vertex_t *v = &bsp->vertexes[*e];

				st_mins = Vec2_Minf(st_mins, v->diffusemap);
				st_maxs = Vec2_Maxf(st_maxs, v->diffusemap);
			}

			out->st_origin = Vec2_Scale(Vec2_Add(st_mins, st_maxs), .5f);
		}

		if (out->texinfo->flags & SURF_SKY) {
			if (bsp->sky) {
				Com_Warn("Model contains multiple sky elements\n");
			} else {
				bsp->sky = out;
			}
		}
	}
}

/**
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp) {
	r_bsp_leaf_t *out;

	const bsp_leaf_t *in = bsp->cm->file.leafs;

	bsp->num_leafs = bsp->cm->file.num_leafs;
	bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		out->contents = in->contents;

		out->bounds = in->bounds;
	}
}

/**
 * @brief Loads all r_bsp_node_t for the specified BSP model.
 */
static void R_LoadBspNodes(r_bsp_model_t *bsp) {
	r_bsp_node_t *out;

	const bsp_node_t *in = bsp->cm->file.nodes;

	bsp->num_nodes = bsp->cm->file.num_nodes;
	bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

		out->contents = CONTENTS_NODE; // differentiate from leafs

		out->bounds = in->bounds;

		out->plane = bsp->planes + in->plane_num;

		out->faces = bsp->faces + in->first_face;
		out->num_faces = in->num_faces;

		for (int32_t j = 0; j < 2; j++) {
			const int32_t c = in->children[j];
			if (c >= 0) {
				out->children[j] = bsp->nodes + c;
			} else {
				out->children[j] = (r_bsp_node_t *) (bsp->leafs + (-1 - c));
			}
		}
	}
}

/**
 * @brief
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent, r_bsp_inline_model_t *model) {

	node->parent = parent;
	node->model = model;

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	r_bsp_face_t *face = node->faces;
	for (int32_t i = 0; i < node->num_faces; i++, face++) {
		face->node = node;
	}

	R_SetupBspNode(node->children[0], node, model);
	R_SetupBspNode(node->children[1], node, model);
}

/**
 * @brief
 */
static void R_LoadBspInlineModels(r_bsp_model_t *bsp) {
	r_bsp_inline_model_t *out;

	const bsp_model_t *in = bsp->cm->file.models;

	bsp->num_inline_models = bsp->cm->file.num_models;
	bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

		out->head_node = bsp->nodes + in->head_node;

		out->bounds = in->bounds;

		out->faces = bsp->faces + in->first_face;
		out->num_faces = in->num_faces;

		out->blend_elements = g_ptr_array_new();

		out->draw_elements = bsp->draw_elements + in->first_draw_elements;
		out->num_draw_elements = in->num_draw_elements;

		R_SetupBspNode(out->head_node, NULL, out);
	}
}

/**
 * @brief Creates an r_model_t for each inline model so that entities may reference them.
 */
static void R_SetupBspInlineModels(r_model_t *mod) {

	r_bsp_inline_model_t *in = mod->bsp->inline_models;
	for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {

		char name[MAX_QPATH];
		g_snprintf(name, sizeof(name), "%s#%d", mod->media.name, i);

		r_model_t *out = (r_model_t *) R_AllocMedia(name, sizeof(r_model_t), R_MEDIA_MODEL);

		out->type = MOD_BSP_INLINE;
		out->bsp_inline = in;

		out->bounds = in->bounds;

		mod->bounds = Box3_Union(mod->bounds, out->bounds);

		R_RegisterDependency(&mod->media, &out->media);
	}
}

/**
 * @brief Loads the lightmap layers to a 2D array texture, appending a layer for the stainmap.
 */
static void R_LoadBspLightmap(r_model_t *mod) {

	const bsp_lightmap_t *in = mod->bsp->cm->file.lightmap;

	r_bsp_lightmap_t *out = mod->bsp->lightmap = Mem_LinkMalloc(sizeof(*out), mod->bsp);

	if (in) {
		out->width = in->width;
	} else {
		out->width = 1;
	}

	out->atlas = (r_image_t *) R_AllocMedia("lightmap", sizeof(r_image_t), R_MEDIA_IMAGE);
	out->atlas->media.Free = R_FreeImage;
	out->atlas->type = IT_LIGHTMAP;
	out->atlas->width = out->width;
	out->atlas->height = out->width;
	out->atlas->depth = BSP_LIGHTMAP_LAYERS + BSP_STAINMAP_LAYERS;
	out->atlas->target = GL_TEXTURE_2D_ARRAY;
	out->atlas->format = GL_RGB;

	const size_t in_size = out->width * out->width * BSP_LIGHTMAP_LAYERS * BSP_LIGHTMAP_BPP;
	const size_t out_size = out->atlas->width * out->atlas->height * out->atlas->depth * BSP_LIGHTMAP_BPP;

	byte *data = Mem_Malloc(out_size);

	if (in) {
		memcpy(data, (byte *) in + sizeof(bsp_lightmap_t), in_size);
	} else {
		memset(data, 0xff, in_size);
	}

	memset(data + in_size, 0xff, out->width * out->width * BSP_LIGHTMAP_BPP);

	R_UploadImage(out->atlas, GL_TEXTURE_2D_ARRAY, data);

	Mem_Free(data);
}

/**
 * @brief Resets all face stainmaps in the event that the map is reloaded.
 */
static void R_ResetBspLightmap(r_model_t *mod) {

	r_bsp_lightmap_t *out = mod->bsp->lightmap;

	glBindTexture(GL_TEXTURE_2D_ARRAY, out->atlas->texnum);

	r_bsp_face_t *face = mod->bsp->faces;
	for (int32_t i = 0; i < mod->bsp->num_faces; i++, face++) {

		memset(face->lightmap.stainmap, 0xff, face->lightmap.w * face->lightmap.h * BSP_LIGHTMAP_BPP);

		glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
				0,
				face->lightmap.s,
				face->lightmap.t,
				BSP_LIGHTMAP_LAYERS,
				face->lightmap.w,
				face->lightmap.h,
				1,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				face->lightmap.stainmap);
	}
}

/**
 * @brief
 */
static void R_LoadBspLightgrid(r_model_t *mod) {

	const bsp_lightgrid_t *in = mod->bsp->cm->file.lightgrid;

	r_bsp_lightgrid_t *out = mod->bsp->lightgrid = Mem_LinkMalloc(sizeof(*out), mod->bsp);

	if (in) {
		out->size = in->size;
	} else {
		out->size = Vec3i(1, 1, 1);
	}

	const vec3_t grid_size = Vec3_Scale(Vec3i_CastVec3(out->size), BSP_LIGHTGRID_LUXEL_SIZE);
	const vec3_t world_size = Box3_Size(mod->bounds);
	const vec3_t padding = Vec3_Scale(Vec3_Subtract(grid_size, world_size), 0.5);

	out->bounds = Box3_Expand3(mod->bounds, padding);

	const size_t luxels = out->size.x * out->size.y * out->size.z;

	byte *data;
	if (in) {
		data = (byte *) in + sizeof(bsp_lightgrid_t);
	} else {
		data = (byte []) {
			0xff, 0xff, 0xff,
			0xff, 0xff, 0xff,
			0xff, 0xff, 0xff,
			0x00, 0x00, 0x00, 0x00
		};
	}

	for (int32_t i = 0; i < (int32_t) lengthof(out->textures); i++) {

		r_image_t *texture = (r_image_t *) R_AllocMedia(va("lightgrid[%d]", i), sizeof(r_image_t), R_MEDIA_IMAGE);
		texture->media.Free = R_FreeImage;
		texture->type = IT_LIGHTGRID;
		texture->width = out->size.x;
		texture->height = out->size.y;
		texture->depth = out->size.z;
		texture->target = GL_TEXTURE_3D;

		if (i < BSP_LIGHTGRID_TEXTURES) {
			texture->format = GL_RGB;
		} else {
			texture->format = GL_RGBA;
		}

		R_UploadImage(texture, texture->target, data);

		if (i < BSP_LIGHTGRID_TEXTURES) {
			data += luxels * BSP_LIGHTGRID_BPP;
		} else {
			data += luxels * BSP_FOG_BPP;
		}

		out->textures[i] = texture;
	}
}

/**
 * @brief Create the depth elements buffer.
 */
static void R_LoadBspDepthPassElements(r_bsp_model_t *bsp) {

	glGenBuffers(1, &bsp->depth_pass_elements_buffer);

	const r_bsp_inline_model_t *in = bsp->inline_models;
	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (!(draw->texinfo->flags & SURF_MASK_TRANSLUCENT)) {
			bsp->num_depth_pass_elements += draw->num_elements;
		}
	}

	glBindBuffer(GL_COPY_WRITE_BUFFER, bsp->depth_pass_elements_buffer);

	glBufferData(GL_COPY_WRITE_BUFFER, bsp->num_depth_pass_elements * sizeof(GLuint), NULL, GL_STATIC_DRAW);

	glBindBuffer(GL_COPY_READ_BUFFER, bsp->elements_buffer);

	draw = in->draw_elements;

	GLintptr offset = 0;

	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (draw->texinfo->flags & SURF_MASK_TRANSLUCENT) {
			continue;
		}

		glCopyBufferSubData(GL_COPY_READ_BUFFER,
							GL_COPY_WRITE_BUFFER,
							(GLintptr) draw->elements,
							(GLintptr) offset,
							draw->num_elements * sizeof(GLuint));

		offset += draw->num_elements * sizeof(GLuint);
	}

	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

/**
 * @brief
 */
static void R_LoadBspOcclusionQueries(r_bsp_model_t *bsp) {

	const cm_bsp_brush_t *in = bsp->cm->brushes;
	r_bsp_occlusion_query_t *out = NULL;

	for (int32_t i = 0; i < bsp->cm->file.num_brushes; i++, in++) {
		if (in->contents & CONTENTS_OCCLUSION_QUERY) {

			if (bsp->num_occlusion_queries == MAX_BSP_OCCLUSION_QUERIES) {
				Com_Error(ERROR_DROP, "MAX_BSP_OCCLUSION_QUERIES");
			}
			
			bsp->occlusion_queries = Mem_Realloc(bsp->occlusion_queries, (bsp->num_occlusion_queries + 1) * sizeof(*out));
			out = bsp->occlusion_queries + bsp->num_occlusion_queries;

			glGenQueries(1, &out->name);

			out->bounds = Box3_Expand(in->bounds, NEAR_DIST);

			Box3_ToPoints(out->bounds, out->vertexes);

			out->pending = false;
			out->result = 1;

			bsp->num_occlusion_queries++;
		}
	}

	if (out) {
		Mem_Link(bsp, bsp->occlusion_queries);
	}

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_LoadBspVertexArray(r_model_t *mod) {

	glGenVertexArrays(1, &mod->bsp->vertex_array);
	glBindVertexArray(mod->bsp->vertex_array);

	glGenBuffers(1, &mod->bsp->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, mod->bsp->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, mod->bsp->num_vertexes * sizeof(r_bsp_vertex_t), mod->bsp->vertexes, GL_STATIC_DRAW);

	glGenBuffers(1, &mod->bsp->elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->elements_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->num_elements * sizeof(GLuint), mod->bsp->elements, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, normal));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, tangent));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, bitangent));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, diffusemap));
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, lightmap));
	glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, color));

	R_GetError(mod->media.name);

	glBindVertexArray(0);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/**
 * @brief Extra lumps we need to load for the renderer.
 */
#define R_BSP_LUMPS ( \
	(1 << BSP_LUMP_VERTEXES) | \
	(1 << BSP_LUMP_ELEMENTS) | \
	(1 << BSP_LUMP_FACES) | \
	(1 << BSP_LUMP_DRAW_ELEMENTS) | \
	(1 << BSP_LUMP_LIGHTMAP) | \
	(1 << BSP_LUMP_LIGHTGRID) \
)

/**
 * @brief
 */
static void R_LoadBspModel(r_model_t *mod, void *buffer) {

	bsp_header_t *file = (bsp_header_t *) buffer;

	mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);
	mod->bsp->cm = Cm_Bsp();

	// load in lumps that the renderer needs
	Bsp_LoadLumps(file, &mod->bsp->cm->file, R_BSP_LUMPS);

	R_LoadModelMaterials(mod);
	R_LoadBspEntities(mod->bsp);
	R_LoadBspPlanes(mod->bsp);
	R_LoadBspTexinfo(mod->bsp);
	R_LoadBspVertexes(mod->bsp);
	R_LoadBspElements(mod->bsp);
	R_LoadBspFaces(mod->bsp);
	R_LoadBspDrawElements(mod->bsp);
	R_LoadBspLeafs(mod->bsp);
	R_LoadBspNodes(mod->bsp);
	R_LoadBspInlineModels(mod->bsp);
	R_SetupBspInlineModels(mod);
	R_LoadBspVertexArray(mod);
	R_LoadBspLightmap(mod);
	R_LoadBspLightgrid(mod);
	R_LoadBspDepthPassElements(mod->bsp);
	R_LoadBspOcclusionQueries(mod->bsp);

	if (r_draw_bsp_lightgrid->value) {
		Bsp_UnloadLumps(&mod->bsp->cm->file, R_BSP_LUMPS & ~(1 << BSP_LUMP_LIGHTGRID));
	} else {
		Bsp_UnloadLumps(&mod->bsp->cm->file, R_BSP_LUMPS);
	}

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel:   %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->bsp->num_vertexes);
	Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->bsp->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->bsp->num_faces);
	Com_Debug(DEBUG_RENDERER, "!  Draw elements:  %d\n", mod->bsp->num_draw_elements);
	Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief
 */
static void R_RegisterBspModel(r_media_t *self) {

	r_model_t *mod = (r_model_t *) self;

	r_bsp_texinfo_t *texinfo = mod->bsp->texinfo;
	for (int32_t i = 0; i < mod->bsp->num_texinfo; i++, texinfo++) {
		R_RegisterDependency(self, (r_media_t *) texinfo->material);
	}

	R_RegisterDependency(self, (r_media_t *) mod->bsp->lightmap->atlas);

	R_ResetBspLightmap(mod);

	for (size_t i = 0; i < lengthof(mod->bsp->lightgrid->textures); i++) {
		R_RegisterDependency(self, (r_media_t *) mod->bsp->lightgrid->textures[i]);
	}

	r_world_model = mod;
}

/**
 * @brief
 */
static void R_FreeBspModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	glDeleteBuffers(1, &mod->bsp->vertex_buffer);
	glDeleteBuffers(1, &mod->bsp->elements_buffer);
	glDeleteBuffers(1, &mod->bsp->depth_pass_elements_buffer);

	glDeleteVertexArrays(1, &mod->bsp->vertex_array);

	for (int32_t i = 0; i < mod->bsp->num_occlusion_queries; i++) {
		glDeleteQueries(1, &mod->bsp->occlusion_queries[i].name);
	}

	r_bsp_plane_t *plane = mod->bsp->planes;
	for (int32_t i = 0; i < mod->bsp->num_planes; i++, plane++) {
		g_ptr_array_free(plane->blend_elements, 1);
	}

	r_bsp_inline_model_t *in = mod->bsp->inline_models;
	for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {
		g_ptr_array_free(in->blend_elements, 1);
	}
}

/**
 * @brief
 */
const r_model_format_t r_bsp_model_format = {
	.extension = "bsp",
	.type = MOD_BSP,
	.Load = R_LoadBspModel,
	.Register = R_RegisterBspModel,
	.Free = R_FreeBspModel
};
