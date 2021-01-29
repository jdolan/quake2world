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

#include "cm_local.h"

/**
 * @brief Box trace data encapsulation and context management.
 */
typedef struct {
	vec3_t start, end;

	vec3_t extents;

	vec3_t offsets[8];

	vec3_t box_mins, box_maxs;

	int32_t contents;

	_Bool is_point;

	cm_trace_t trace;

	int32_t brushes[32]; // used to avoid multiple intersection tests with brushes
} cm_trace_data_t;

/**
 * @brief
 */
static _Bool Cm_BrushAlreadyTested(cm_trace_data_t *data, const int32_t brush_num) {

	const int32_t hash = brush_num & 31;
	const _Bool skip = (data->brushes[hash] == brush_num);

	data->brushes[hash] = brush_num;

	return skip;
}

/**
 * @brief Clips the bounded box to all brush sides for the given brush.
 */
static void Cm_TraceToBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_sides) {
		return;
	}

	if (!Vec3_BoxIntersect(data->box_mins, data->box_maxs, brush->mins, brush->maxs)) {
		return;
	}

	float enter_fraction = -1.0;
	float leave_fraction = 1.0;

	const cm_bsp_plane_t *clip_plane = NULL;
	const cm_bsp_brush_side_t *clip_side = NULL;

	_Bool end_outside = false, start_outside = false;

	const cm_bsp_brush_side_t *side = brush->sides;
	for (int32_t i = 0; i < brush->num_sides; i++, side++) {

		const cm_bsp_plane_t *plane = side->plane;

		const float dist = plane->dist - Vec3_Dot(data->offsets[plane->sign_bits], plane->normal);

		const float d1 = Vec3_Dot(data->start, plane->normal) - dist;
		const float d2 = Vec3_Dot(data->end, plane->normal) - dist;

		if (d1 > 0.f) {
			start_outside = true;
		}
		if (d2 > 0.f) {
			end_outside = true;
		}

		// if completely in front of face, no intersection with entire brush
		if (d1 > 0.f && (d2 > TRACE_EPSILON || d2 >= d1)) {
			return;
		}

		// if completely behind plane, no intersection
		if (d1 < 0.f && d2 < 0.f) {
			continue;
		}

		// crosses face
		if (d1 > d2) { // enter
			const float f = (d1 - TRACE_EPSILON) / (d1 - d2);

			if (f > enter_fraction) {
				enter_fraction = f;
				clip_plane = plane;
				clip_side = side;
			}
		} else { // leave
			const float f = (d1 + TRACE_EPSILON) / (d1 - d2);

			if (f < leave_fraction) {
				leave_fraction = f;
			}
		}
	}

	// some sort of collision has occurred

	if (!start_outside) { // original point was inside brush
		data->trace.start_solid = true;
		if (!end_outside) {
			data->trace.all_solid = true;
			data->trace.fraction = 0.0;
			data->trace.contents = brush->contents;
		}
	} else if (enter_fraction < leave_fraction) { // pierced brush
		if (enter_fraction > -1.0f && enter_fraction < data->trace.fraction) {
			data->trace.fraction = Maxf(0.0f, enter_fraction);
			data->trace.plane = *clip_plane;
			data->trace.texinfo = clip_side->texinfo;
			data->trace.contents = brush->contents;
		}
	}
}

/**
 * @brief
 */
static void Cm_TestBoxInBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_sides) {
		return;
	}

	if (!Vec3_BoxIntersect(data->box_mins, data->box_maxs, brush->mins, brush->maxs)) {
		return;
	}

	const cm_bsp_brush_side_t *side = brush->sides;
	for (int32_t i = 0; i < brush->num_sides; i++, side++) {

		const cm_bsp_plane_t *plane = side->plane;

		const float dist = plane->dist - Vec3_Dot(data->offsets[plane->sign_bits], plane->normal);

		const float d1 = Vec3_Dot(data->start, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0.0f) {
			return;
		}
	}

	// inside this brush
	data->trace.start_solid = data->trace.all_solid = true;
	data->trace.fraction = 0.0f;
	data->trace.contents = brush->contents;
}

/**
 * @brief
 */
