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

#include "brush.h"
#include "bsp.h"
#include "map.h"
#include "material.h"
#include "qbsp.h"
#include "texinfo.h"

int32_t num_entities;
entity_t entities[MAX_BSP_ENTITIES];

int32_t num_brushes;
brush_t brushes[MAX_BSP_BRUSHES];

static int32_t num_brush_sides;
static brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES];

int32_t num_planes;
plane_t planes[MAX_BSP_PLANES];

#define	PLANE_HASHES (size_t) MAX_WORLD_COORD
static plane_t *plane_hash[PLANE_HASHES];

box3_t map_bounds;

#define	NORMAL_EPSILON	0.0001
#define	DIST_EPSILON	0.005

/**
 * @brief
 */
static _Bool PlaneEqual(const plane_t *p, const vec3_t normal, double dist) {

	if (EqualEpsilon(p->dist, dist, DIST_EPSILON) &&
		Vec3_EqualEpsilon(p->normal, normal, NORMAL_EPSILON)) {
		return true;
	}

	return false;
}

/**
 * @brief
 */
static inline void AddPlaneToHash(plane_t *p) {

	const int32_t hash = ((int32_t) fabs(p->dist)) & (PLANE_HASHES - 1);

	p->hash_chain = plane_hash[hash];
	plane_hash[hash] = p;
}

/**
 * @brief
 */
static int32_t CreatePlane(const vec3_t normal, double dist) {

	// bad plane
	if (Vec3_Length(normal) < 1.f - FLT_EPSILON) {
		Com_Error(ERROR_FATAL, "Malformed plane\n");
	}

	// create a new plane
	if (num_planes + 2 > MAX_BSP_PLANES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_PLANES\n");
	}

	plane_t *a = &planes[num_planes++];
	a->normal = normal;
	a->dist = dist;
	a->type = Cm_PlaneTypeForNormal(a->normal);

	plane_t *b = &planes[num_planes++];
	b->normal = Vec3_Negate(normal);
	b->dist = -dist;
	b->type = Cm_PlaneTypeForNormal(b->normal);

	// always put axial planes facing positive first
	if (AXIAL(a)) {
		if (Cm_SignBitsForNormal(a->normal)) {
			plane_t temp = *a;
			*a = *b;
			*b = temp;

			AddPlaneToHash(a);
			AddPlaneToHash(b);
			return num_planes - 1;
		}
	}

	AddPlaneToHash(a);
	AddPlaneToHash(b);
	return num_planes - 2;
}

/**
 * @brief If the specified normal is very close to an axis, align with it.
 */
static vec3_t SnapNormal(const vec3_t normal) {

	vec3_t snapped = normal;

	_Bool snap = false;
	for (int32_t i = 0; i < 3; i++) {
		if (snapped.xyz[i] != 0.f) {
			if (fabsf(snapped.xyz[i]) < NORMAL_EPSILON) {
				snapped.xyz[i] = 0.f;
				snap = true;
			}
		}
	}

	if (snap) {
		snapped = Vec3_Normalize(snapped);
	}

	return snapped;
}

/**
 * @brief Snaps normals within NORMAL_EPSILON to axial, and snaps axial plane
 * distances to integers, rounding towards the origin.
 */
static void SnapPlane(vec3_t *normal, double *dist) {

	*normal = SnapNormal(*normal);

	// snap axial planes to integer distances
	if (Cm_PlaneTypeForNormal(*normal) <= PLANE_Z) {
		const double d = floor(*dist + 0.5);
		if (fabs(*dist - d) < DIST_EPSILON) {
			*dist = d;
		}
	}
}

/**
 * @brief
 */
int32_t FindPlane(const vec3_t normal, double dist) {

	vec3_t snapped = normal;
	SnapPlane(&snapped, &dist);

	const int32_t hash = ((int32_t) fabs(dist)) & (PLANE_HASHES - 1);

	// search the adjacent bins as well
	for (int32_t i = -1; i <= 1; i++) {
		const int32_t h = (hash + i) & (PLANE_HASHES - 1);
		
		const plane_t *p = plane_hash[h];
		while (p) {
			if (PlaneEqual(p, snapped, dist)) {
				return (int32_t) (ptrdiff_t) (p - planes);
			}
			p = p->hash_chain;
		}
	}

	return CreatePlane(snapped, dist);
}

