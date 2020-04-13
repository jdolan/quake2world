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

r_image_state_t r_image_state;

typedef struct {
	const char *name;
	GLenum minify, magnify;
} r_texture_mode_t;

static r_texture_mode_t r_texture_modes[] = { // specifies mipmapping (texture quality)
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

/**
 * @brief Sets the texture parameters for mipmapping and anisotropy.
 */
static void R_TextureMode(void) {
	size_t i;

	for (i = 0; i < lengthof(r_texture_modes); i++) {
		if (!g_ascii_strcasecmp(r_texture_modes[i].name, r_texture_mode->string)) {
			break;
		}
	}

	if (i == lengthof(r_texture_modes)) {
		Com_Warn("Bad filter name: %s\n", r_texture_mode->string);
		return;
	}

	r_image_state.filter_min = r_texture_modes[i].minify;
	r_image_state.filter_mag = r_texture_modes[i].magnify;

	if (r_anisotropy->value) {
		if (GLAD_GL_EXT_texture_filter_anisotropic) {
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_image_state.anisotropy);
		} else {
			Com_Warn("Anisotropy is enabled but not supported by your GPU.\n");
			Cvar_ForceSetInteger(r_anisotropy->name, 0);
		}
	} else {
		r_image_state.anisotropy = 1.0;
	}
}

#define MAX_SCREENSHOTS 1000

typedef struct {
	uint32_t width;
	uint32_t height;
	byte *buffer;
} r_screenshot_t;

/**
 * @brief ThreadRunFunc for R_Screenshot_f.
 */
static void R_Screenshot_f_encode(void *data) {
	static int32_t last_screenshot;
	char filename[MAX_QPATH];
	int32_t i;

	for (i = last_screenshot; i < MAX_SCREENSHOTS; i++) {
		g_snprintf(filename, sizeof(filename), "screenshots/quetoo%03u.%s", i, r_screenshot_format->string);

		if (!Fs_Exists(filename)) {
			break;
		}
	}

	if (i == MAX_SCREENSHOTS) {
		Com_Warn("MAX_SCREENSHOTS exceeded\n");
		return;
	}

	last_screenshot = i;

	r_screenshot_t *s = (r_screenshot_t *) data;
	_Bool screenshot_saved;

	if (!g_strcmp0(r_screenshot_format->string, "tga")) {
		screenshot_saved = Img_WriteTGA(filename, s->buffer, s->width, s->height);
	} else {
		screenshot_saved = Img_WritePNG(filename, s->buffer, s->width, s->height);
	}

	if (screenshot_saved) {
		Com_Print("Saved %s\n", Basename(filename));
	} else {
		Com_Warn("Failed to write %s\n", filename);
	}

	Mem_Free(s);
}

/**
* @brief Captures a screenshot, writing it to the user's directory.
*/
void R_Screenshot_f(void) {

	r_screenshot_t *s = Mem_Malloc(sizeof(r_screenshot_t));

	s->width = r_context.drawable_width;
	s->height = r_context.drawable_height;

	s->buffer = Mem_LinkMalloc(s->width * s->height * 3, s);

	glReadPixels(0, 0, s->width, s->height, GL_BGR, GL_UNSIGNED_BYTE, s->buffer);

	Thread_Create(R_Screenshot_f_encode, s, THREAD_NO_WAIT);
}

/**
 * @brief Uploads the specified image to the OpenGL implementation. Images that
 * do not have a GL texture reserved (which is most diffusemap textures) will have
 * one generated for them. This flexibility allows for explicitly managed
 * textures (such as lightmaps) to be here as well.
 */
void R_UploadImage(r_image_t *image, GLenum format, byte *data) {

	assert(image);

	if (image->texnum == 0) {
		glGenTextures(1, &(image->texnum));
	}

	GLenum target = GL_TEXTURE_2D;

	if (image->depth) {
		switch (image->type) {
			case IT_MATERIAL:
				target = GL_TEXTURE_2D_ARRAY;
				break;
			case IT_LIGHTMAP:
				target = GL_TEXTURE_2D_ARRAY;
				break;
			case IT_LIGHTGRID:
				target = GL_TEXTURE_3D;
				break;
			default:
				break;
		}
	}

	glBindTexture(target, image->texnum);

	switch (format) {
		case GL_RGB:
		case GL_RGBA:
			break;
		default:
			Com_Error(ERROR_DROP, "Unsupported format %d\n", format);
			break;
	}

	const GLenum type = GL_UNSIGNED_BYTE;

	if (image->type & IT_MASK_MIPMAP) {
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, r_image_state.filter_min);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);

		if (r_image_state.anisotropy) {
			glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_image_state.anisotropy);
		}
	} else {
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, r_image_state.filter_mag);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, r_image_state.filter_mag);
	}

	if (image->type & IT_MASK_CLAMP_EDGE) {
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	} else {
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_REPEAT);
	}

	if (GLAD_GL_ARB_texture_storage) {
		const GLenum internal_format = (format == GL_RGBA) ? GL_RGBA8 : GL_RGB8;

		if (image->depth) {
			glTexStorage3D(target, (image->type & IT_MASK_MIPMAP) ? (floor(log2(MAX(MAX(image->width, image->height), image->depth))) + 1) : 1, internal_format, image->width, image->height, image->depth);
			glTexSubImage3D(target, 0, 0, 0, 0, image->width, image->height, image->depth, format, type, data);
		} else {
			glTexStorage2D(target, (image->type & IT_MASK_MIPMAP) ? (floor(log2(MAX(image->width, image->height))) + 1) : 1, internal_format, image->width, image->height);
			glTexSubImage2D(target, 0, 0, 0, image->width, image->height, format, type, data);
		}
	} else {
		if (image->depth) {
			glTexImage3D(target, 0, format, image->width, image->height, image->depth, 0, format, type, data);
		} else {
			glTexImage2D(target, 0, format, image->width, image->height, 0, format, type, data);
		}
	}

	if (image->type & IT_MASK_MIPMAP) {
		glGenerateMipmap(target);
	}

	R_RegisterMedia((r_media_t *) image);

	R_GetError(image->media.name);
}