static void Cm_TraceToLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &cm_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents)) {
		return;
	}

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = cm_bsp.leaf_brushes[leaf->first_leaf_brush + i];

		if (Cm_BrushAlreadyTested(data, brush_num)) {
			continue; // already checked this brush in another leaf
		}

		const cm_bsp_brush_t *b = &cm_bsp.brushes[brush_num];

		if (!(b->contents & data->contents)) {
			continue;
		}

		Cm_TraceToBrush(data, b);

		if (data->trace.all_solid) {
			return;
		}
	}
}

/**
 * @brief
 */
static void Cm_TestInLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &cm_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents)) {
		return;
	}

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = cm_bsp.leaf_brushes[leaf->first_leaf_brush + i];

		if (Cm_BrushAlreadyTested(data, brush_num)) {
			continue; // already checked this brush in another leaf
		}

		const cm_bsp_brush_t *b = &cm_bsp.brushes[brush_num];

		if (!(b->contents & data->contents)) {
			continue;
		}

		Cm_TestBoxInBrush(data, b);

		if (data->trace.all_solid) {
			return;
		}
	}
}

/**
 * @brief
 */
static void Cm_TraceToNode(cm_trace_data_t *data, int32_t num, float p1f, float p2f,
                           const vec3_t p1, const vec3_t p2) {

	if (data->trace.fraction <= p1f) {
		return; // already hit something nearer
	}

	// if < 0, we are in a leaf node
	if (num < 0) {
		Cm_TraceToLeaf(data, -1 - num);
		return;
	}

	// find the point distances to the separating plane
	// and the offset for the size of the box
	const cm_bsp_node_t *node = cm_bsp.nodes + num;
	const cm_bsp_plane_t *plane = node->plane;

	float d1, d2, offset;
	if (AXIAL(plane)) {
		d1 = p1.xyz[plane->type] - plane->dist;
		d2 = p2.xyz[plane->type] - plane->dist;
		offset = data->extents.xyz[plane->type];
	} else {
		d1 = Vec3_Dot(plane->normal, p1) - plane->dist;
		d2 = Vec3_Dot(plane->normal, p2) - plane->dist;
		if (data->is_point) {
			offset = 0.f;
		} else {
			offset = (fabsf(data->extents.x * plane->normal.x) +
			         fabsf(data->extents.y * plane->normal.y) +
			         fabsf(data->extents.z * plane->normal.z)) * 3.f;
		}
	}

	// see which sides we need to consider
	if (d1 >= offset + TRACE_EPSILON && d2 >= offset + TRACE_EPSILON) {
		Cm_TraceToNode(data, node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (d1 <= -offset - TRACE_EPSILON && d2 <= -offset - TRACE_EPSILON) {
		Cm_TraceToNode(data, node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the cross point DIST_EPSILON pixels on the near side
	int32_t side;
	float frac1, frac2;

	if (d1 < d2) {
		const float idist = 1.f / (d1 - d2);
		side = 1;
		frac2 = (d1 + offset + TRACE_EPSILON) * idist;
		frac1 = (d1 - offset + TRACE_EPSILON) * idist;
	} else if (d1 > d2) {
		const float idist = 1.f / (d1 - d2);
		side = 0;
		frac2 = (d1 - offset - TRACE_EPSILON) * idist;
		frac1 = (d1 + offset + TRACE_EPSILON) * idist;
	} else {
		side = 0;
		frac1 = 1.f;
		frac2 = 0.f;
	}

	// move up to the node
	frac1 = Clampf(frac1, 0.f, 1.f);

	const float midf1 = p1f + (p2f - p1f) * frac1;

	vec3_t mid = Vec3_Mix(p1, p2, frac1);

	Cm_TraceToNode(data, node->children[side], p1f, midf1, p1, mid);

	// go past the node
	frac2 = Clampf(frac2, 0.f, 1.f);

	const float midf2 = p1f + (p2f - p1f) * frac2;

	mid = Vec3_Mix(p1, p2, frac2);

	Cm_TraceToNode(data, node->children[side ^ 1], midf2, p2f, mid, p2);
}

/**
 * @brief Adjust inputs so that mins and maxs are always symetric, which
 * avoids some complications with plane expanding of rotated bmodels.
 * @param start 
 * @param end 
 * @param mins 
 * @param maxs 
*/
static void Cm_AdjustTraceSymmetry(vec3_t *start, vec3_t *end, vec3_t *mins, vec3_t *maxs) {

	const vec3_t offset = Vec3_Scale(Vec3_Add(*mins, *maxs), .5f);

	*mins = Vec3_Subtract(*mins, offset);
	*maxs = Vec3_Subtract(*maxs, offset);

	*start = Vec3_Add(*start, offset);
	*end = Vec3_Add(*end, offset);
}

/**
 * @brief Primary collision detection entry point. This function recurses down
 * the BSP tree from the specified head node, clipping the desired movement to
 * brushes that match the specified contents mask.
 *
 * @param start The starting point.
 * @param end The desired end point.
 * @param mins The bounding box mins, in model space.
 * @param maxs The bounding box maxs, in model space.
 * @param head_node The BSP head node to recurse down.
 * @param contents The contents mask to clip to.
 *
 * @return The trace.
 */
cm_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                       const int32_t head_node, const int32_t contents) {

	vec3_t start0 = start, end0 = end, mins0 = mins, maxs0 = maxs;

	Cm_AdjustTraceSymmetry(&start0, &end0, &mins0, &maxs0);

	cm_trace_data_t data = {
		.start = start0,
		.end = end0,
		.contents = contents,
		.trace = {
			.fraction = 1.f
		}
	};

	memset(data.brushes, 0xff, sizeof(data.brushes));

	if (!cm_bsp.file.num_nodes) { // map not loaded
		return data.trace;
	}

	// check for point special case
	if (Vec3_Equal(mins, Vec3_Zero()) && Vec3_Equal(maxs, Vec3_Zero())) {
		data.is_point = true;
	} else {
		data.is_point = false;

		// extents allow planes to be shifted to account for the box size
		data.extents.x = -mins0.x > maxs0.x ? -mins0.x : maxs0.x;
		data.extents.y = -mins0.y > maxs0.y ? -mins0.y : maxs0.y;
		data.extents.z = -mins0.z > maxs0.z ? -mins0.z : maxs0.z;

		// offsets provide sign bit lookups for fast plane tests
		data.offsets[0] = mins0;

		data.offsets[1].x = maxs0.x;
		data.offsets[1].y = mins0.y;
		data.offsets[1].z = mins0.z;

		data.offsets[2].x = mins0.x;
		data.offsets[2].y = maxs0.y;
		data.offsets[2].z = mins0.z;

		data.offsets[3].x = maxs0.x;
		data.offsets[3].y = maxs0.y;
		data.offsets[3].z = mins0.z;

		data.offsets[4].x = mins0.x;
		data.offsets[4].y = mins0.y;
		data.offsets[4].z = maxs0.z;

		data.offsets[5].x = maxs0.x;
		data.offsets[5].y = mins0.y;
		data.offsets[5].z = maxs0.z;

		data.offsets[6].x = mins0.x;
		data.offsets[6].y = maxs0.y;
		data.offsets[6].z = maxs0.z;

		data.offsets[7] = maxs0;
	}

	Cm_TraceBounds(start0, end0, mins0, maxs0, &data.box_mins, &data.box_maxs);

	// check for position test special case
	if (Vec3_Equal(start, end)) {
		static __thread int32_t leafs[MAX_BSP_LEAFS];
		const size_t num_leafs = Cm_BoxLeafnums(data.box_mins,
												data.box_maxs,
												leafs,
												lengthof(leafs),
												NULL,
												head_node);

		for (size_t i = 0; i < num_leafs; i++) {
			Cm_TestInLeaf(&data, leafs[i]);

			if (data.trace.all_solid) {
				break;
			}
		}

		data.trace.end = start;
		return data.trace;
	}

	Cm_TraceToNode(&data, head_node, 0.0, 1.0, start0, end0);

	if (data.trace.fraction == 0.0f) {
		data.trace.end = start;
	} else if (data.trace.fraction == 1.0f) {
		data.trace.end = end;
	} else {
		data.trace.end = Vec3_Mix(start, end, data.trace.fraction);
	}

	return data.trace;
}

/**
 * @brief Collision detection for non-world models. Rotates the specified end
 * points into the model's space, and traces down the relevant subset of the
 * BSP tree. For inline BSP models, the head node is the root of the model's
 * subtree. For mesh models, a special reserved box hull and head node are
 * used.
 *
 * @param start The trace start point, in world space.
 * @param end The trace end point, in world space.
 * @param mins The bounding box mins, in model space.
 * @param maxs The bounding box maxs, in model space.
 * @param head_node The BSP head node to recurse down.
 * @param contents The contents mask to clip to.
 * @param matrix The matrix of the entity to clip to.
 * @param inverse_matrix The inverse matrix of the entity to clip to.
 *
 * @return The trace.
 */
cm_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end,
								  const vec3_t mins, const vec3_t maxs,
								  const int32_t head_node, const int32_t contents,
                                  const mat4_t *matrix, const mat4_t *inverse_matrix) {
	
	vec3_t start0 = start, end0 = end, mins0 = mins, maxs0 = maxs;

	Cm_AdjustTraceSymmetry(&start0, &end0, &mins0, &maxs0);

	vec3_t start1, end1;

	Matrix4x4_Transform(inverse_matrix, start0.xyz, start1.xyz);
	Matrix4x4_Transform(inverse_matrix, end0.xyz, end1.xyz);

	// sweep the box through the model
	cm_trace_t trace = Cm_BoxTrace(start1, end1, mins0, maxs0, head_node, contents);

	if (trace.fraction < 1.0f) { // transform the impacted plane
		trace.plane = Cm_TransformPlane(matrix, &trace.plane);
	}

	// and calculate the final end point
	trace.end = Vec3_Mix(start, end, trace.fraction);

	return trace;
}

/**
 * @brief Calculates a suitable bounding box for tracing to an entity.
 * @param solid The entity's solid type.
 * @param origin The entity's origin.
 * @param angles The entity's angles.
 * @param mins The entity's mins, in model space.
 * @param maxs The entity's maxs, in model space.
 * @param bounds_mins The resulting bounds mins, in world space.
 * @param bounds_maxs The resulting bounds maxs, in world space.
 * @remarks BSP entities have asymmetrical bounding boxes, requiring special attention.
 */
void Cm_EntityBounds(const solid_t solid, const vec3_t origin, const vec3_t angles,
                     const vec3_t mins, const vec3_t maxs, vec3_t *bounds_mins, vec3_t *bounds_maxs) {

	if (solid == SOLID_BSP && !Vec3_Equal(angles, Vec3_Zero())) {
		float max = 0.0;

		for (int32_t i = 0; i < 3; i++) {
			max = Maxf(max, Maxf(fabsf(mins.xyz[i]), fabsf(maxs.xyz[i])));
		}

		*bounds_mins = Vec3_Add(origin, Vec3(-max, -max, -max));
		*bounds_maxs = Vec3_Add(origin, Vec3( max,  max,  max));
	} else {
		*bounds_mins = Vec3_Add(origin, mins);
		*bounds_maxs = Vec3_Add(origin, maxs);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	for (int32_t i = 0; i < 3; i++) {
		bounds_mins->xyz[i] -= 1;
		bounds_maxs->xyz[i] += 1;
	}
}

/**
 * @brief Calculates the bounding box for a trace.
 * @param start The trace start point, in world space.
 * @param end The trace end point, in world space.
 * @param mins The bounding box mins, in model space.
 * @param maxs The bounding box maxs, in model space.
 * @param bounds_mins The resulting bounds mins, in world space.
 * @param bounds_maxs The resulting bounds maxs, in world space.
 */
void Cm_TraceBounds(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                    vec3_t *bounds_mins, vec3_t *bounds_maxs) {

	for (int32_t i = 0; i < 3; i++) {
		if (end.xyz[i] > start.xyz[i]) {
			bounds_mins->xyz[i] = start.xyz[i] + mins.xyz[i];
			bounds_maxs->xyz[i] = end.xyz[i] + maxs.xyz[i];
		} else {
			bounds_mins->xyz[i] = end.xyz[i] + mins.xyz[i];
			bounds_maxs->xyz[i] = start.xyz[i] + maxs.xyz[i];
		}
	}
}