/**
 * @brief
 */
static int32_t PlaneFromPoints(const vec3d_t p0, const vec3d_t p1, const vec3d_t p2) {

	const vec3d_t t1 = Vec3d_Subtract(p0, p1);
	const vec3d_t t2 = Vec3d_Subtract(p2, p1);

	const vec3d_t normal = Vec3d_Normalize(Vec3d_Cross(t1, t2));
	const double dist = Vec3d_Dot(p0, normal);

	return FindPlane(Vec3d_CastVec3(normal), dist);
}

/**
 * @brief
 */
static int32_t BrushContents(const brush_t *b) {

	int32_t contents = 0;

	{
		const brush_side_t *s = b->sides;
		for (int32_t i = 1; i < b->num_sides; i++, s++) {
			contents |= s->contents;
		}
	}

	// if we have a mix of solid and window, window wins
	if ((contents & CONTENTS_SOLID) && (contents & CONTENTS_WINDOW)) {
		brush_side_t *s = b->sides;
		for (int32_t i = 1; i < b->num_sides; i++, s++) {
			s->contents |= CONTENTS_WINDOW;
			s->contents &= ~CONTENTS_SOLID;
		}
	}

	{
		const brush_side_t *s = b->sides;
		contents = s->contents;

		for (int32_t i = 1; i < b->num_sides; i++, s++) {

			if ((s->contents & CONTENTS_MASK_VISIBLE) != (contents & CONTENTS_MASK_VISIBLE)) {
				char bits[33], bobs[33];

				SDL_itoa(s->contents & CONTENTS_MASK_VISIBLE, bits, 2);
				SDL_itoa(contents & CONTENTS_MASK_VISIBLE, bobs, 2);

				Mon_SendSelect(MON_WARN, b->entity_num, b->brush_num,
							   va("Mixed face contents: %s expected %s", bits, bobs));
				break;
			}
		}
	}

	return contents;
}

/**
 * @brief qsort comparator to sort a brushes sides by plane type. This ensures
 * the first 6 sides of the brush are axial.
 */
static int32_t SortBrushSides(const void *a, const void *b) {

	const brush_side_t *a_side = a;
	const brush_side_t *b_side = b;

	return planes[a_side->plane_num].type - planes[b_side->plane_num].type;
}

/**
 * @brief Adds any additional planes necessary to allow the brush to be expanded
 * against axial bounding boxes. Ensures that the first 6 sides of every brush
 * are axial, which allows some optimizations in collision detection.
 */
static void AddBrushBevels(brush_t *b) {

	for (int32_t axis = 0; axis < 3; axis++) {
		for (int32_t side = -1; side <= 1; side += 2) {

			vec3_t normal = Vec3_Zero();
			normal.xyz[axis] = side;

			float dist;
			if (side == -1) {
				dist = -b->bounds.mins.xyz[axis];
			} else {
				dist = b->bounds.maxs.xyz[axis];
			}

			const int32_t plane_num = FindPlane(normal, dist);

			int32_t j;
			for (j = 0; j < b->num_sides; j++) {
				if (b->sides[j].plane_num == plane_num) {
					break;
				}
			}

			if (j == b->num_sides) {
				if (num_brush_sides >= MAX_BSP_BRUSH_SIDES) {
					Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
				}

				brush_side_t *s = &b->sides[b->num_sides++];

				s->plane_num = plane_num;
				s->texinfo = BSP_TEXINFO_BEVEL;

				num_brush_sides++;
			}
		}
	}

	qsort(b->sides, b->num_sides, sizeof(brush_side_t), SortBrushSides);
}

/**
 * @brief Makes windings for sides and mins / maxs for the brush
 */
