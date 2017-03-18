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

#include "cl_local.h"

#define NET_GRAPH_WIDTH 256
#define NET_GRAPH_Y 0

// net graph samples
typedef struct {
	vec_t value;
	int32_t color;
} net_graph_sample_t;

static net_graph_sample_t net_graph_samples[NET_GRAPH_WIDTH];
static int32_t num_net_graph_samples;

/**
 * @brief Accumulates a net graph sample.
 */
static void Cl_NetGraph(vec_t value, int32_t color) {

	net_graph_samples[num_net_graph_samples].value = value;
	net_graph_samples[num_net_graph_samples].color = color;

	if (net_graph_samples[num_net_graph_samples].value > 1.0) {
		net_graph_samples[num_net_graph_samples].value = 1.0;
	}

	num_net_graph_samples++;

	if (num_net_graph_samples == NET_GRAPH_WIDTH) {
		num_net_graph_samples = 0;
	}
}

/**
 * @brief Accumulates net graph samples for the current frame. Dropped or
 * suppressed packets are recorded as peak samples, and packet latency is
 * recorded over a range of 0-300ms.
 */
void Cl_AddNetGraph(void) {
	uint32_t i;

	// we only need to do our accounting when asked to
	if (!cl_draw_net_graph->value) {
		return;
	}

	for (i = 0; i < cls.net_chan.dropped; i++) {
		Cl_NetGraph(1.0, 0x40);
	}

	for (i = 0; i < cl.suppress_count; i++) {
		Cl_NetGraph(1.0, 0xdf);
	}

	// see what the latency was on this packet
	const uint32_t frame = cls.net_chan.incoming_acknowledged & CMD_MASK;
	const uint32_t ping = cl.unclamped_time - cl.cmds[frame].timestamp;

	Cl_NetGraph(ping / 300.0, 0xd0); // 300ms is lagged out
}

/**
 * @brief Provides a real-time visual representation of latency and packet loss
 * via a small graph drawn on screen.
 */
static void Cl_DrawNetGraph(void) {
	int32_t i, j, x, y, h;

	if (!cl_draw_net_graph->value) {
		return;
	}

	r_pixel_t ch;
	R_BindFont("small", NULL, &ch);

	const r_pixel_t netgraph_height = ch * 3;

	x = r_context.width - NET_GRAPH_WIDTH;
	y = r_context.height - NET_GRAPH_Y - netgraph_height;

	R_DrawFill(x, y, NET_GRAPH_WIDTH, netgraph_height, 8, 0.2);

	for (i = 0; i < NET_GRAPH_WIDTH; i++) {

		j = (num_net_graph_samples - i) & (NET_GRAPH_WIDTH - 1);
		h = net_graph_samples[j].value * netgraph_height;

		if (!h) {
			continue;
		}

		x = r_context.width - i;
		y = r_context.height - NET_GRAPH_Y - h;

		R_DrawFill(x, y, 1, h, net_graph_samples[j].color, 0.5);
	}
}

static const char *r_state_names[] = {
	"program binds",
	"activeTexture changes",
	"texture binds",
	"buffer binds",
	"blendFunc changes",
	"GL_BLEND toggles",
	"depthMask changes",
	"GL_STENCIL toggles",
	"stencilOp changes",
	"stencilFunc changes",
	"polygonOffset changes",
	"GL_POLYGON_OFFSET toggles",
	"viewport changes",
	"GL_DEPTH_TEST toggles",
	"depthRange changes",
	"GL_SCISSOR toggles",
	"scissor changes",
	"program uniform changes",
	"program attrib pointer changes",
	"program attrib constant changes",
	"program attrib toggles",
};

static const char *r_texnum_names[] = {
	"diffuse",
	"lightmap",
	"deluxemap",
	"normalmap",
	"specularmap",
	"warp",
	"tint"
};

/**
 * @brief Draws counters and performance information about the renderer.
 */
