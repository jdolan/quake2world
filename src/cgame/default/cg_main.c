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

/**
 * @brief Local state to the cgame per server.
 */
typedef struct {
	vec_t hook_pull_speed;
} cg_state_t;

static cg_state_t cg_state;

cvar_t *cg_add_emits;
cvar_t *cg_add_entities;
cvar_t *cg_add_particles;
cvar_t *cg_add_weather;
cvar_t *cg_auto_switch;
cvar_t *cg_bob;
cvar_t *cg_color;
cvar_t *cg_draw_blend;
cvar_t *cg_draw_blend_damage;
cvar_t *cg_draw_blend_liquid;
cvar_t *cg_draw_blend_pickup;
cvar_t *cg_draw_blend_powerup;
cvar_t *cg_draw_captures;
cvar_t *cg_draw_crosshair;
cvar_t *cg_draw_crosshair_alpha;
cvar_t *cg_draw_crosshair_color;
cvar_t *cg_draw_crosshair_health;
cvar_t *cg_draw_crosshair_pulse;
cvar_t *cg_draw_crosshair_scale;
cvar_t *cg_draw_held_flag;
cvar_t *cg_draw_held_tech;
cvar_t *cg_draw_frags;
cvar_t *cg_draw_deaths;
cvar_t *cg_draw_hud;
cvar_t *cg_draw_pickup;
cvar_t *cg_draw_powerups;
cvar_t *cg_draw_time;
cvar_t *cg_draw_target_name;
cvar_t *cg_draw_team_banner;
cvar_t *cg_draw_weapon;
cvar_t *cg_draw_weapon_alpha;
cvar_t *cg_draw_weapon_bob;
cvar_t *cg_draw_weapon_x;
cvar_t *cg_draw_weapon_y;
cvar_t *cg_draw_weapon_z;
cvar_t *cg_draw_vitals;
cvar_t *cg_draw_vitals_pulse;
cvar_t *cg_draw_vote;
cvar_t *cg_entity_bob;
cvar_t *cg_entity_pulse;
cvar_t *cg_entity_rotate;
cvar_t *cg_fov;
cvar_t *cg_fov_zoom;
cvar_t *cg_fov_interpolate;
cvar_t *cg_hand;
cvar_t *cg_handicap;
cvar_t *cg_helmet;
cvar_t *cg_hit_sound;
cvar_t *cg_hook_style;
cvar_t *cg_pants;
cvar_t *cg_particle_quality;
cvar_t *cg_predict;
cvar_t *cg_quick_join_max_ping;
cvar_t *cg_quick_join_min_clients;
cvar_t *cg_shirt;
cvar_t *cg_skin;
cvar_t *cg_third_person;
cvar_t *cg_third_person_chasecam;
cvar_t *cg_third_person_x;
cvar_t *cg_third_person_y;
cvar_t *cg_third_person_z;
cvar_t *cg_third_person_pitch;
cvar_t *cg_third_person_yaw;

cvar_t *g_gameplay;
cvar_t *g_teams;
cvar_t *g_ctf;
cvar_t *g_match;
cvar_t *g_ai_max_clients;

cg_import_t cgi;

/**
 * @brief Called when the client first comes up or switches game directories. Client
 * game modules should bootstrap any globals they require here.
 */