static void MakeBrushWindings(brush_t *brush) {

	assert(brush);
	assert(brush->num_sides);

	brush->bounds = Box3_Null();

	brush_side_t *side = brush->sides;
	for (int32_t i = 0; i < brush->num_sides; i++, side++) {

		if (side->texinfo == BSP_TEXINFO_BEVEL) {
			continue;
		}

		const plane_t *plane = &planes[side->plane_num];
		side->winding = Cm_WindingForPlane(plane->normal, plane->dist);

		const brush_side_t *s = brush->sides;
		for (int32_t j = 0; j < brush->num_sides; j++, s++) {
			if (side == s) {
				continue;
			}
			if (s->texinfo == BSP_TEXINFO_BEVEL) {
				continue;
			}
			const plane_t *p = &planes[s->plane_num ^ 1];
			Cm_ClipWinding(&side->winding, p->normal, p->dist, SIDE_EPSILON);

			if (side->winding == NULL) {
				break;
			}
		}

		if (side->winding) {
			brush->bounds = Box3_Union(brush->bounds, Cm_WindingBounds(side->winding));
		} else {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Malformed brush");
			brush->num_sides = 0;
			return;
		}
	}

	for (int32_t i = 0; i < 3; i++) {
		//IDBUG: all the indexes into the mins and maxs were zero (not using i)
		if (brush->bounds.mins.xyz[i] < MIN_WORLD_COORD || brush->bounds.maxs.xyz[i] > MAX_WORLD_COORD) {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Brush bounds out of range");
			brush->num_sides = 0;
			return;
		}
		if (brush->bounds.mins.xyz[i] > MAX_WORLD_COORD || brush->bounds.maxs.xyz[i] < MIN_WORLD_COORD) {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "No visible sides on brush");
			brush->num_sides = 0;
			return;
		}
	}
}

/**
 * @brief
 */
static void SetMaterialFlags(brush_side_t *side) {

	const cm_material_t *material = LoadMaterial(side->texture, ASSET_CONTEXT_TEXTURES);
	if (material) {
		if (material->contents) {
			if (side->contents == 0) {
				side->contents = material->contents;
			}
		}
		if (material->surface) {
			if (side->surf == 0) {
				side->surf = material->surface;
			}
		}
		if (material->light) {
			if (side->value == 0) {
				side->value = material->light;
			}
		}
	}

	if (!g_strcmp0(side->texture, "common/caulk")) {
		side->surf |= SURF_NO_DRAW;
	} else if (!g_strcmp0(side->texture, "common/clip")) {
		side->contents |= CONTENTS_PLAYER_CLIP;
	} else if (!g_strcmp0(side->texture, "common/dust")) {
		side->contents |= CONTENTS_MIST;
		side->surf |= SURF_NO_DRAW;
	} else if (!g_strcmp0(side->texture, "common/fog")) {
		side->contents |= CONTENTS_MIST;
		side->surf |= SURF_NO_DRAW;
	} else if (!g_strcmp0(side->texture, "common/hint")) {
		side->surf |= SURF_HINT;
	} else if (!g_strcmp0(side->texture, "common/ladder")) {
		side->contents |= CONTENTS_LADDER | CONTENTS_WINDOW;
		side->surf |= SURF_NO_DRAW;
	} if (!g_strcmp0(side->texture, "common/monsterclip")) {
		side->contents |= CONTENTS_MONSTER_CLIP;
	} else if (!g_strcmp0(side->texture, "common/nodraw")) {
		side->surf |= SURF_NO_DRAW;
	} else if (!g_strcmp0(side->texture, "common/occlude")) {
		side->contents |= CONTENTS_OCCLUSION_QUERY;
		side->surf |= SURF_SKIP;
	} else if (!g_strcmp0(side->texture, "common/origin")) {
		side->contents |= CONTENTS_ORIGIN;
	} else if (!g_strcmp0(side->texture, "common/skip")) {
		side->surf |= SURF_SKIP;
	} else if (!g_strcmp0(side->texture, "common/sky")) {
		side->surf |= SURF_SKY;
	} else if (!g_strcmp0(side->texture, "common/trigger")) {
		side->contents |= CONTENTS_WINDOW;
		side->surf |= SURF_NO_DRAW;
	}

	if (side->contents & CONTENTS_MASK_LIQUID) {
		side->surf |= SURF_LIQUID;
	}
}

