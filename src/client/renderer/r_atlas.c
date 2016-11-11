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
 * @brief Free event listener for atlases.
 */
static void R_FreeAtlas(r_media_t *media) {
	r_atlas_t *atlas = (r_atlas_t *) media;

	if (atlas->image.texnum) {
		glDeleteTextures(1, &atlas->image.texnum);
	}

	g_array_unref(atlas->images);

	g_hash_table_destroy(atlas->hash);
}

/**
 * @brief Creates a blank state for an atlas and returns it.
 */
r_atlas_t *R_CreateAtlas(const char *name) {
	r_atlas_t *atlas = (r_atlas_t *) R_AllocMedia(name, sizeof(r_atlas_t));

	atlas->image.media.Free = R_FreeAtlas;
	atlas->image.type = IT_ATLAS_MAP;
	g_strlcpy(atlas->image.media.name, name, sizeof(atlas->image.media.name));

	atlas->images = g_array_new(false, true, sizeof(r_atlas_image_t));

	atlas->hash = g_hash_table_new(g_direct_hash, g_direct_equal);

	return atlas;
}

/**
 * @brief Add an image to the list of images for this atlas.
 */
void R_AddImageToAtlas(r_atlas_t *atlas, const r_image_t *image) {

	// ignore duplicates
	if (g_hash_table_contains(atlas->hash, image)) {
		return;
	}

	// add to array
	g_array_append_vals(atlas->images, &(const r_atlas_image_t) {
		.input_image = image
	}, 1);

	// add blank entry to hash, it's filled in in upload stage
	g_hash_table_insert(atlas->hash, (gpointer) image, NULL);

	// might as well register it as a dependent
	R_RegisterDependency((r_media_t *) atlas, (r_media_t *) image);

	Com_Debug("Atlas %s -> %s\n", atlas->image.media.name, image->media.name);
}

/**
 * @brief Resolve an atlas image from an atlas and image.
 */
const r_atlas_image_t *R_GetAtlasImageFromAtlas(const r_atlas_t *atlas, const r_image_t *image) {

	return (const r_atlas_image_t *) g_hash_table_lookup(atlas->hash, image);
}

typedef struct {
	uint16_t width, height;
	uint16_t x, y;

	uint32_t right, down; // nodes to the right/below this one
	_Bool used; // whether this node is used by an image or not yet
} r_packer_node_t;

typedef struct {
	GArray *nodes;
	uint32_t root;
} r_packer_t;

/**
 * @brief Finds a free node that is big enough to hold us.
 */
static r_packer_node_t *R_AtlasPacker_FindNode(r_packer_t *packer, r_packer_node_t *root, r_atlas_image_t *image) {

	if (root->used) {
		r_packer_node_t *node = R_AtlasPacker_FindNode(packer, &g_array_index(packer->nodes, r_packer_node_t, root->right),
		                        image);

		if (node) {
			return node;
		}

		node = R_AtlasPacker_FindNode(packer, &g_array_index(packer->nodes, r_packer_node_t, root->down), image);

		if (node) {
			return node;
		}
	} else if (image->input_image->width <= root->width && image->input_image->height <= root->height) {
		return root;
	}

	return NULL;
}

/**
 * @brief Split a packer node into two, assigning the first to the image.
 */
static r_packer_node_t *R_AtlasPacker_SplitNode(r_packer_t *packer, r_packer_node_t *node, r_atlas_image_t *image) {
	const intptr_t index = (intptr_t) (node - (r_packer_node_t *) packer->nodes->data);

	node->used = true;
	node->down = packer->nodes->len;
	node->right = packer->nodes->len + 1;

	g_array_append_vals(packer->nodes, &(const r_packer_node_t[]) {
		{
			.width = node->width,
			 .height = node->height - image->input_image->height,
			  .x = node->x,
			   .y = node->y + image->input_image->height,
			    .right = -1,
			     .down = -1
		}, {
			.width = node->width - image->input_image->width,
			.height = image->input_image->height,
			.x = node->x + image->input_image->width,
			.y = node->y,
			.right = -1,
			.down = -1
		}
	}, 2);

	return &g_array_index(packer->nodes, r_packer_node_t, index);
}

#define GROW_RIGHT true
#define GROW_DOWN false

/**
 * @brief Grow the packer in the specified direction.
 */