static void Cg_Init(void) {

	cgi.Print("  ^6Client game module initialization...\n");

	const char *s = va("%s %s %s", VERSION, BUILD_HOST, REVISION);
	cvar_t *cgame_version = cgi.AddCvar("cgame_version", s, CVAR_NO_SET, NULL);

	cgi.Print("  ^6Version %s\n", cgame_version->string);

	Cg_InitInput();

	cg_add_emits = cgi.AddCvar("cg_add_emits", "1", 0,
	                        "Toggles adding client-side entities to the scene.");
	cg_add_entities = cgi.AddCvar("cg_add_entities", "1", 0, "Toggles adding entities to the scene.");
	cg_add_particles = cgi.AddCvar("cg_add_particles", "1", 0,
	                            "Toggles adding particles to the scene.");
	cg_add_weather = cgi.AddCvar("cg_add_weather", "1", CVAR_ARCHIVE,
	                          "Control the intensity of atmospheric effects.");

	cg_auto_switch = cgi.AddCvar("auto_switch", "1", CVAR_USER_INFO | CVAR_ARCHIVE,
				 "The weapon pickup auto-switch method. 0 disables, 1 switches from Blaster only,"
				 " 2 always switches, 3 switches to new weapons.");

	cg_bob = cgi.AddCvar("cg_bob", "1", CVAR_ARCHIVE, "Controls weapon bobbing effect.");

	cg_color = cgi.AddCvar("color", "default", CVAR_USER_INFO | CVAR_ARCHIVE,
	                    "Specifies the effect color for your own weapon trails.");

	cg_shirt = cgi.AddCvar("shirt", "default", CVAR_USER_INFO | CVAR_ARCHIVE,
	                    "Specifies your shirt color, in the hex format \"rrggbb\". \"default\" uses the skin or team's defaults.");

	cg_pants = cgi.AddCvar("pants", "default", CVAR_USER_INFO | CVAR_ARCHIVE,
	                    "Specifies your pants color, in the hex format \"rrggbb\". \"default\" uses the skin or team's defaults.");

	cg_helmet = cgi.AddCvar("helmet", "default", CVAR_USER_INFO | CVAR_ARCHIVE,
	                    "Specifies your helmet color, in the hex format \"rrggbb\". \"default\" uses the skin or team's defaults.");

	cg_draw_blend = cgi.AddCvar("cg_draw_blend", "1", CVAR_ARCHIVE,
                                 "Controls the intensity of screen alpha-blending.");
	cg_draw_blend_damage = cgi.AddCvar("cg_draw_blend_damage", "1", CVAR_ARCHIVE,
                                        "Controls the intensity of the blend flash effect when taking damage.");
	cg_draw_blend_liquid = cgi.AddCvar("cg_draw_blend_liquid", "1", CVAR_ARCHIVE,
                                        "Controls the intensity of the blend effect while in a liquid.");
	cg_draw_blend_pickup = cgi.AddCvar("cg_draw_blend_pickup", "1", CVAR_ARCHIVE,
                                        "Controls the intensity of the blend flash effect when picking up items.");
	cg_draw_blend_powerup = cgi.AddCvar("cg_draw_blend_powerup", "1", CVAR_ARCHIVE,
                                         "Controls the intensity of the blend flash effect when holding a powerup.");
	cg_draw_captures = cgi.AddCvar("cg_draw_captures", "1", CVAR_ARCHIVE,
	                            "Draw the number of captures");
	cg_draw_crosshair = cgi.AddCvar("cg_draw_crosshair", "1", CVAR_ARCHIVE,
                                     "Which crosshair image to use, 0 disables (Default is 1)");
	cg_draw_crosshair_alpha = cgi.AddCvar("cg_draw_crosshair_alpha", "1.0", CVAR_ARCHIVE,
                                	      "Opacity of the crosshair");
	cg_draw_crosshair_color = cgi.AddCvar("cg_draw_crosshair_color", "default", CVAR_ARCHIVE,
	                                   "Specifies your crosshair color, in the hex format \"rrggbb\". \"default\" uses white.");
	cg_draw_crosshair_health = cgi.AddCvar("cg_draw_crosshair_health", "0", CVAR_ARCHIVE,
	                                     "Method of coloring the crosshair by health. Range from 1-5, 0 disables.");
	cg_draw_crosshair_pulse = cgi.AddCvar("cg_draw_crosshair_pulse", "1", CVAR_ARCHIVE,
	                                   "Pulse the crosshair when picking up items");
	cg_draw_crosshair_scale = cgi.AddCvar("cg_draw_crosshair_scale", "1", CVAR_ARCHIVE,
	                                   "Controls the crosshair scale (size)");

	cg_draw_held_flag = cgi.AddCvar("cg_draw_held_flag", "1", CVAR_ARCHIVE, "Draw the currently held team flag");
	cg_draw_held_tech = cgi.AddCvar("cg_draw_held_tech", "1", CVAR_ARCHIVE, "Draw the currently held tech");
	cg_draw_frags = cgi.AddCvar("cg_draw_frags", "1", CVAR_ARCHIVE, "Draw the number of frags");
	cg_draw_deaths = cgi.AddCvar("cg_draw_deaths", "1", CVAR_ARCHIVE, "Draw the number of deaths");
	cg_draw_hud = cgi.AddCvar("cg_draw_hud", "1", CVAR_ARCHIVE, "Render the Heads-Up-Display");
	cg_draw_pickup = cgi.AddCvar("cg_draw_pickup", "1", CVAR_ARCHIVE, "Draw the current pickup");
	cg_draw_time = cgi.AddCvar("cg_draw_time", "1", CVAR_ARCHIVE, "Draw the time remaning");
	cg_draw_target_name = cgi.AddCvar("cg_draw_target_name", "1", CVAR_ARCHIVE, "Draw the target's name");
	cg_draw_team_banner = cgi.AddCvar("cg_draw_team_banner", "1", CVAR_ARCHIVE, "Draw the team banner");
	cg_draw_powerups = cgi.AddCvar("cg_draw_powerups", "1", CVAR_ARCHIVE,
	                            "Draw currently active powerups, such as Quad Damage and Adrenaline.");

	cg_draw_weapon = cgi.AddCvar("cg_draw_weapon", "1", CVAR_ARCHIVE,
	                          "Toggle drawing of the weapon model.");
	cg_draw_weapon_alpha = cgi.AddCvar("cg_draw_weapon_alpha", "1", CVAR_ARCHIVE,
	                                "The alpha transparency for drawing the weapon model.");
	cg_draw_weapon_bob = cgi.AddCvar("cg_draw_weapon_bob", "1", CVAR_ARCHIVE,
	                                "If the weapon model bobs while moving.");
	cg_draw_weapon_x = cgi.AddCvar("cg_draw_weapon_x", "0", CVAR_ARCHIVE,
	                            "The x offset for drawing the weapon model.");
	cg_draw_weapon_y = cgi.AddCvar("cg_draw_weapon_y", "0", CVAR_ARCHIVE,
	                            "The y offset for drawing the weapon model.");
	cg_draw_weapon_z = cgi.AddCvar("cg_draw_weapon_z", "0", CVAR_ARCHIVE,
	                            "The z offset for drawing the weapon model.");
	cg_draw_vitals = cgi.AddCvar("cg_draw_vitals", "1", CVAR_ARCHIVE,
	                          "Draw the vitals (health, armor, ammo)");
	cg_draw_vitals_pulse = cgi.AddCvar("cg_draw_vitals_pulse", "1", CVAR_ARCHIVE,
	                                "Pulse the vitals when low");
	cg_draw_vote = cgi.AddCvar("cg_draw_vote", "1", CVAR_ARCHIVE, "Draw the current vote on the hud");

	cg_entity_bob = cgi.AddCvar("cg_entity_bob", "1", CVAR_ARCHIVE, "Controls the bobbing of items");
	cg_entity_pulse = cgi.AddCvar("cg_entity_pulse", "1", CVAR_ARCHIVE, "Controls the pulsing of items");
	cg_entity_rotate = cgi.AddCvar("cg_entity_rotate", "1", CVAR_ARCHIVE, "Controls the rotation of items");

	cg_fov = cgi.AddCvar("cg_fov", "110", CVAR_ARCHIVE, "Horizontal field of view, in degrees");
	cg_fov_zoom = cgi.AddCvar("cg_fov_zoom", "55", CVAR_ARCHIVE, "Zoomed in field of view");
	cg_fov_interpolate = cgi.AddCvar("cg_fov_interpolate", "1", CVAR_ARCHIVE,
	                              "Interpolate between field of view changes (default 1.0).");

	cg_hand = cgi.AddCvar("hand", "1", CVAR_ARCHIVE | CVAR_USER_INFO,
	                   "Controls weapon handedness (center: 0, right: 1, left: 2).");
	cg_handicap = cgi.AddCvar("handicap", "100", CVAR_USER_INFO | CVAR_ARCHIVE,
	                       "Your handicap, or disadvantage.");

	cg_hit_sound = cgi.AddCvar("cg_hit_sound", "1", CVAR_ARCHIVE,
	                       "If a hit sound is played when damaging an enemy.");

	cg_hook_style = cgi.AddCvar("hook_style", "pull", CVAR_USER_INFO | CVAR_ARCHIVE,
	                         "Your preferred hook style. Can be either \"pull\" or \"swing\".");

	cg_particle_quality = cgi.AddCvar("cg_particle_quality", "1", CVAR_ARCHIVE, "Particle quality. 0 disables most eyecandy particles, 1 enables all.");

	cg_predict = cgi.AddCvar("cg_predict", "1", 0, "Use client side movement prediction");

	cg_quick_join_max_ping = cgi.AddCvar("cg_quick_join_max_ping", "200", CVAR_SERVER_INFO,
									  "Maximum ping allowed for quick join");
	cg_quick_join_min_clients = cgi.AddCvar("cg_quick_join_min_clients", "1", CVAR_SERVER_INFO,
										 "Minimum clients allowed for quick join");

	cg_skin = cgi.AddCvar("skin", "qforcer/default", CVAR_USER_INFO | CVAR_ARCHIVE,
	                   "Your player model and skin.");

	cg_third_person = cgi.AddCvar("cg_third_person", "0", CVAR_ARCHIVE | CVAR_DEVELOPER,
	                           "Activate third person perspective.");
	cg_third_person_chasecam = cgi.AddCvar("cg_third_person_chasecam", "0", CVAR_ARCHIVE,
	                                    "Activate third person chase camera perspective.");
	cg_third_person_x = cgi.AddCvar("cg_third_person_x", "-200", CVAR_ARCHIVE,
	                             "The x offset for third person perspective.");
	cg_third_person_y = cgi.AddCvar("cg_third_person_y", "0", CVAR_ARCHIVE,
	                             "The y offset for third person perspective.");
	cg_third_person_z = cgi.AddCvar("cg_third_person_z", "40", CVAR_ARCHIVE,
	                             "The z offset for third person perspective.");
	cg_third_person_pitch = cgi.AddCvar("cg_third_person_pitch", "0", CVAR_ARCHIVE,
	                                 "The pitch offset for third person perspective.");
	cg_third_person_yaw = cgi.AddCvar("cg_third_person_yaw", "0", CVAR_ARCHIVE,
	                               "The yaw offset for third person perspective.");

	g_gameplay = cgi.AddCvar("g_gameplay", "default", CVAR_SERVER_INFO,
	                      "Selects deathmatch, duel, arena, or instagib combat");
	g_teams = cgi.AddCvar("g_teams", "0", CVAR_SERVER_INFO,
	                   "Enables teams-based play");
	g_ctf = cgi.AddCvar("g_ctf", "0", CVAR_SERVER_INFO,
	                 "Enables capture the flag gameplay");
	g_match = cgi.AddCvar("g_match", "0", CVAR_SERVER_INFO,
	                   "Enables match play requiring players to ready");
	g_ai_max_clients = cgi.AddCvar("g_ai_max_clients", "0", CVAR_SERVER_INFO,
	                           "The minimum amount player slots that will always be filled. Specify -1 to fill all available slots.");

	// add forward to server commands for tab completion

	cgi.AddCmd("wave", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("kill", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("use", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("drop", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("say", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("say_team", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("info", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("give", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("god", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("no_clip", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("weapon_last", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("vote", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("team", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("team_name", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("team_skin", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("spectate", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("join", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("ready", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("unready", NULL, CMD_CGAME, NULL);
	cgi.AddCmd("player_list", NULL, CMD_CGAME, NULL);

	Cg_InitUi();

	Cg_InitHud();

	cgi.Print("  ^6Client game module initialized\n");
}

/**
 * @brief Called when switching game directories or quitting.
 */
static void Cg_Shutdown(void) {

	cgi.Print("  ^6Client game module shutdown...\n");

	Cg_ShutdownUi();

	cgi.FreeTag(MEM_TAG_CGAME_LEVEL);
	cgi.FreeTag(MEM_TAG_CGAME);
}

/**
 * @brief Parse a single server command, returning true on success.
 */
static _Bool Cg_ParseMessage(int32_t cmd) {

	switch (cmd) {
		case SV_CMD_TEMP_ENTITY:
			Cg_ParseTempEntity();
			return true;

		case SV_CMD_MUZZLE_FLASH:
			Cg_ParseMuzzleFlash();
			return true;

		case SV_CMD_SCORES:
			Cg_ParseScores();
			return true;

		case SV_CMD_CENTER_PRINT:
			Cg_ParseCenterPrint();
			return true;

		case SV_CMD_VIEW_KICK:
			Cg_ParseViewKick();
			return true;

		default:
			break;
	}

	return false;
}

/**
 * @brief
 */
static void Cg_UpdateScreen(const cl_frame_t *frame) {

	Cg_DrawHud(&frame->ps);

	Cg_DrawScores(&frame->ps);
}

/**
 * @brief Fetch the server's reported hook pull speed.
 */
vec_t Cg_GetHookPullSpeed(void) {

	return cg_state.hook_pull_speed;
}

/**
 * @brief Clear any state that should not persist over multiple server connections.
 */
static void Cg_ClearState(void) {

	memset(&cg_state, 0, sizeof(cg_state));

	Cg_ClearInput();

	Cg_FreeEmits();

	Cg_ClearHud();
}

cg_team_info_t cg_team_info[MAX_TEAMS];

/**
 * @brief Resolve team info from team configstring
 */
static void Cg_ParseTeamInfo(const char *s) {

	gchar **info = g_strsplit(s, "\\", 0);
	const size_t count = g_strv_length(info);

	if (count != lengthof(cg_team_info) * 2) {
		g_strfreev(info);
		cgi.Error("Invalid team data");
	}

	cg_team_info_t *team = cg_team_info;

	for (size_t i = 0; i < count; i += 2, team++) {

		g_strlcpy(team->team_name, info[i], sizeof(team->team_name));

		const int16_t hue = atoi(info[i + 1]);
		const SDL_Color color = MVC_HSVToRGB(hue, 1.0, 1.0);
		team->color.u32 = *(int32_t *) &color;
	}

	g_strfreev(info);
}

/**
 * @brief An updated configuration string has just been received from the server.
 * Refresh related variables and media that aren't managed by the engine.
 */
static void Cg_UpdateConfigString(uint16_t i) {

	const char *s = cgi.ConfigString(i);

	switch (i) {
		case CS_WEATHER:
			Cg_ResolveWeather(s);
			return;
		case CS_HOOK_PULL_SPEED:
			cg_state.hook_pull_speed = strtof(s, NULL);
			return;
		case CS_TEAM_INFO:
			Cg_ParseTeamInfo(s);
			return;
		case CS_WEAPONS:
			Cg_ParseWeaponInfo(s);
			return;
		default:
			break;
	}

	if (i >= CS_CLIENTS && i < CS_CLIENTS + MAX_CLIENTS) {

		cl_client_info_t *ci = &cgi.client->client_info[i - CS_CLIENTS];
		Cg_LoadClient(ci, s);

		cl_entity_t *ent = &cgi.client->entities[i - CS_CLIENTS + 1];

		ent->animation1.time = ent->animation2.time = 0;
		ent->animation1.frame = ent->animation2.frame = -1;
	}
}

/**
 * @brief
 */
cg_export_t *Cg_LoadCgame(cg_import_t *import) {
	static cg_export_t cge;

	cgi = *import;

	cge.api_version = CGAME_API_VERSION;
	cge.protocol = PROTOCOL_MINOR;

	cge.Init = Cg_Init;
	cge.Shutdown = Cg_Shutdown;
	cge.ClearState = Cg_ClearState;
	cge.Look = Cg_Look;
	cge.Move = Cg_Move;
	cge.UpdateMedia = Cg_UpdateMedia;
	cge.UpdateConfigString = Cg_UpdateConfigString;
	cge.ParseMessage = Cg_ParseMessage;
	cge.Interpolate = Cg_Interpolate;
	cge.UsePrediction = Cg_UsePrediction;
	cge.PredictMovement = Cg_PredictMovement;
	cge.UpdateLoading = Cg_UpdateLoading;
	cge.UpdateView = Cg_UpdateView;
	cge.UpdateScreen = Cg_UpdateScreen;

	return &cge;
}