/**
 * @brief
 */
static void ParseBrush(parser_t *parser, entity_t *entity) {
	char token[MAX_TOKEN_CHARS];

	if (Parse_IsEOF(parser)) {
		return;
	}

	Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

	if (g_strcmp0(token, "{")) {
		return;
	}

	if (num_brushes == MAX_BSP_BRUSHES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_BRUSHES\n");
	}

	brush_t *brush = &brushes[num_brushes];

	brush->entity_num = (int32_t) (entity - entities);
	brush->brush_num = num_brushes - entity->first_brush;

	num_brushes++;
	entity->num_brushes++;

	brush->sides = &brush_sides[num_brush_sides];

	while (true) {

		if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
			Com_Error(ERROR_FATAL, "EOF without closing brush\n");
		}

		if (!g_strcmp0(token, "}")) {
			Parse_SkipToken(parser, PARSE_DEFAULT);
			break;
		}

		if (num_brush_sides == MAX_BSP_BRUSH_SIDES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
		}

		brush_side_t *side = &brush_sides[num_brush_sides];

		vec3d_t points[3];

		// read the three point plane definition
		for (int32_t i = 0; i < 3; i++) {

			Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
			if (g_strcmp0(token, "(")) {
				Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", num_brushes, token);
			}

			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].x, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].y, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].z, 1);

			Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
			if (g_strcmp0(token, ")")) {
				Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", num_brushes, token);
			}
		}

		// read the texturedef
		Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

		if (strlen(token) > sizeof(side->texture) - 1) {
			Com_Error(ERROR_FATAL, "Texture name \"%s\" is too long.\n", token);
		}

		g_strlcpy(side->texture, token, sizeof(side->texture));

		Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->shift.x, 1);
		Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->shift.y, 1);
		Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->rotate, 1);
		Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->scale.x, 1);
		Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->scale.y, 1);

		if (!Parse_IsEOL(parser)) {
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->contents, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->surf, 1);
			Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->value, 1);
		}

		// resolve material-based surface and contents flags
		SetMaterialFlags(side);

		// translucent faces are inherently details beacuse they can not occlude
		if ((side->surf & SURF_MASK_TRANSLUCENT) || (side->contents & CONTENTS_WINDOW)) {
			side->contents |= CONTENTS_TRANSLUCENT | CONTENTS_DETAIL;
		}

		// clip brushes, similarly, are not drawn and therefore can not occlude
		if (side->contents & (CONTENTS_PLAYER_CLIP | CONTENTS_MONSTER_CLIP)) {
			side->contents |= CONTENTS_DETAIL;
		}

		// atmospheric brushes like dust, fog and mist are always detail
		if (side->contents & CONTENTS_MASK_ATMOSPHERIC) {
			side->contents |= CONTENTS_TRANSLUCENT | CONTENTS_DETAIL;
		}

		// brushes with no visible or functional contents default to solid or window
		if (!(side->contents & CONTENTS_MASK_VISIBLE)) {
			if (!(side->contents & CONTENTS_MASK_FUNCTIONAL)) {
				if (side->contents & CONTENTS_TRANSLUCENT) {
					side->contents |= CONTENTS_WINDOW;
					side->contents &= ~CONTENTS_SOLID;
				} else {
					side->contents |= CONTENTS_SOLID;
				}
			}
		}

			// hints and skips have no contents
		if (side->surf & (SURF_HINT | SURF_SKIP)) {
			if (!(side->contents & CONTENTS_OCCLUSION_QUERY)) {
				side->contents = CONTENTS_NONE;
			}
		}

		// find the plane number
		side->plane_num = PlaneFromPoints(points[0], points[1], points[2]);
		if (side->plane_num == -1) {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Bad plane");
			brush->num_sides = 0;
			return;
		}

		// ensure that no other side on the brush references the same plane
		const brush_side_t *other = brush->sides;
		for (int32_t i = 0; i < brush->num_sides; i++, other++) {
			if (other->plane_num == side->plane_num) {
				Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Duplicate plane");
				brush->num_sides = 0;
				return;
			}
			if (other->plane_num == (side->plane_num ^ 1)) {
				Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Mirrored plane");
				brush->num_sides = 0;
				return;
			}
		}

		// resolve the texinfo
		side->texinfo = TexinfoForBrushSide(side, Vec3_Zero());

		brush->num_sides++;
		num_brush_sides++;
	}

	// get the content for the entire brush
	brush->contents = BrushContents(brush);

	// allow detail brushes to be removed
	if (no_detail && (brush->contents & CONTENTS_DETAIL)) {
		brush->num_sides = 0;
		return;
	}

	// allow liquid brushes to be removed
	if (no_liquid && (brush->contents & CONTENTS_MASK_LIQUID)) {
		brush->num_sides = 0;
		return;
	}

	// create windings for sides and bounds for brush
	MakeBrushWindings(brush);

	// origin brushes are removed, but they set the rotation origin for the rest of the brushes
	// in the entity. After the entire entity is parsed, the plane_nums and texinfos will be adjusted for
	// the origin brush
	if (brush->contents & CONTENTS_ORIGIN) {

		if (brush->entity_num == 0) {
			Mon_SendSelect(MON_WARN, brush->entity_num, brush->brush_num, "Origin brush in world");
		} else {
			const vec3_t origin = Box3_Center(brush->bounds);
			SetValueForKey(entity, "origin", va("%g %g %g", origin.x, origin.y, origin.z));
		}

		brush->num_sides = 0;
		return;
	}

	// sides that will not be visible at all will never be used as bsp splitters
	for (int32_t i = 0; i < brush->num_sides; i++) {

		if (brush->contents & CONTENTS_MASK_CLIP) {
			brush->sides[i].texinfo = BSP_TEXINFO_NODE;
		}

		if (brush->sides[i].surf & SURF_SKIP) {
			brush->sides[i].texinfo = BSP_TEXINFO_NODE;
		}
	}

	// add brush bevels, which are required for collision
	AddBrushBevels(brush);
}