static r_packer_node_t *R_AtlasPacker_Grow(r_packer_t *packer, r_atlas_image_t *image, const _Bool grow_direction) {
	const uint32_t new_root_id = packer->nodes->len;
	const uint32_t new_connect_id = packer->nodes->len + 1;
	const r_packer_node_t *old_root = &g_array_index(packer->nodes, r_packer_node_t, packer->root);

	if (grow_direction == GROW_RIGHT) {
		g_array_append_vals(packer->nodes, &(const r_packer_node_t[]) {
			{
				.width = old_root->width + image->input_image->width,
				 .height = old_root->height,
				  .used = true,
				   .down = packer->root,
				    .right = new_connect_id
			}, {
				.width = image->input_image->width,
				.height = old_root->height,
				.x = old_root->width,
				.y = 0,
				.right = -1,
				.down = -1
			}
		}, 2);

	} else {
		g_array_append_vals(packer->nodes, &(const r_packer_node_t[]) {
			{
				.width = old_root->width,
				 .height = old_root->height + image->input_image->height,
				  .used = true,
				   .right = packer->root,
				    .down = new_connect_id
			}, {
				.width = old_root->width,
				.height = image->input_image->height,
				.x = 0,
				.y = old_root->height,
				.right = -1,
				.down = -1
			}
		}, 2);
	}

	packer->root = new_root_id;

	r_packer_node_t *node = R_AtlasPacker_FindNode(packer, &g_array_index(packer->nodes, r_packer_node_t, packer->root),
	                        image);

	if (node != NULL) {
		return R_AtlasPacker_SplitNode(packer, node, image);
	}

	return NULL;
}

/**
 * @brief Checks to see where the packer should grow into next. This keeps the atlas square.
 */
static r_packer_node_t *R_AtlasPacker_GrowNode(r_packer_t *packer, r_atlas_image_t *image) {
	const r_packer_node_t *root = &g_array_index(packer->nodes, r_packer_node_t, packer->root);

	const _Bool canGrowDown = (image->input_image->width <= root->width);
	const _Bool canGrowRight = (image->input_image->height <= root->height);

	const _Bool shouldGrowRight = canGrowRight &&
	                              (root->height >= (root->width +
	                                      image->input_image->width)); // attempt to keep square-ish by growing right when height is much greater than width
	const _Bool shouldGrowDown = canGrowDown &&
	                             (root->width >= (root->height +
	                                     image->input_image->height));  // attempt to keep square-ish by growing down  when width  is much greater than height

	if (shouldGrowRight) {
		return R_AtlasPacker_Grow(packer, image, GROW_RIGHT);
	} else if (shouldGrowDown) {
		return R_AtlasPacker_Grow(packer, image, GROW_DOWN);
	} else if (canGrowRight) {
		return R_AtlasPacker_Grow(packer, image, GROW_RIGHT);
	} else if (canGrowDown) {
		return R_AtlasPacker_Grow(packer, image, GROW_DOWN);
	}

	return NULL;
}

typedef struct {
	uint16_t width, height;
	uint16_t num_mips;
} r_atlas_params_t;

/**
 * @brief Stitches the atlas, returning atlas parameters.
 */
static void R_StitchAtlas(r_atlas_t *atlas, r_atlas_params_t *params) {
	uint16_t min_size = USHRT_MAX;
	params->width = params->height = 0;

	// setup base packer parameters
	r_packer_t packer;
	packer.nodes = g_array_new(false, true, sizeof(r_packer_node_t));

	g_array_append_vals(packer.nodes, &(const r_packer_node_t) {
		.width = g_array_index(atlas->images, r_atlas_image_t, 0).input_image->width,
		 .height = g_array_index(atlas->images, r_atlas_image_t, 0).input_image->height,
		  .right = -1,
		   .down = -1
	}, 1);

	packer.root = 0;

	// stitch!
	for (uint16_t i = 0; i < atlas->images->len; i++) {
		r_atlas_image_t *image = &g_array_index(atlas->images, r_atlas_image_t, i);
		r_packer_node_t *node = R_AtlasPacker_FindNode(&packer, &g_array_index(packer.nodes, r_packer_node_t, packer.root),
		                        image);

		if (node != NULL) {
			node = R_AtlasPacker_SplitNode(&packer, node, image);
		} else {
			node = R_AtlasPacker_GrowNode(&packer, image);
		}

		params->width = MAX(node->x + image->input_image->width, params->width);
		params->height = MAX(node->y + image->input_image->height, params->height);

		min_size = MIN(min_size, MIN(image->input_image->width, image->input_image->height));

		image->position[0] = node->x;
		image->position[1] = node->y;
		image->image.width = image->input_image->width;
		image->image.height = image->input_image->height;
		image->image.type = IT_ATLAS_IMAGE;
		image->image.texnum = atlas->image.texnum;

		// replace with new atlas image ptr
		g_hash_table_replace(atlas->hash, (gpointer) image->input_image, (gpointer) image);
	}

	// free temp nodes
	g_array_free(packer.nodes, true);

	// see how many mips we need to make
	params->num_mips = 1;

	while (min_size > 1) {
		min_size = floor(min_size / 2.0);
		params->num_mips++;
	}
}

