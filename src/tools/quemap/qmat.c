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

#include "qmat.h"

/**
 * @brief Parses the materials and map files, merging all known materials, and 
 * serializes them back to the materials file for the current map.
 */
int32_t MAT_Main(void) {

	Com_Print("\n----- MAT -----\n\n");

	const time_t start = time(NULL);

	// clear the whole bsp structure
	memset(&bsp_file, 0, sizeof(bsp_file));

	Bsp_AllocLump(&bsp_file, BSP_LUMP_TEXINFO, MAX_BSP_TEXINFO);

	LoadMaterials();

	LoadMapFile(map_name);

	for (int32_t i = 0; i < bsp_file.num_texinfo; i++) {
		LoadMaterial(bsp_file.texinfo[i].texture);
	}

	UnloadScriptFiles();

	WriteMaterialsFile(mat_name);

	FreeMaterials();

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nMaterials time: ");
	if (duration > 59) {
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	}
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
