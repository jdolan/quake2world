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

/*
 * In video memory, lightmaps are chunked into NxN RGB blocks. In the BSP,
 * they are a contiguous lump. During the loading process, we use floating
 * point to provide precision.
 */

typedef struct {
	r_bsp_model_t *bsp;
	GSList *blocks;
	file_t *cache_file;
} r_lightmap_state_t;

#define R_LMCACHE_MAGIC		0xABCDDCBA

typedef struct {
	uint32_t		magic;
	uint32_t		size;
	uint32_t		num_packers;
} r_lmcache_header_t;

typedef struct {
	uint32_t		width, height; // final lm size
	uint32_t		count; // # of surfs this packer is responsible for
} r_lmcache_packer_header_t;

static r_lightmap_state_t r_lightmap_state;

/**
 * @brief The source of my misery for about 3 weeks.
 */
static void R_FreeLightmap(r_media_t *media) {
	glDeleteTextures(1, &((r_image_t *) media)->texnum);
}

/**
 * @brief Allocates a new lightmap (or deluxemap) image handle.
 */
static r_image_t *R_AllocLightmap_(r_image_type_t type, const uint32_t width, const uint32_t height) {
	static uint32_t count;
	char name[MAX_QPATH];

	const char *base;

	switch (type) {
		case IT_LIGHTMAP:
		default:
			base = "lightmap";
			break;
		case IT_DELUXEMAP:
			base = "deluxemap";
			break;
		case IT_STAINMAP:
			base = "stainmap";
			break;
	}

	g_snprintf(name, sizeof(name), "%s %u", base, count++);

	r_image_t *image = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);

	image->media.Free = R_FreeLightmap;

	image->type = type;
	image->width = width;
	image->height = height;

	return image;
}

/**
 * @brief Allocate handles for the stainmap
 */
static void R_AllocStainmap_(const uint32_t width, const uint32_t height, r_stainmap_t *out) {
	out->image = R_AllocLightmap_(IT_STAINMAP, width, height);

	R_UploadImage(out->image, GL_RGBA, NULL);

	out->fb = R_CreateFramebuffer(va("%s_fb", out->image->media.name));
	R_AttachFramebufferImage(out->fb, out->image);
	
	if (!R_FramebufferReady(out->fb)) {
		Com_Warn("Unable to allocate a stainmap framebuffer");
		memset(out, 0, sizeof(r_stainmap_t));
	}

	Matrix4x4_FromOrtho(&out->projection, 0.0, width, height, 0.0, -1.0, 1.0);

	R_BindFramebuffer(FRAMEBUFFER_DEFAULT);
}

#define R_AllocLightmap(w, h) R_AllocLightmap_(IT_LIGHTMAP, w, h)
#define R_AllocDeluxemap(w, h) R_AllocLightmap_(IT_DELUXEMAP, w, h)
#define R_AllocStainmap(w, h, o) R_AllocStainmap_(w, h, o)

/**
 * @brief
 */
static void R_BuildDefaultLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, byte *sout,
                                   byte *dout, size_t stride) {

	const uint32_t smax = surf->lightmap_size[0];
	const uint32_t tmax = surf->lightmap_size[1];

	stride -= (smax * 3);

	for (uint32_t i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (uint32_t j = 0; j < smax; j++) {

			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += 3;

			dout[0] = 127;
			dout[1] = 127;
			dout[2] = 255;

			dout += 3;
		}
	}
}

/**
 * @brief Apply brightness, saturation and contrast to the lightmap.
 */
static void R_FilterLightmap(uint32_t width, uint32_t height, byte *lightmap) {
	static r_image_t image = { .type = IT_LIGHTMAP };

	image.width = width;
	image.height = height;

	R_FilterImage(&image, GL_RGB, lightmap, false);
}

/**
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGB to the specified destinations.
 *
 * @param in The beginning of the surface lightmap [and deluxemap] data.
 * @param sout The destination for processed lightmap data.
 * @param dout The destination for processed deluxemap data.
 */
