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

#include "cg_local.h"

#define HUD_COLOR_STAT			CON_COLOR_DEFAULT
#define HUD_COLOR_STAT_MED		CON_COLOR_YELLOW
#define HUD_COLOR_STAT_LOW		CON_COLOR_RED

#define CROSSHAIR_COLOR_RED		242
#define CROSSHAIR_COLOR_GREEN	209
#define CROSSHAIR_COLOR_YELLOW	219
#define CROSSHAIR_COLOR_ORANGE  225
#define CROSSHAIR_COLOR_DEFAULT	15

#define HUD_PIC_HEIGHT			64

#define HUD_HEALTH_MED			75
#define HUD_HEALTH_LOW			25

#define HUD_ARMOR_MED			50
#define HUD_ARMOR_LOW			25

#define HUD_POWERUP_LOW			5

typedef struct cg_crosshair_s {
	char name[16];
	r_image_t *image;
	vec4_t color;
} cg_crosshair_t;

static cg_crosshair_t crosshair;

#define CENTER_PRINT_LINES 8
typedef struct cg_center_print_s {
	char lines[CENTER_PRINT_LINES][MAX_STRING_CHARS];
	uint16_t num_lines;
	uint32_t time;
} cg_center_print_t;

static cg_center_print_t center_print;

typedef struct {
	// pickup
	uint32_t last_pulse_time;
	int16_t pickup_pulse;

	// damage
	uint32_t last_hit_sound_time;

	// blend
	uint32_t last_pickup_time;
	uint32_t last_damage_time;
	int16_t pickup;
} cg_hud_locals_t;

static cg_hud_locals_t cg_hud_locals;

/**
 * @brief Draws the icon at the specified ConfigString index, relative to CS_IMAGES.
 */
static void Cg_DrawIcon(const r_pixel_t x, const r_pixel_t y, const vec_t scale,
                        const uint16_t icon) {

	if (icon >= MAX_IMAGES || !cgi.client->image_precache[icon]) {
		cgi.Warn("Invalid icon: %d\n", icon);
		return;
	}

	cgi.DrawImage(x, y, scale, cgi.client->image_precache[icon]);
}

/**
 * @brief Draws the vital numeric and icon, flashing on low quantities.
 */
static void Cg_DrawVital(r_pixel_t x, const int16_t value, const int16_t icon, int16_t med,
                         int16_t low) {
	r_pixel_t y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT + 4;

	vec4_t pulse = { 1.0, 1.0, 1.0, 1.0 };
	int32_t color = HUD_COLOR_STAT;

	if (value < low) {
		if (cg_draw_vitals_pulse->integer) {
			pulse[3] = sin(cgi.client->unclamped_time / 250.0) + 0.75;
		}
		color = HUD_COLOR_STAT_LOW;
	} else if (value < med) {
		color = HUD_COLOR_STAT_MED;
	}

	const char *string = va("%3d", value);

	x += cgi.view->viewport.x;
	cgi.DrawString(x, y, string, color);

	x += cgi.StringWidth(string);
	y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT;

	cgi.Color(pulse);
	Cg_DrawIcon(x, y, 1.0, icon);
	cgi.Color(NULL);
}

/**
 * @brief Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawVitals(const player_state_t *ps) {
	r_pixel_t x, cw, x_offset;

	if (!cg_draw_vitals->integer) {
		return;
	}

	cgi.BindFont("large", &cw, NULL);

	x_offset = 3 * cw;

	if (ps->stats[STAT_HEALTH] > 0) {
		const int16_t health = ps->stats[STAT_HEALTH];
		const int16_t health_icon = ps->stats[STAT_HEALTH_ICON];

		x = cgi.view->viewport.w * 0.25 - x_offset;

		Cg_DrawVital(x, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

	if (atoi(cgi.ConfigString(CS_GAMEPLAY)) != GAME_INSTAGIB) {

		if (ps->stats[STAT_AMMO] > 0) {
			const int16_t ammo = ps->stats[STAT_AMMO];
			const int16_t ammo_low = ps->stats[STAT_AMMO_LOW];
			const int16_t ammo_icon = ps->stats[STAT_AMMO_ICON];

			x = cgi.view->viewport.w * 0.5 - x_offset;

			Cg_DrawVital(x, ammo, ammo_icon, -1, ammo_low);
		}

		if (ps->stats[STAT_ARMOR] > 0) {
			const int16_t armor = ps->stats[STAT_ARMOR];
			const int16_t armor_icon = ps->stats[STAT_ARMOR_ICON];

			x = cgi.view->viewport.w * 0.75 - x_offset;

			Cg_DrawVital(x, armor, armor_icon, HUD_ARMOR_MED, HUD_ARMOR_LOW);
		}
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the powerup and the time remaining
 */
