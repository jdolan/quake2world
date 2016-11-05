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

#include "quemap.h"
#include "qbsp.h"

vec_t microvolume = 0.125;
_Bool noprune = false;
_Bool nodetail = false;
_Bool fulldetail = false;
_Bool onlyents = false;
_Bool nomerge = false;
_Bool nowater = false;
_Bool nofill = false;
_Bool nocsg = false;
_Bool noweld = false;
_Bool noshare = false;
_Bool nosubdivide = false;
_Bool notjunc = false;
_Bool noopt = false;
_Bool leaktest = false;
_Bool verboseentities = false;

int32_t block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;

int32_t entity_num;

static node_t *block_nodes[10][10];

/**
 * @brief
 */
static node_t *BlockTree(int32_t xl, int32_t yl, int32_t xh, int32_t yh) {
	node_t *node;
	vec3_t normal;
	vec_t dist;
	int32_t mid;

	if (xl == xh && yl == yh) {
		node = block_nodes[xl + 5][yl + 5];
		if (!node) { // return an empty leaf
			node = AllocNode();
			node->plane_num = PLANENUM_LEAF;
			node->contents = 0; //CONTENTS_SOLID;
			return node;
		}
		return node;
	}
	// create a separator along the largest axis
	node = AllocNode();

	if (xh - xl > yh - yl) { // split x axis
		mid = xl + (xh - xl) / 2 + 1;
		normal[0] = 1;
		normal[1] = 0;
		normal[2] = 0;
		dist = mid * 1024;
		node->plane_num = FindPlane(normal, dist);
		node->children[0] = BlockTree(mid, yl, xh, yh);
		node->children[1] = BlockTree(xl, yl, mid - 1, yh);
	} else {
		mid = yl + (yh - yl) / 2 + 1;
		normal[0] = 0;
		normal[1] = 1;
		normal[2] = 0;
		dist = mid * 1024;
		node->plane_num = FindPlane(normal, dist);
		node->children[0] = BlockTree(xl, mid, xh, yh);
		node->children[1] = BlockTree(xl, yl, xh, mid - 1);
	}

	return node;
}

/**
 * @brief
 */
static int32_t brush_start, brush_end;
static void ProcessBlock_Thread(int32_t blocknum) {
	int32_t xblock, yblock;
	vec3_t mins, maxs;
	bsp_brush_t *brushes;
	tree_t *tree;
	node_t *node;

	yblock = block_yl + blocknum / (block_xh - block_xl + 1);
	xblock = block_xl + blocknum % (block_xh - block_xl + 1);

	Com_Verbose("############### block %2i,%2i ###############\n", xblock, yblock);

	mins[0] = xblock * 1024;
	mins[1] = yblock * 1024;
	mins[2] = MIN_WORLD_COORD;
	maxs[0] = (xblock + 1) * 1024;
	maxs[1] = (yblock + 1) * 1024;
	maxs[2] = MAX_WORLD_COORD;

	ThreadLock();

	// the makelist and chopbrushes could be cached between the passes...
	brushes = MakeBspBrushList(brush_start, brush_end, mins, maxs);
	if (!brushes) {
		node = AllocNode();
		node->plane_num = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		block_nodes[xblock + 5][yblock + 5] = node;
		ThreadUnlock();
		return;
	}

	if (!nocsg) {
		brushes = ChopBrushes(brushes);
	}

	tree = BrushBSP(brushes, mins, maxs);

	ThreadUnlock();
	block_nodes[xblock + 5][yblock + 5] = tree->head_node;
}

/**
 * @brief
 */