static void R_BuildLightmap(const r_bsp_model_t *bsp, const r_bsp_surface_t *surf, const byte *in,
                            byte *lout, byte *dout, size_t stride) {

	const uint32_t smax = surf->lightmap_size[0];
	const uint32_t tmax = surf->lightmap_size[1];

	const size_t size = smax * tmax;
	stride -= (smax * 3);

	byte *lightmap = (byte *) Mem_TagMalloc(size * 3, MEM_TAG_RENDERER);
	byte *lm = lightmap;

	byte *deluxemap = (byte *) Mem_TagMalloc(size * 3, MEM_TAG_RENDERER);
	byte *dm = deluxemap;

	// convert the raw lightmap samples to RGBA for softening
	for (size_t i = 0; i < size; i++) {
		*lm++ = *in++;
		*lm++ = *in++;
		*lm++ = *in++;

		// read in directional samples for per-pixel lighting as well
		if (bsp->version == BSP_VERSION_QUETOO) {
			*dm++ = *in++;
			*dm++ = *in++;
			*dm++ = *in++;
		} else {
			*dm++ = 127;
			*dm++ = 127;
			*dm++ = 255;
		}
	}

	// apply modulate, contrast, saturation, etc..
	R_FilterLightmap(smax, tmax, lightmap);

	// the lightmap is uploaded to the card via the strided block

	lm = lightmap;
	dm = deluxemap;

	for (uint32_t t = 0; t < tmax; t++, lout += stride, dout += stride) {
		for (uint32_t s = 0; s < smax; s++) {

			// copy the lightmap and deluxemap to the strided block
			*lout++ = *lm++;
			*lout++ = *lm++;
			*lout++ = *lm++;

			*dout++ = *dm++;
			*dout++ = *dm++;
			*dout++ = *dm++;
		}
	}

	Mem_Free(lightmap);
	Mem_Free(deluxemap);
}

/**
 * @brief Stable sort, for read/write purposes
 */
static gint R_InsertBlock_CompareFunc(gconstpointer  a,
                                      gconstpointer  b) {

	const r_bsp_surface_t *ai = (const r_bsp_surface_t *) a;
	const r_bsp_surface_t *bi = (const r_bsp_surface_t *) b;

	int32_t result = bi->lightmap_size[1] - ai->lightmap_size[1];

	if (result) {
		return result;
	}

	return bi->index - ai->index;
}

/**
 * @brief Pushes lightmap to list of blocks to be processed
 */
void R_CreateBspSurfaceLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, const byte *data) {

	if (!(surf->flags & R_SURF_LIGHTMAP)) {
		return;
	}

	surf->lightmap_input = data;
	r_lightmap_state.blocks = g_slist_prepend(r_lightmap_state.blocks, surf);
}

/**
 * @brief Uploads sorted lightmaps from start to (end - 1) and
 * puts them in the new maps sized to width/height
 */
static void R_UploadPackedLightmaps(const uint32_t width, const uint32_t height, r_bsp_model_t *bsp, GSList *start, GSList *end, const r_packer_t *packer, const uint32_t num_surfs) {

	// edge case, no blocks left
	if (!width || !height || !start) {
		Com_Error(ERROR_DROP, "Unable to load lightmaps - this is bad!");
	}

	// write to cache
	if (r_lightmap_state.cache_file) {

		Fs_Write(r_lightmap_state.cache_file, &(const r_lmcache_packer_header_t) {
			.width = width,
			.height = height,
			.count = (end == NULL) ? g_slist_length(start) : g_slist_position(start, end)
		}, sizeof(r_lmcache_packer_header_t), 1);

		// serialize packer
		R_AtlasPacker_Serialize(packer, r_lightmap_state.cache_file);
	}

	// allocate the image
	r_image_t *lightmap = R_AllocLightmap(width, height);
	r_image_t *deluxemap = R_AllocDeluxemap(width, height);
	r_stainmap_t stainmap = { .image = NULL, .fb = NULL };

	if (r_stainmaps->value) {
		R_AllocStainmap(width, height, &stainmap);
	}

	// temp buffers
	byte *sample_buffer = Mem_Malloc(width * height * 3);
	byte *direction_buffer = Mem_Malloc(width * height * 3);

	do {
		r_bsp_surface_t *surf = (r_bsp_surface_t *) start->data;

		const size_t stride = width * 3;
		const size_t lightmap_offset = (surf->lightmap_t *width + surf->lightmap_s) * 3;

		byte *sout = sample_buffer + lightmap_offset;
		byte *dout = direction_buffer + lightmap_offset;

		if (surf->lightmap_input) {
			R_BuildLightmap(bsp, surf, surf->lightmap_input, sout, dout, stride);
		} else {
			R_BuildDefaultLightmap(bsp, surf, sout, dout, stride);
		}

		surf->lightmap = lightmap;
		surf->deluxemap = deluxemap;

		if (stainmap.fb) {
			surf->stainmap = stainmap;
		}

		if (r_lightmap_state.cache_file) {

			Fs_Write(r_lightmap_state.cache_file, &surf->lightmap_s, sizeof(surf->lightmap_s), 1);
			Fs_Write(r_lightmap_state.cache_file, &surf->lightmap_t, sizeof(surf->lightmap_t), 1);
		}

		start = start->next;
	} while (start != end);

	// upload!
	R_UploadImage(lightmap, GL_RGB, sample_buffer);
	R_UploadImage(deluxemap, GL_RGB, direction_buffer);

	Mem_Free(sample_buffer);
	Mem_Free(direction_buffer);
}