/**
 * @brief Retain event listener for images.
 */
_Bool R_RetainImage(r_media_t *self) {
	const r_image_type_t type = ((r_image_t *) self)->type;

	if (type == IT_NULL || type == IT_PROGRAM || type == IT_FONT || type == IT_UI) {
		return true;
	}

	return false;
}

/**
 * @brief Free event listener for images.
 */
void R_FreeImage(r_media_t *media) {

	r_image_t *image = (r_image_t *) media;

	glDeleteTextures(1, &image->texnum);

	image->texnum = 0;
}

/**
 * @brief Loads the image by the specified name.
 */
r_image_t *R_LoadImage(const char *name, r_image_type_t type) {
	r_image_t *image;
	char key[MAX_QPATH];

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	StripExtension(name, key);

	if (!(image = (r_image_t *) R_FindMedia(key))) {

		SDL_Surface *surf = Img_LoadSurface(key);
		if (surf) {
			image = (r_image_t *) R_AllocMedia(key, sizeof(r_image_t), MEDIA_IMAGE);

			image->media.Retain = R_RetainImage;
			image->media.Free = R_FreeImage;

			image->width = surf->w;
			image->height = surf->h;
			image->type = type;

			R_UploadImage(image, GL_RGBA, surf->pixels);

			SDL_FreeSurface(surf);
		} else {
			Com_Debug(DEBUG_RENDERER, "Couldn't load %s\n", key);
			image = r_image_state.null;
		}
	}

	return image;
}

#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

/**
 * @brief Dump the image to the specified output file (must be .png)
 */
void R_DumpImage(const r_image_t *image, const char *output) {

	const char *real_path = Fs_RealPath(output);
	char real_dir[MAX_QPATH];
	Dirname(output, real_dir);
	Fs_Mkdir(real_dir);

	SDL_RWops *f = SDL_RWFromFile(real_path, "wb");
	if (!f) {
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	GLenum target;
	switch (image->type) {
		case IT_LIGHTMAP:
			target = GL_TEXTURE_2D_ARRAY;
			break;
		case IT_LIGHTGRID:
			target = GL_TEXTURE_3D;
			break;
		default:
			target = GL_TEXTURE_2D;
			break;
	}

	glBindTexture(target, image->texnum);

	int32_t width, height, depth;

	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &height);
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, &depth);

	GLubyte *pixels = Mem_Malloc(width * height * depth * 4);

	glGetTexImage(target, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * 4, RMASK, GMASK, BMASK, AMASK);
	IMG_SavePNG_RW(surf, f, 0);
	SDL_FreeSurface(surf);

	Mem_Free(pixels);
	SDL_RWclose(f);
}

/**
 * @brief R_MediaEnumerator for R_DumpImages_f.
 */
static void R_DumpImages_enumerator(const r_media_t *media, void *data) {

	if (media->type == MEDIA_IMAGE) {
		const r_image_t *image = (const r_image_t *) media;
		char path[MAX_OS_PATH];

		g_snprintf(path, sizeof(path), "imgdmp/%s %i.png", media->name, image->texnum);

		R_DumpImage(image, path);
	}
}

/**
 * @brief
 */
void R_DumpImages_f(void) {

	Com_Print("Dumping media... ");

	Fs_Mkdir("imgdmp");

	R_EnumerateMedia(R_DumpImages_enumerator, NULL);
}

/**
 * @brief Initializes the null (default) image, used when the desired texture
 * is not available.
 */
static void R_InitNullImage(void) {

	r_image_state.null = (r_image_t *) R_AllocMedia("r_image_state.null", sizeof(r_image_t), MEDIA_IMAGE);

	r_image_state.null->media.Retain = R_RetainImage;
	r_image_state.null->media.Free = R_FreeImage;

	r_image_state.null->width = r_image_state.null->height = 1;
	r_image_state.null->type = IT_NULL;

	byte data[1 * 1 * 3];
	memset(&data, 0xff, sizeof(data));

	R_UploadImage(r_image_state.null, GL_RGB, data);
}

/**
 * @brief Initializes the mesh shell image.
 */
static void R_InitShellImage(void) {
	r_image_state.shell = R_LoadImage("envmaps/envmap_3", IT_PROGRAM);
}

/**
 * @brief Initializes the images facilities, which includes generation of
 */
void R_InitImages(void) {

	// set up alignment parameters.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	memset(&r_image_state, 0, sizeof(r_image_state));

	R_TextureMode();

	R_InitNullImage();

	R_InitShellImage();

	R_GetError(NULL);
	
	Fs_Mkdir("screenshots");
}