/**
 * @brief Generate mipmap levels for the specified atlas.
 */
void R_GenerateAtlasMips(r_atlas_t *atlas, r_atlas_params_t *params) {

	for (uint16_t i = 0; i < params->num_mips; i++) {
		const uint16_t mip_scale = 1 << i;

		const r_pixel_t mip_width = params->width / mip_scale;
		const r_pixel_t mip_height = params->height / mip_scale;

		R_BindTexture(atlas->image.texnum);

		// make initial space
		glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, mip_width, mip_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		// pop in all of the textures
		for (uint16_t j = 0; j < atlas->images->len; j++) {
			r_atlas_image_t *image = &g_array_index(atlas->images, r_atlas_image_t, j);

			// pull the mip pixels from the original image
			GLint image_mip_width;
			GLint image_mip_height;

			R_BindTexture(image->input_image->texnum);

			glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_WIDTH, &image_mip_width);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_HEIGHT, &image_mip_height);

			GLubyte *subimage_pixels = Mem_Malloc(image_mip_width * image_mip_height * 4);

			glGetTexImage(GL_TEXTURE_2D, i, GL_RGBA, GL_UNSIGNED_BYTE, subimage_pixels);

			// push them into the atlas
			R_BindTexture(atlas->image.texnum);

			const r_pixel_t subimage_x = image->position[0] / mip_scale;
			const r_pixel_t subimage_y = image->position[1] / mip_scale;

			glTexSubImage2D(GL_TEXTURE_2D, i, subimage_x, subimage_y, image_mip_width, image_mip_height, GL_RGBA, GL_UNSIGNED_BYTE,
			                subimage_pixels);

			// free scratch
			Mem_Free(subimage_pixels);

			// if we're setting up mip 0, set up texcoords
			if (i == 0) {
				Vector4Set(image->texcoords,
				           image->position[0] / (vec_t) params->width,
				           image->position[1] / (vec_t) params->height,
				           (image->position[0] + image->input_image->width) / (vec_t) params->width,
				           (image->position[1] + image->input_image->height) / (vec_t) params->height);
			}
		}
	}
}

/**
 * @brief Comparison function for atlas images. This keeps the larger ones running first.
 */
static int R_AtlasImage_Compare(gconstpointer a, gconstpointer b) {
	const r_atlas_image_t *ai = (const r_atlas_image_t *) a;
	const r_atlas_image_t *bi = (const r_atlas_image_t *) b;
	int cmp;

	if ((cmp = MAX(bi->input_image->width, bi->input_image->height) - MAX(ai->input_image->width,
	           ai->input_image->height)) ||
	        (cmp = MIN(bi->input_image->width, bi->input_image->height) - MIN(ai->input_image->width, ai->input_image->height)) ||
	        (cmp = bi->input_image->height - ai->input_image->height) ||
	        (cmp = bi->input_image->width - ai->input_image->width)) {
		return cmp;
	}

	return 0;
}

/**
 * @brief Compiles the specified atlas.
 */
void R_CompileAtlas(r_atlas_t *atlas) {

	// sort images, to ensure best stitching
	g_array_sort(atlas->images, R_AtlasImage_Compare);

	r_atlas_params_t params;
	uint32_t time_start = SDL_GetTicks();

	// make the image if we need to
	if (!atlas->image.texnum) {

		glGenTextures(1, &(atlas->image.texnum));
		R_BindTexture(atlas->image.texnum);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_image_state.filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_image_state.anisotropy);
	} else {
		R_BindTexture(atlas->image.texnum);
	}

	// stitch
	R_StitchAtlas(atlas, &params);

	// set up num mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, params.num_mips - 1);

	// run generation
	R_GenerateAtlasMips(atlas, &params);

	// check for errors
	R_GetError(atlas->image.media.name);

	// register if we aren't already
	R_RegisterMedia((r_media_t *) atlas);

	uint32_t time = SDL_GetTicks() - time_start;
	Com_Debug("Atlas %s compiled in %u ms", atlas->image.media.name, time);
}