/**
 * @brief Attempt to load, parse and upload the surface lightmap cache.
 * @returns false if the cache does not exist or is out of date.
 */
static _Bool R_LoadBspSurfaceLightmapCache(r_bsp_model_t *bsp) {

	if (!r_lmcache->integer) {
		return false;
	}

	char filename[MAX_QPATH];
	g_snprintf(filename, sizeof(filename), "lmcache/%s.lmc", bsp->cm->name);

	if (!Fs_Exists(filename)) {
		return false;
	}

	file_t *file = Fs_OpenRead(filename);

	if (!file) {
		return false;
	}

	r_lmcache_header_t header;
	
	// read header
	if (!Fs_Read(file, &header, sizeof(header), 1)) {

		Fs_Close(file);
		return false;
	}

	// check header validity
	// FIXME: better hashing?
	if (header.magic != R_LMCACHE_MAGIC ||
		header.size != bsp->cm->size) {

		Fs_Close(file);
		return false;
	}

	// read the packers
	r_packer_t packer;
	memset(&packer, 0, sizeof(packer));

	GSList *start = r_lightmap_state.blocks, *list = start;

	for (uint32_t i = 0; i < header.num_packers; i++) {
		r_lmcache_packer_header_t packer_header;

		if (!Fs_Read(file, &packer_header, sizeof(packer_header), 1)) {
			
			R_AtlasPacker_FreePacker(&packer);
			Fs_Close(file);
			return false;
		}

		if (!R_AtlasPacker_Unserialize(file, &packer)) {
			
			R_AtlasPacker_FreePacker(&packer);
			Fs_Close(file);
			return false;
		}

		// read in the surf x/y positions
		for (uint32_t s = 0; s < packer_header.count; s++) {

			r_bsp_surface_t *surf = (r_bsp_surface_t *) list->data;

			if (!Fs_Read(file, &surf->lightmap_s, sizeof(surf->lightmap_s), 1) ||
				!Fs_Read(file, &surf->lightmap_t, sizeof(surf->lightmap_t), 1)) {
				
				R_AtlasPacker_FreePacker(&packer);
				Fs_Close(file);
				return false;
			}

			if (surf->lightmap_s == -1 ||
				surf->lightmap_t == -1) {

				R_AtlasPacker_FreePacker(&packer);
				Fs_Close(file);
				return false;
			}

			list = list->next;
		}

		// upload!
		R_UploadPackedLightmaps(packer_header.width, packer_header.height, bsp, start, list, NULL, 0);

		// reset for next round
		start = list;

		R_AtlasPacker_FreePacker(&packer);
	}

	Fs_Close(file);

	g_slist_free(r_lightmap_state.blocks);
	r_lightmap_state.blocks = NULL;

	return true;
}

/**
 * @brief
 */