static void ProcessWorldModel(void) {
	entity_t *e;
	tree_t *tree;
	_Bool leaked;
	int32_t optimize;

	e = &entities[entity_num];

	brush_start = e->first_brush;
	brush_end = brush_start + e->num_brushes;
	leaked = false;

	// perform per-block operations
	if (block_xh * 1024 > map_maxs[0]) {
		block_xh = floor(map_maxs[0] / 1024.0);
	}
	if ((block_xl + 1) * 1024 < map_mins[0]) {
		block_xl = floor(map_mins[0] / 1024.0);
	}
	if (block_yh * 1024 > map_maxs[1]) {
		block_yh = floor(map_maxs[1] / 1024.0);
	}
	if ((block_yl + 1) * 1024 < map_mins[1]) {
		block_yl = floor(map_mins[1] / 1024.0);
	}

	if (block_xl < -4) {
		block_xl = -4;
	}
	if (block_yl < -4) {
		block_yl = -4;
	}
	if (block_xh > 3) {
		block_xh = 3;
	}
	if (block_yh > 3) {
		block_yh = 3;
	}

	for (optimize = 0; optimize <= 1; optimize++) {
		Com_Verbose("--------------------------------------------\n");

		RunThreadsOn((block_xh - block_xl + 1) * (block_yh - block_yl + 1), !verbose,
		             ProcessBlock_Thread);

		// build the division tree
		// oversizing the blocks guarantees that all the boundaries
		// will also get nodes.

		Com_Verbose("--------------------------------------------\n");

		tree = AllocTree();
		tree->head_node = BlockTree(block_xl - 1, block_yl - 1, block_xh + 1, block_yh + 1);

		tree->mins[0] = (block_xl) * 1024;
		tree->mins[1] = (block_yl) * 1024;
		tree->mins[2] = map_mins[2] - 8;

		tree->maxs[0] = (block_xh + 1) * 1024;
		tree->maxs[1] = (block_yh + 1) * 1024;
		tree->maxs[2] = map_maxs[2] + 8;

		// perform the global operations
		MakeTreePortals(tree);

		if (FloodEntities(tree)) {
			FillOutside(tree->head_node);
		} else {
			leaked = true;
			LeakFile(tree);

			if (leaktest) {
				Com_Error(ERR_FATAL, "!--- MAP LEAKED, ABORTING LEAKTEST ---\n");
			}
			Com_Verbose("**** leaked ****\n");
		}

		MarkVisibleSides(tree, brush_start, brush_end);
		if (noopt || leaked) {
			break;
		}
		if (!optimize) {
			FreeTree(tree);
		}
	}

	FloodAreas(tree);
	MakeFaces(tree->head_node);
	FixTjuncs(tree->head_node);

	if (!noprune) {
		PruneNodes(tree->head_node);
	}

	WriteBSP(tree->head_node);

	if (!leaked) {
		WritePortalFile(tree);
	}

	FreeTree(tree);
}

/**
 * @brief
 */
static void ProcessSubModel(void) {
	entity_t *e;
	int32_t start, end;
	tree_t *tree;
	bsp_brush_t *list;
	vec3_t mins, maxs;

	e = &entities[entity_num];

	start = e->first_brush;
	end = start + e->num_brushes;

	mins[0] = mins[1] = mins[2] = MIN_WORLD_COORD;
	maxs[0] = maxs[1] = maxs[2] = MAX_WORLD_COORD;
	list = MakeBspBrushList(start, end, mins, maxs);
	if (!nocsg) {
		list = ChopBrushes(list);
	}
	tree = BrushBSP(list, mins, maxs);
	MakeTreePortals(tree);
	MarkVisibleSides(tree, start, end);
	MakeFaces(tree->head_node);
	FixTjuncs(tree->head_node);
	WriteBSP(tree->head_node);
	FreeTree(tree);
}

/**
 * @brief
 */
static void ProcessModels(void) {
	BeginBSPFile();

	for (entity_num = 0; entity_num < num_entities; entity_num++) {
		if (!entities[entity_num].num_brushes) {
			continue;
		}

		Com_Verbose("############### model %i ###############\n", d_bsp.num_models);
		BeginModel();
		if (entity_num == 0) {
			ProcessWorldModel();
		} else {
			ProcessSubModel();
		}
		EndModel();
	}

	EndBSPFile();
}

/**
 * @brief
 */
int32_t BSP_Main(void) {
	char base[MAX_OS_PATH];

	Com_Print("\n----- BSP -----\n\n");

	const time_t start = time(NULL);

	StripExtension(map_name, base);

	// clear the whole bsp structure
	memset(&d_bsp, 0, sizeof(d_bsp));

	// delete portal and line files
	remove(va("%s.prt", base));
	remove(va("%s.lin", base));

	// if onlyents, just grab the entities and re-save
	if (onlyents) {

		LoadBSPFile(bsp_name);
		num_entities = 0;

		LoadMapFile(map_name);
		SetModelNumbers();

		UnparseEntities();

		WriteBSPFile(bsp_name);
	} else {
		// start from scratch
		LoadMapFile(map_name);
		SetModelNumbers();

		ProcessModels();
	}

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nBSP Time: ");
	if (duration > 59) {
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	}
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