static void Cl_DrawRendererStats(void) {
	r_pixel_t ch, y = 64;

	if (!cl_show_renderer_stats->value) {
		return;
	}

	if (cls.state != CL_ACTIVE) {
		return;
	}

	R_BindFont("small", NULL, &ch);
	R_DrawString(0, y, "BSP:", CON_COLOR_YELLOW);
	y += ch;

	R_DrawString(0, y, va("%d clusters", r_view.num_bsp_clusters), CON_COLOR_YELLOW);
	y += ch;

	R_DrawString(0, y, va("%d leafs", r_view.num_bsp_leafs), CON_COLOR_YELLOW);
	y += ch;

	R_DrawString(0, y, va("%d surfaces", r_view.num_bsp_surfaces), CON_COLOR_YELLOW);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Mesh:", CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d models", r_view.num_mesh_models), CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d tris", r_view.num_mesh_tris), CON_COLOR_CYAN);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Draws:", CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d elements over %d batches total", r_view.num_draw_array_count + r_view.num_draw_element_count,
	                      r_view.num_draw_arrays + r_view.num_draw_elements), CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d elements over %d array batches", r_view.num_draw_array_count, r_view.num_draw_arrays),
	             CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d elements over %d element batches", r_view.num_draw_element_count, r_view.num_draw_elements),
	             CON_COLOR_CYAN);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Other:", CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("%d lights", r_view.num_lights), CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("%d particles", r_view.num_particles), CON_COLOR_WHITE);
	y += ch;

	uint32_t total_state_changes = 0;

	for (uint32_t i = 0; i < R_STATE_TOTAL; i++) {
		total_state_changes += r_view.num_state_changes[i];
	}

	R_DrawString(0, y, va("%d state changes", total_state_changes), CON_COLOR_WHITE);
	y += ch;

	for (uint32_t i = 0; i < R_STATE_TOTAL; i++) {
		R_DrawString(0, y, va("- %d %s", r_view.num_state_changes[i], r_state_names[i]), CON_COLOR_WHITE);
		y += ch;
	}

	uint32_t total_texunit_changes = 0;

	for (uint32_t i = 0; i < R_TEXUNIT_TOTAL; ++i) {
		total_texunit_changes += r_view.num_binds[i];
	}

	R_DrawString(0, y, va("%d texunit changes", total_texunit_changes), CON_COLOR_GREEN);
	y += ch;

	for (uint32_t i = 0; i < R_TEXUNIT_TOTAL; ++i) {
		R_DrawString(0, y, va("- %d %s", r_view.num_binds[i], r_texnum_names[i]), CON_COLOR_GREEN);
		y += ch;
	}

	y += ch;
	R_DrawString(0, y, va("%d buffer uploads (%d partial, %d full; %d bytes)",
	                      r_view.num_buffer_full_uploads + r_view.num_buffer_partial_uploads, r_view.num_buffer_full_uploads,
	                      r_view.num_buffer_partial_uploads, r_view.size_buffer_uploads), CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("%d total buffers created (%d bytes)", R_GetNumAllocatedBuffers(),
	                      R_GetNumAllocatedBufferBytes()), CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("cull: %d pass, %d fail", r_view.cull_passes, r_view.cull_fails), CON_COLOR_WHITE);

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws counters and performance information about the sound subsystem.
 */
static void Cl_DrawSoundStats(void) {
	r_pixel_t ch, y = cl_show_renderer_stats->value ? 400 : 64;

	if (!cl_show_sound_stats->value) {
		return;
	}

	if (cls.state != CL_ACTIVE) {
		return;
	}

	R_BindFont("small", NULL, &ch);

	R_DrawString(0, y, "Sound:", CON_COLOR_MAGENTA);
	y += ch;

	R_DrawString(0, y, va("%d channels", s_env.num_active_channels), CON_COLOR_MAGENTA);

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cl_DrawCounters(void) {
	static vec3_t velocity;
	static char pps[8], fps[8], spd[8];
	static int32_t last_draw_time, last_speed_time;
	r_pixel_t cw, ch;

	if (!cl_draw_counters->integer) {
		return;
	}

	R_BindFont("small", &cw, &ch);

	const r_pixel_t x = r_context.width - 7 * cw;
	r_pixel_t y = r_context.height - 3 * ch;

	cl.frame_counter++;

	if (quetoo.ticks - last_speed_time >= 100) {

		VectorCopy(cl.frame.ps.pm_state.velocity, velocity);
		velocity[2] = 0.0;

		g_snprintf(spd, sizeof(spd), "%4.0fspd", VectorLength(velocity));

		last_speed_time = quetoo.ticks;
	}

	if (quetoo.ticks - last_draw_time >= 1000) {

		g_snprintf(fps, sizeof(fps), "%4ufps", cl.frame_counter);
		g_snprintf(pps, sizeof(pps), "%4upps", cl.packet_counter);

		last_draw_time = quetoo.ticks;

		cl.frame_counter = 0;
		cl.packet_counter = 0;
	}

	if (cl_draw_position->integer) {
		R_DrawString(r_context.width - 14 * cw, y - ch, va("%4.0f %4.0f %4.0f", cl.frame.ps.pm_state.origin[0], cl.frame.ps.pm_state.origin[1], cl.frame.ps.pm_state.origin[2]), CON_COLOR_DEFAULT);
	}

	R_DrawString(x, y, spd, CON_COLOR_DEFAULT);
	y += ch;

	R_DrawString(x, y, fps, CON_COLOR_DEFAULT);
	y += ch;

	R_DrawString(x, y, pps, CON_COLOR_DEFAULT);
	y += ch;

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief This is called every frame, and can also be called explicitly to flush
 * text to the screen.
 */
void Cl_UpdateScreen(void) {

	if (cls.state == CL_ACTIVE) {

		R_BeginFrame();

		R_Setup3D();

		R_DrawView();

		R_Setup2D();

		R_EnableBlend(false);

		R_DrawSupersample();

		R_EnableBlend(true);

		switch (cls.key_state.dest) {
			case KEY_CONSOLE:
				Cl_DrawConsole();
				break;
			default:

				Cl_DrawChat();
				Cl_DrawNotify();
				Cl_DrawNetGraph();
				Cl_DrawCounters();
				Cl_DrawRendererStats();
				Cl_DrawSoundStats();
				cls.cgame->UpdateScreen(&cl.frame);
				break;
		}

		R_AddStains();
	} else {
		R_BeginFrame();

		R_Setup2D();

		if (cls.state == CL_LOADING) {
			Cl_DrawLoading();
		} else if (cls.key_state.dest == KEY_CONSOLE) {
			Cl_DrawConsole();
		}
	}

	R_Draw2D();

	Ui_Draw();

	R_EndFrame();

	Cl_ClearView();
}