/**
 * @brief Some entities are merged into the world, e.g. func_group.
 */
static void MoveBrushesToWorld(entity_t *ent) {

	const int32_t new_brushes = ent->num_brushes;
	const int32_t world_brushes = entities[0].num_brushes;

	brush_t *temp = Mem_TagMalloc(new_brushes * sizeof(brush_t), MEM_TAG_BRUSH);
	memcpy(temp, brushes + ent->first_brush, new_brushes * sizeof(brush_t));

	// make space to move the brushes (overlapped copy)
	memmove(brushes + world_brushes + new_brushes,
	        brushes + world_brushes,
	        sizeof(brush_t) * (num_brushes - world_brushes - new_brushes));

	// copy the new brushes down
	memcpy(brushes + world_brushes, temp, sizeof(brush_t) * new_brushes);

	// fix up indexes
	entities[0].num_brushes += new_brushes;
	for (int32_t i = 1; i < num_entities; i++) {
		entities[i].first_brush += new_brushes;
	}
	Mem_Free(temp);

	ent->num_brushes = 0;
}

/**
 * @brief
 */
static entity_t *ParseEntity(parser_t *parser) {
	char token[MAX_TOKEN_CHARS];

	entity_t *entity = NULL;

	if (Parse_IsEOF(parser)) {
		return NULL;
	}

	Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

	if (!g_strcmp0(token, "{")) {

		if (num_entities == MAX_BSP_ENTITIES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES\n");
		}

		entity = &entities[num_entities];
		num_entities++;

		entity->first_brush = num_brushes;

		while (true) {

			if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
				Com_Error(ERROR_FATAL, "EOF without closing entity\n");
			}

			if (!g_strcmp0(token, "}")) {
				Parse_SkipToken(parser, PARSE_DEFAULT);
				break;
			}

			if (!g_strcmp0(token, "{")) {
				ParseBrush(parser, entity);
			} else {
				entity_key_value_t *e = Mem_TagMalloc(sizeof(*e), MEM_TAG_EPAIR);

				if (!Parse_Token(parser, PARSE_DEFAULT, e->key, sizeof(e->key))) {
					Com_Error(ERROR_FATAL, "Invalid entity key\n");
				}

				Parse_Token(parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, e->value, sizeof(e->value));
				
				e->next = entity->values;
				entity->values = e;
			}
		}

		const vec3_t origin = VectorForKey(entity, "origin", Vec3_Zero());

		// if there was an origin brush, offset all of the planes and texinfo
		if (!Vec3_Equal(origin, Vec3_Zero())) {

			brush_t *brush = brushes + entity->first_brush;
			for (int32_t i = 0; i < entity->num_brushes; i++, brush++) {

				if (!brush->num_sides) {
					continue;
				}

				brush_side_t *side = brush->sides;
				for (int32_t j = 0; j < brush->num_sides; j++, side++) {

					const plane_t *plane = &planes[side->plane_num];
					const double dist = plane->dist - Vec3_Dot(plane->normal, origin);

					side->plane_num = FindPlane(plane->normal, dist);
					if (side->texinfo != BSP_TEXINFO_BEVEL) {
						side->texinfo = TexinfoForBrushSide(side, origin);
					}
				}
				MakeBrushWindings(brush);
			}
		}

		// jdolan: Some entities have their brushes merged into the world so that they are
		// CSG subtracted and included in the world's BSP tree. However, these brushes will
		// maintain a reference to their entity definition, so that any key-value pairs
		// associated with them will still be available.
		const char *class_name = ValueForKey(entity, "classname", NULL);
		if (!g_strcmp0(class_name, "func_group") ||
			!g_strcmp0(class_name, "misc_fog") ||
			!g_strcmp0(class_name, "misc_dust") ||
			!g_strcmp0(class_name, "misc_sprite")) {
			MoveBrushesToWorld(entity);
		}
	}

	return entity;
}