void R_EndBspSurfaceLightmaps(r_bsp_model_t *bsp) {

	// sort all the lightmap blocks
	r_lightmap_state.blocks = g_slist_sort(r_lightmap_state.blocks, R_InsertBlock_CompareFunc);

	// check if the cache file exists
	if (R_LoadBspSurfaceLightmapCache(bsp)) {
		return;
	}

	// otherwise we're loading a new one.
	// make packers and start packin!
	r_bsp_surface_t *surf = (r_bsp_surface_t *) r_lightmap_state.blocks->data;

	r_packer_t packer;
	memset(&packer, 0, sizeof(packer));

	R_AtlasPacker_InitPacker(&packer, Min(r_config.max_texture_size, USHRT_MAX), Min(r_config.max_texture_size, USHRT_MAX), surf->lightmap_size[0],
	                         surf->lightmap_size[1], bsp->num_surfaces / 2);

	GSList *start = r_lightmap_state.blocks;

	uint32_t current_width = 0, current_height = 0, num_packers = 1, num_surfs = 0;
	r_lightmap_state.cache_file = NULL;

	if (r_lmcache->integer) {
		char filename[MAX_QPATH];
		g_snprintf(filename, sizeof(filename), "lmcache/%s.lmc", bsp->cm->name);

		r_lightmap_state.cache_file = Fs_OpenWrite(filename);

		if (r_lightmap_state.cache_file) {

			// write initial header; num_packers will be substituted in later on.
			// use -1 so that if this doesn't finish for some reason, it'll be picked up
			// by the read function as invalid.
			Fs_Write(r_lightmap_state.cache_file, &(const r_lmcache_header_t) {
				.magic = R_LMCACHE_MAGIC,
				.size = bsp->cm->size,
				.num_packers = (uint32_t) -1
			}, sizeof(r_lmcache_header_t), 1);
		}
	}

	for (GSList *list = r_lightmap_state.blocks; ; list = list->next) {

		if (list == NULL) {
			// Upload
			R_UploadPackedLightmaps(current_width, current_height, bsp, start, list, &packer, num_surfs);
			break;
		}

		surf = (r_bsp_surface_t *) list->data;

		r_packer_node_t *node;

		do {
			node = R_AtlasPacker_FindNode(&packer, packer.root, surf->lightmap_size[0], surf->lightmap_size[1]);

			if (node != NULL) {
				node = R_AtlasPacker_SplitNode(&packer, node, surf->lightmap_size[0], surf->lightmap_size[1]);
			} else {
				node = R_AtlasPacker_GrowNode(&packer, surf->lightmap_size[0], surf->lightmap_size[1]);
			}

			// can't fit any more, so upload and initialize a new packer
			if (node == NULL) {

				// Upload
				R_UploadPackedLightmaps(current_width, current_height, bsp, start, list, &packer, num_surfs);

				// reinitialize packer
				R_AtlasPacker_InitPacker(&packer, Min(r_config.max_texture_size, USHRT_MAX), Min(r_config.max_texture_size, USHRT_MAX),
				                         surf->lightmap_size[0], surf->lightmap_size[1], bsp->num_surfaces / 2);

				num_packers++;

				// new start position
				start = list;

				// reset accumulators
				current_width = current_height = num_surfs = 0;
			} else {
				uint32_t w = node->x + surf->lightmap_size[0],
				         h = node->y + surf->lightmap_size[1];

				w = NearestMultiple(w, 4);
				h = NearestMultiple(h, 4);

				// fit good, update current sizes
				current_width = Max(current_width, w);
				current_height = Max(current_height, h);

				// update surface parameters
				surf->lightmap_s = node->x;
				surf->lightmap_t = node->y;

				num_surfs++;
			}
		} while (node == NULL);
	}

	// free stuff
	R_AtlasPacker_FreePacker(&packer);

	g_slist_free(r_lightmap_state.blocks);
	r_lightmap_state.blocks = NULL;

	if (r_lightmap_state.cache_file) {

		// write final packer #
		Fs_Seek(r_lightmap_state.cache_file, offsetof(r_lmcache_header_t, num_packers));
		Fs_Write(r_lightmap_state.cache_file, &num_packers, sizeof(num_packers), 1);

		Fs_Close(r_lightmap_state.cache_file);
		r_lightmap_state.cache_file = NULL;
	}
}
