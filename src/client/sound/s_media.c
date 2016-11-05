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

#include "s_local.h"

typedef struct {
	GHashTable *media;
	GList *keys;
	int32_t seed; // for freeing stale assets
} s_media_state_t;

static s_media_state_t s_media_state;

/**
 * @brief Prints information about all currently loaded media to the console.
 */
void S_ListMedia_f(void) {

	Com_Print("Loaded media:\n");

	GList *key = s_media_state.keys;
	while (key) {
		s_media_t *media = g_hash_table_lookup(s_media_state.media, key->data);

		Com_Print("%s\n", media->name);

		key = key->next;
	}
}

/**
 * @brief GCompareFunc for S_RegisterMedia. Sorts media by name.
 */
static int32_t S_RegisterMedia_Compare(gconstpointer name1, gconstpointer name2) {
	return g_strcmp0((const char *) name1, (const char *) name2);
}

static gboolean S_FreeMedia_(gpointer key, gpointer value, gpointer data);

/**
 * @brief Inserts the specified media into the shared table.
 */
void S_RegisterMedia(s_media_t *media) {

	// check to see if we're already seeded
	if (media->seed != s_media_state.seed) {
		s_media_t *m;

		if ((m = g_hash_table_lookup(s_media_state.media, media->name))) {
			if (m != media) {
				Com_Debug("Replacing %s\n", media->name);
				S_FreeMedia_(NULL, m, m);
				g_hash_table_replace(s_media_state.media, media->name, media);
			} else {
				Com_Debug("Retaining %s\n", media->name);
			}
		} else {
			Com_Debug("Inserting %s\n", media->name);
			g_hash_table_insert(s_media_state.media, media->name, media);
		}

		s_media_state.keys = g_list_insert_sorted(s_media_state.keys, media->name,
		                     S_RegisterMedia_Compare);

		// re-seed the media to retain it
		media->seed = s_media_state.seed;
	}
}

/**
 * @brief Resolves the specified media if it is already known. The returned
 * media is re-registered for convenience.
 *
 * @return s_media_t The media, or `NULL`.
 */
s_media_t *S_FindMedia(const char *name) {
	s_media_t *media;

	if ((media = g_hash_table_lookup(s_media_state.media, name))) {
		S_RegisterMedia(media);
	}

	return media;
}

/**
 * @brief Returns a newly allocated s_media_t with the specified name.
 *
 * @param size_t size The number of bytes to allocate for the media.
 *
 * @return The newly initialized media.
 */
s_media_t *S_AllocMedia(const char *name, size_t size) {

	if (!name || !*name) {
		Com_Error(ERR_DROP, "NULL name\n");
	}

	s_media_t *media = Mem_TagMalloc(size, MEM_TAG_SOUND);

	g_strlcpy(media->name, name, sizeof(media->name));

	return media;
}

/**
 * @brief GHRFunc for freeing media. If data is non-NULL, then the media is
 * always freed. Otherwise, only media with stale seed values and no explicit
 * retainment are released.
 */
static gboolean S_FreeMedia_(gpointer key, gpointer value, gpointer data) {
	s_media_t *media = (s_media_t *) value;

	if (!data) { // see if the media should be freed
		if (media->seed == s_media_state.seed || (media->Retain && media->Retain(media))) {
			return false;
		}
	}

	Com_Debug("Freeing %s\n", media->name);

	// ask the implementation to clean up
	if (media->Free) {
		media->Free(media);
	}

	s_media_state.keys = g_list_remove(s_media_state.keys, key);

	return true;
}

/**
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void S_FreeMedia(void) {
	g_hash_table_foreach_remove(s_media_state.media, S_FreeMedia_, NULL);
}

/**
 * @brief Prepares the media subsystem for loading.
 */
void S_BeginLoading(void) {
	int32_t s;

	do {
		s = Random();
	} while (s == s_media_state.seed);

	s_media_state.seed = s;
}

/**
 * @brief Initializes the media pool.
 */
void S_InitMedia(void) {

	memset(&s_media_state, 0, sizeof(s_media_state));

	s_media_state.media = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Mem_Free);

	S_BeginLoading();
}

/**
 * @brief Shuts down the media pool.
 */
void S_ShutdownMedia(void) {

	g_hash_table_foreach_remove(s_media_state.media, S_FreeMedia_, (void *) true);

	g_hash_table_destroy(s_media_state.media);
	g_list_free(s_media_state.keys);
}

