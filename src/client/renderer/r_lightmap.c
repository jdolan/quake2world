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
	r_image_t *lightmap;
	r_image_t *deluxemap;

	r_pixel_t block_size; // lightmap block size (NxN)
	r_pixel_t *allocated; // block availability

	byte *sample_buffer; // RGB buffers for uploading
	byte *direction_buffer;
} r_lightmap_state_t;

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
static r_image_t *R_AllocLightmap_(r_image_type_t type) {
	static uint32_t count;
	char name[MAX_QPATH];

	const char *base = (type == IT_LIGHTMAP ? "lightmap" : "deluxemap");
	g_snprintf(name, sizeof(name), "%s %u", base, count++);

	r_image_t *image = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);

	image->media.Free = R_FreeLightmap;

	image->type = type;
	image->width = image->height = r_lightmap_state.block_size;

	return image;
}

#define R_AllocLightmap() R_AllocLightmap_(IT_LIGHTMAP)
#define R_AllocDeluxemap() R_AllocLightmap_(IT_DELUXEMAP)

/**
 * @brief
 */
static void R_UploadLightmapBlock(r_bsp_model_t *bsp) {

	R_UploadImage(r_lightmap_state.lightmap, GL_RGB, r_lightmap_state.sample_buffer);
	R_UploadImage(r_lightmap_state.deluxemap, GL_RGB, r_lightmap_state.direction_buffer);

	// clear the allocation block and buffers
	memset(r_lightmap_state.allocated, 0, r_lightmap_state.block_size * sizeof(r_pixel_t));
	memset(r_lightmap_state.sample_buffer, 0, r_lightmap_state.block_size * r_lightmap_state.block_size * 3);
	memset(r_lightmap_state.direction_buffer, 0, r_lightmap_state.block_size * r_lightmap_state.block_size * 3);
}

/**
 * @brief
 */
static _Bool R_AllocLightmapBlock(r_pixel_t w, r_pixel_t h, r_pixel_t *x, r_pixel_t *y) {
	r_pixel_t i, j;
	r_pixel_t best;

	best = r_lightmap_state.block_size;

	for (i = 0; i < r_lightmap_state.block_size - w; i++) {
		r_pixel_t best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmap_state.allocated[i + j] >= best) {
				break;
			}

			if (r_lightmap_state.allocated[i + j] > best2) {
				best2 = r_lightmap_state.allocated[i + j];
			}
		}
		if (j == w) { // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > r_lightmap_state.block_size) {
		return false;
	}

	for (i = 0; i < w; i++) {
		r_lightmap_state.allocated[*x + i] = best + h;
	}

	return true;
}

/**
 * @brief
 */
static void R_BuildDefaultLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, byte *sout,
                                   byte *dout, size_t stride) {

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmaps->scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmaps->scale) + 1;

	stride -= (smax * 3);

	for (r_pixel_t i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (r_pixel_t j = 0; j < smax; j++) {

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
static void R_FilterLightmap(r_pixel_t width, r_pixel_t height, byte *lightmap) {
	r_image_t image;

	memset(&image, 0, sizeof(image));

	image.type = IT_LIGHTMAP;

	image.width = width;
	image.height = height;

	R_FilterImage(&image, GL_RGB, lightmap);
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

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmaps->scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmaps->scale) + 1;

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

	for (r_pixel_t t = 0; t < tmax; t++, lout += stride, dout += stride) {
		for (r_pixel_t s = 0; s < smax; s++) {

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
 * @brief Creates the lightmap and deluxemap for the specified surface. The GL
 * textures to which the lightmap and deluxemap are written is stored on the
 * surface.
 *
 * @param data If NULL, a default lightmap and deluxemap will be generated.
 */
void R_CreateBspSurfaceLightmap(r_bsp_model_t *bsp, r_bsp_surface_t *surf, const byte *data) {

	if (!(surf->flags & R_SURF_LIGHTMAP)) {
		return;
	}

	const r_pixel_t smax = (surf->st_extents[0] / bsp->lightmaps->scale) + 1;
	const r_pixel_t tmax = (surf->st_extents[1] / bsp->lightmaps->scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->lightmap_s, &surf->lightmap_t)) {

		R_UploadLightmapBlock(bsp); // upload the last block

		r_lightmap_state.lightmap = R_AllocLightmap();
		r_lightmap_state.deluxemap = R_AllocDeluxemap();

		if (!R_AllocLightmapBlock(smax, tmax, &surf->lightmap_s, &surf->lightmap_t)) {
			Com_Error(ERR_DROP, "Consecutive calls to R_AllocLightmapBlock failed");
		}
	}

	surf->lightmap = r_lightmap_state.lightmap;
	surf->deluxemap = r_lightmap_state.deluxemap;

	byte *sout = r_lightmap_state.sample_buffer;
	sout += (surf->lightmap_t *r_lightmap_state.block_size + surf->lightmap_s) * 3;

	byte *dout = r_lightmap_state.direction_buffer;
	dout += (surf->lightmap_t *r_lightmap_state.block_size + surf->lightmap_s) * 3;

	const size_t stride = r_lightmap_state.block_size * 3;

	if (data) {
		R_BuildLightmap(bsp, surf, data, sout, dout, stride);
	} else {
		R_BuildDefaultLightmap(bsp, surf, sout, dout, stride);
	}
}

/**
 * @brief
 */
void R_BeginBspSurfaceLightmaps(r_bsp_model_t *bsp) {

	// users can tune max lightmap size for their card
	r_lightmap_state.block_size = r_lightmap_block_size->integer;

	// but clamp it to the card's capability to avoid errors
	r_lightmap_state.block_size = Clamp(r_lightmap_state.block_size, 256, (r_pixel_t) r_config.max_texture_size);

	Com_Debug("Block size: %d (max: %d)\n", r_lightmap_state.block_size, r_config.max_texture_size);

	const r_pixel_t bs = r_lightmap_state.block_size;

	r_lightmap_state.allocated = Mem_TagMalloc(bs * sizeof(r_pixel_t), MEM_TAG_RENDERER);

	r_lightmap_state.sample_buffer = Mem_TagMalloc(bs * bs * sizeof(uint32_t), MEM_TAG_RENDERER);
	r_lightmap_state.direction_buffer = Mem_TagMalloc(bs * bs * sizeof(uint32_t), MEM_TAG_RENDERER);

	// generate the initial textures for lightmap data
	r_lightmap_state.lightmap = R_AllocLightmap();
	r_lightmap_state.deluxemap = R_AllocDeluxemap();
}

/**
 * @brief
 */
void R_EndBspSurfaceLightmaps(r_bsp_model_t *bsp) {

	// upload the pending lightmap block
	R_UploadLightmapBlock(bsp);

	Mem_Free(r_lightmap_state.allocated);

	Mem_Free(r_lightmap_state.sample_buffer);
	Mem_Free(r_lightmap_state.direction_buffer);
}