/**
 * @brief
 */
void LoadMapFile(const char *filename) {

	Com_Verbose("--- LoadMapFile ---\n");

	memset(entities, 0, sizeof(entities));
	num_entities = 0;

	memset(brushes, 0, sizeof(brushes));
	num_brushes = 0;

	memset(brush_sides, 0, sizeof(brush_sides));
	num_brush_sides = 0;

	memset(planes, 0, sizeof(planes));
	num_planes = 0;

	memset(plane_hash, 0, sizeof(plane_hash));

	void *buffer;
	if (Fs_Load(filename, &buffer) == -1) {
		Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
	}

	parser_t parser = Parse_Init(buffer, PARSER_DEFAULT);

	for (int32_t i = 0, models = 1; i < MAX_BSP_ENTITIES; i++) {

		entity_t *entity = ParseEntity(&parser);

		if (!entity) {
			break;
		}

		if (i > 0 && entity->num_brushes) {
			SetValueForKey(entity, "model", va("*%d", models++));
		}
	}

	map_bounds = Box3_Null();

	for (int32_t i = 0; i < entities[0].num_brushes; i++) {
		if (brushes[i].bounds.mins.x > MAX_WORLD_COORD) {
			continue; // no valid points
		}
		map_bounds = Box3_Union(map_bounds, brushes[i].bounds);
	}

	Com_Verbose("%5i brushes\n", num_brushes);
	Com_Verbose("%5i brush sides\n", num_brush_sides);
	Com_Verbose("%5i entities\n", num_entities);
	Com_Verbose("%5i planes\n", num_planes);
	Com_Verbose("size: %5.0f,%5.0f,%5.0f to %5.0f,%5.0f,%5.0f\n",
				map_bounds.mins.x, map_bounds.mins.y, map_bounds.mins.z,
				map_bounds.maxs.x, map_bounds.maxs.y, map_bounds.maxs.z);

	Fs_Free(buffer);
}