static void Cg_DrawPowerup(r_pixel_t y, const int16_t value, const r_image_t *icon) {
	r_pixel_t x;

	vec4_t pulse = { 1.0, 1.0, 1.0, 1.0 };
	int32_t color = HUD_COLOR_STAT;

	if (value < HUD_POWERUP_LOW) {
		color = HUD_COLOR_STAT_LOW;
	}

	const char *string = va("%d", value);

	x = cgi.view->viewport.x + (HUD_PIC_HEIGHT / 2);

	cgi.Color(pulse);
	cgi.DrawImage(x, y, 1.0, icon);
	cgi.Color(NULL);

	x += HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, string, color);
}

/**
 * @brief Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawPowerups(const player_state_t *ps) {
	r_pixel_t y, ch;

	if (!cg_draw_powerups->integer) {
		return;
	}

	cgi.BindFont("large", &ch, NULL);

	y = cgi.view->viewport.y + (cgi.view->viewport.h / 2);

	if (ps->stats[STAT_QUAD_TIME] > 0) {
		const int32_t timer = ps->stats[STAT_QUAD_TIME];

		Cg_DrawPowerup(y, timer, cgi.LoadImage("pics/i_quad", IT_PIC));

		y += HUD_PIC_HEIGHT;
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the flag you are currently holding
 */
static void Cg_DrawHeldFlag(const player_state_t *ps) {
	r_pixel_t x, y;

	if (!cg_draw_heldflag->integer) {
		return;
	}

	vec4_t pulse = { 1.0, 1.0, 1.0, sin(cgi.client->unclamped_time / 150.0) + 0.75 };

	x = cgi.view->viewport.x + (HUD_PIC_HEIGHT / 2);
	y = cgi.view->viewport.y + ((cgi.view->viewport.h / 2) - (HUD_PIC_HEIGHT * 2));

	uint16_t flag = ps->stats[STAT_CARRYING_FLAG];

	if (flag != 0) {
		cgi.Color(pulse);

		cgi.DrawImage(x, y, 1.0, cgi.LoadImage(va("pics/i_flag%d", flag), IT_PIC));

		cgi.Color(NULL);
	}
}

/**
 * @brief
 */
static void Cg_DrawPickup(const player_state_t *ps) {
	r_pixel_t x, y, cw, ch;

	if (!cg_draw_pickup->integer) {
		return;
	}

	cgi.BindFont(NULL, &cw, &ch);

	if (ps->stats[STAT_PICKUP_ICON] != -1) {
		const int16_t icon = ps->stats[STAT_PICKUP_ICON] & ~STAT_TOGGLE_BIT;
		const int16_t pickup = ps->stats[STAT_PICKUP_STRING];

		const char *string = cgi.ConfigString(pickup);

		x = cgi.view->viewport.x + cgi.view->viewport.w - HUD_PIC_HEIGHT - cgi.StringWidth(string);
		y = cgi.view->viewport.y;

		Cg_DrawIcon(x, y, 1.0, icon);

		x += HUD_PIC_HEIGHT;
		y += (HUD_PIC_HEIGHT - ch) / 2 + 2;

		cgi.DrawString(x, y, string, HUD_COLOR_STAT);
	}
}

/**
 * @brief
 */
static void Cg_DrawFrags(const player_state_t *ps) {
	const int16_t frags = ps->stats[STAT_FRAGS];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	if (!cg_draw_frags->integer) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Frags");
	y = cgi.view->viewport.y + HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, "Frags", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->viewport.x + cgi.view->viewport.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", frags), HUD_COLOR_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawDeaths(const player_state_t *ps) {
	const int16_t deaths = ps->stats[STAT_DEATHS];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	if (!cg_draw_deaths->integer) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Deaths");
	y = cgi.view->viewport.y + 2 * (HUD_PIC_HEIGHT + ch);

	cgi.DrawString(x, y, "Deaths", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->viewport.x + cgi.view->viewport.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", deaths), HUD_COLOR_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}


/**
 * @brief
 */
static void Cg_DrawCaptures(const player_state_t *ps) {
	const int16_t captures = ps->stats[STAT_CAPTURES];
	r_pixel_t x, y, cw, ch;

	if (!cg_draw_captures->integer) {
		return;
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	if (atoi(cgi.ConfigString(CS_CTF)) < 1) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Captures");
	y = cgi.view->viewport.y + 3 * (HUD_PIC_HEIGHT + ch);

	cgi.DrawString(x, y, "Captures", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->viewport.x + cgi.view->viewport.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", captures), HUD_COLOR_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawSpectator(const player_state_t *ps) {
	r_pixel_t x, y, cw;

	if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE]) {
		return;
	}

	cgi.BindFont("small", &cw, NULL);

	x = cgi.view->viewport.w - cgi.StringWidth("Spectating");
	y = cgi.view->viewport.y + HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Spectating", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawChase(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_USER_INFO_VALUE * 2], *s;

	if (!ps->stats[STAT_CHASE]) {
		return;
	}

	const int32_t c = ps->stats[STAT_CHASE];

	if (c - CS_CLIENTS >= MAX_CLIENTS) {
		cgi.Warn("Invalid client info index: %d\n", c);
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	g_snprintf(string, sizeof(string), "Chasing ^7%s", cgi.ConfigString(c));

	if ((s = strchr(string, '\\'))) {
		*s = '\0';
	}

	x = cgi.view->viewport.x + cgi.view->viewport.w * 0.5 - cgi.StringWidth(string) / 2;
	y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawVote(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_STRING_CHARS];

	if (!cg_draw_vote->integer) {
		return;
	}

	if (!ps->stats[STAT_VOTE]) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	g_snprintf(string, sizeof(string), "Vote: ^7%s", cgi.ConfigString(ps->stats[STAT_VOTE]));

	x = cgi.view->viewport.x;
	y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawTime(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	const char *string = cgi.ConfigString(CS_TIME);

	if (!ps->stats[STAT_TIME]) {
		return;
	}

	if (!cg_draw_time->integer) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth(string);
	y = cgi.view->viewport.y + 3 * (HUD_PIC_HEIGHT + ch);

	if (atoi(cgi.ConfigString(CS_CTF)) > 0) {
		y += HUD_PIC_HEIGHT + ch;
	}

	cgi.DrawString(x, y, string, CON_COLOR_DEFAULT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawReady(const player_state_t *ps) {
	r_pixel_t x, y, ch;

	if (!ps->stats[STAT_READY]) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Ready");
	y = cgi.view->viewport.y + HUD_PIC_HEIGHT * 2 + 2 * ch;

	cgi.DrawString(x, y, "Ready", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawTeam(const player_state_t *ps) {
	const int16_t team = ps->stats[STAT_TEAM];
	r_pixel_t x, y;

	if (team == -1) {
		return;
	}

	if (!cg_draw_teambar->integer) {
		return;
	}

	color_t color = cg_team_info[team].color;
	color.a = 38;

	x = cgi.view->viewport.x;
	y = cgi.view->viewport.y + cgi.view->viewport.h - 64;

	cgi.DrawFill(x, y, cgi.view->viewport.w, 64, color.u32, -1.0);
}

/**
 * @brief
 */
static void Cg_DrawCrosshair(const player_state_t *ps) {
	r_pixel_t x, y;
	int32_t color;

	if (!cg_draw_crosshair->value) {
		return;
	}

	if (cgi.client->third_person) {
		return;
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return; // spectating
	}

	if (center_print.time > cgi.client->unclamped_time) {
		return;
	}

	if (cg_draw_crosshair->modified) { // crosshair image
		cg_draw_crosshair->modified = false;

		if (cg_draw_crosshair->value < 0) {
			cg_draw_crosshair->value = 1;
		}

		if (cg_draw_crosshair->value > 100) {
			cg_draw_crosshair->value = 100;
		}

		g_snprintf(crosshair.name, sizeof(crosshair.name), "ch%d", cg_draw_crosshair->integer);

		crosshair.image = cgi.LoadImage(va("pics/ch%d", cg_draw_crosshair->integer), IT_PIC);

		if (crosshair.image->type == IT_NULL) {
			cgi.Print("Couldn't load pics/ch%d.\n", cg_draw_crosshair->integer);
			return;
		}
	}

	if (crosshair.image->type == IT_NULL) { // not found
		return;
	}

	if (cg_draw_crosshair_color->modified) { // crosshair color
		cg_draw_crosshair_color->modified = false;

		const char *c = cg_draw_crosshair_color->string;
		if (!g_ascii_strcasecmp(c, "red")) {
			color = CROSSHAIR_COLOR_RED;
		} else if (!g_ascii_strcasecmp(c, "green")) {
			color = CROSSHAIR_COLOR_GREEN;
		} else if (!g_ascii_strcasecmp(c, "yellow")) {
			color = CROSSHAIR_COLOR_YELLOW;
		} else if (!g_ascii_strcasecmp(c, "orange")) {
			color = CROSSHAIR_COLOR_ORANGE;
		} else {
			color = CROSSHAIR_COLOR_DEFAULT;
		}

		cgi.ColorFromPalette(color, crosshair.color);
	}

	vec_t scale = cg_draw_crosshair_scale->value * (cgi.context->high_dpi ? 2.0 : 1.0);
	crosshair.color[3] = 1.0;

	// pulse the crosshair size and alpha based on pickups
	if (cg_draw_crosshair_pulse->value) {
		// determine if we've picked up an item
		const int16_t p = ps->stats[STAT_PICKUP_ICON];

		if (p != -1 && (p != cg_hud_locals.pickup_pulse)) {
			cg_hud_locals.last_pulse_time = cgi.client->unclamped_time;
		}

		cg_hud_locals.pickup_pulse = p;

		const vec_t delta = 1.0 - ((cgi.client->unclamped_time - cg_hud_locals.last_pulse_time) / 500.0);

		if (delta > 0.0) {
			scale += cg_draw_crosshair_pulse->value * 0.5 * delta;
			crosshair.color[3] -= 0.5 * delta;
		}
	}

	cgi.Color(crosshair.color);

	// calculate width and height based on crosshair image and scale
	x = (cgi.context->width - crosshair.image->width * scale) / 2.0;
	y = (cgi.context->height - crosshair.image->height * scale) / 2.0;

	cgi.DrawImage(x, y, scale, crosshair.image);

	cgi.Color(NULL);
}

/**
 * @brief
 */
void Cg_ParseCenterPrint(void) {
	char *c, *out, *line;

	memset(&center_print, 0, sizeof(center_print));

	c = cgi.ReadString();

	line = center_print.lines[0];
	out = line;

	while (*c && center_print.num_lines < CENTER_PRINT_LINES - 1) {

		if (*c == '\n') {
			line += MAX_STRING_CHARS;
			out = line;
			center_print.num_lines++;
			c++;
			continue;
		}

		*out++ = *c++;
	}

	center_print.num_lines++;
	center_print.time = cgi.client->unclamped_time + 3000;
}

/**
 * @brief
 */
static void Cg_DrawCenterPrint(const player_state_t *ps) {
	r_pixel_t cw, ch, x, y;
	char *line = center_print.lines[0];

	if (ps->stats[STAT_SCORES]) {
		return;
	}

	if (center_print.time < cgi.client->unclamped_time) {
		return;
	}

	cgi.BindFont(NULL, &cw, &ch);

	y = (cgi.context->height - center_print.num_lines * ch) / 2;

	while (*line) {
		x = (cgi.context->width - cgi.StringWidth(line)) / 2;

		cgi.DrawString(x, y, line, CON_COLOR_DEFAULT);
		line += MAX_STRING_CHARS;
		y += ch;
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Perform composition of the dst/src blends.
 */
static void Cg_AddBlend(vec4_t blend, const vec4_t input) {

	if (input[3] <= 0.0) {
		return;
	}

	vec4_t out;

	out[3] = input[3] + blend[3] * (1.0 - input[3]);

	for (int32_t i = 0; i < 3; i++) {
		out[i] = ((input[i] * input[3]) + ((blend[i] * blend[3]) * (1.0 - input[3]))) / out[3];
	}

	Vector4Copy(out, blend);
}

/**
 * @brief Perform composition of the src blend with the specified color palette index/alpha combo.
 */
static void Cg_AddBlendPalette(vec4_t blend, const uint8_t color, const vec_t alpha) {

	if (alpha <= 0.0) {
		return;
	}

	vec4_t blend_color;
	cgi.ColorFromPalette(color, blend_color);
	blend_color[3] = alpha;

	Cg_AddBlend(blend, blend_color);
}

/**
 * @brief Calculate the alpha factor for the specified blend components.
 * @param blend_start_time The start of the blend, in unclamped time.
 * @param blend_decay_time The length of the blend in milliseconds.
 * @param blend_alpha The base alpha value.
 */
static vec_t Cg_CalculateBlendAlpha(const uint32_t blend_start_time, const uint32_t blend_decay_time,
                                    const vec_t blend_alpha) {

	if ((cgi.client->unclamped_time - blend_start_time) <= blend_decay_time) {
		const vec_t time_factor = (vec_t) (cgi.client->unclamped_time - blend_start_time) / blend_decay_time;
		const vec_t alpha = cg_draw_blend->value * (blend_alpha - (time_factor * blend_alpha));

		return alpha;
	}

	return 0.0;
}

#define CG_DAMAGE_BLEND_TIME 500
#define CG_DAMAGE_BLEND_ALPHA 0.3
#define CG_PICKUP_BLEND_TIME 500
#define CG_PICKUP_BLEND_ALPHA 0.3

/**
 * @brief Draw a full-screen blend effect based on world interaction.
 */
static void Cg_DrawBlend(const player_state_t *ps) {

	if (!cg_draw_blend->value) {
		return;
	}

	vec4_t blend = { 0, 0, 0, 0 };
	uint8_t color;

	// start with base blend based on view origin conents
	const int32_t contents = cgi.view->contents;

	if (contents & MASK_LIQUID) {
		if (contents & CONTENTS_LAVA) {
			color = 71;
		} else if (contents & CONTENTS_SLIME) {
			color = 201;
		} else {
			color = 114;
		}

		Cg_AddBlendPalette(blend, color, 0.3);
	}

	// add supplementary blends.
	// pickups
	const int16_t p = ps->stats[STAT_PICKUP_ICON] & ~STAT_TOGGLE_BIT;

	if (p > -1 && (p != cg_hud_locals.pickup)) { // don't flash on same item
		cg_hud_locals.last_pickup_time = cgi.client->unclamped_time;
	}

	cg_hud_locals.pickup = p;

	if (cg_hud_locals.last_pickup_time) {
		Cg_AddBlendPalette(blend, 215, Cg_CalculateBlendAlpha(cg_hud_locals.last_pickup_time, CG_PICKUP_BLEND_TIME,
		                   CG_PICKUP_BLEND_ALPHA));
	}

	// taken damage
	const int16_t d = ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH];

	if (d) {
		cg_hud_locals.last_damage_time = cgi.client->unclamped_time;
	}

	if (cg_hud_locals.last_damage_time) {
		Cg_AddBlendPalette(blend, 240, Cg_CalculateBlendAlpha(cg_hud_locals.last_damage_time, CG_DAMAGE_BLEND_TIME,
		                   CG_DAMAGE_BLEND_ALPHA));
	}

	// if we have a blend, draw it
	if (blend[3] > 0.0) {
		color_t final_color;

		for (int32_t i = 0; i < 4; i++) {
			final_color.bytes[i] = (uint8_t) (blend[i] * 255.0);
		}

		cgi.DrawFill(cgi.view->viewport.x, cgi.view->viewport.y,
		             cgi.view->viewport.w, cgi.view->viewport.h, final_color.u32, -1.0);
	}
}

/**
 * @brief Plays the hit sound if the player inflicted damage this frame.
 */
static void Cg_DrawDamageInflicted(const player_state_t *ps) {

	const int16_t dmg = ps->stats[STAT_DAMAGE_INFLICT];
	if (dmg) {

		// play the hit sound
		if (cgi.client->unclamped_time - cg_hud_locals.last_hit_sound_time > 50) {
			cg_hud_locals.last_hit_sound_time = cgi.client->unclamped_time;

			cgi.AddSample(&(const s_play_sample_t) {
				.sample = dmg >= 25 ? cg_sample_hits[1] : cg_sample_hits[0]
			});
		}
	}
}

/**
 * @brief Draws the HUD for the current frame.
 */
void Cg_DrawHud(const player_state_t *ps) {

	if (!cg_draw_hud->integer) {
		return;
	}

	if (!ps->stats[STAT_TIME]) { // intermission
		return;
	}

	Cg_DrawVitals(ps);

	Cg_DrawPowerups(ps);

	Cg_DrawHeldFlag(ps);

	Cg_DrawPickup(ps);

	Cg_DrawTeam(ps);

	Cg_DrawFrags(ps);

	Cg_DrawDeaths(ps);

	Cg_DrawCaptures(ps);

	Cg_DrawSpectator(ps);

	Cg_DrawChase(ps);

	Cg_DrawVote(ps);

	Cg_DrawTime(ps);

	Cg_DrawReady(ps);

	Cg_DrawCrosshair(ps);

	Cg_DrawCenterPrint(ps);

	Cg_DrawBlend(ps);

	Cg_DrawDamageInflicted(ps);
}

/**
 * @brief Clear HUD-related state.
 */
void Cg_ClearHud(void) {
	memset(&cg_hud_locals, 0, sizeof(cg_hud_locals));
}
