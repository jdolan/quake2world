/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "cl_keys.h"
#include "libs/cmodel.h"
#include "libs/console.h"
#include "libs/filesystem.h"
#include "libs/mem.h"
#include "libs/net.h"
#include "libs/sys.h"
#include "renderer/renderer.h"
#include "sound/sound.h"

typedef struct cl_frame_s {
	qboolean valid;  // cleared if delta parsing was invalid
	int serverframe;
	int servertime;  // server time the message is valid for (in msec)
	int deltaframe;
	byte areabits[MAX_BSP_AREAS / 8];  // portal area visibility bits
	player_state_t playerstate;
	int num_entities;
	int parse_entities;  // non-masked index into cl_parse_entities array
} cl_frame_t;

typedef struct cl_entity_s {
	entity_state_t baseline;  // delta from this if not from a previous frame
	entity_state_t current;
	entity_state_t prev;  // will always be valid, but might just be a copy of current

	int serverframe;  // if not current, this ent isn't in the frame

	int time;  // for intermittent effects

	int anim_time;  // for animations
	int anim_frame;

	static_lighting_t lighting;  // cached static lighting info
} cl_entity_t;

#define MAX_CLIENTWEAPONMODELS 12

typedef struct clientinfo_s {
	char name[MAX_QPATH];
	char cinfo[MAX_QPATH];
	struct image_s *skin;
	struct model_s *model;
	struct model_s *weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

#define CMD_BACKUP 128  // allow a lot of command backups for very fast systems
#define CMD_MASK (CMD_BACKUP - 1)

// the client_state_s structure is wiped completely at every
// server map change
typedef struct client_state_s {
	int timedemo_frames;
	int timedemo_start;

	usercmd_t cmds[CMD_BACKUP];  // each mesage will send several old cmds
	int cmd_time[CMD_BACKUP];  // time sent, for calculating pings

	vec_t predicted_step;
	int predicted_step_time;

	vec3_t predicted_origin;  // generated by Cl_PredictMovement
	vec3_t predicted_angles;
	vec3_t prediction_error;
	short predicted_origins[CMD_BACKUP][3];  // for debug comparing against server

	qboolean underwater;  // updated by client sided prediction

	cl_frame_t frame;  // received from server
	cl_frame_t frames[UPDATE_BACKUP];  // for calculating delta compression

	int parse_entities;  // index (not anded off) into cl_parse_entities[]
	int playernum;  // our entity number

	int surpress_count;  // number of messages rate supressed

	int time;  // this is the server time value that the client
	// is rendering at.  always <= cls.realtime due to latency

	float lerp;  // linear interpolation between frames

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta when necessary which is added to the locally
	// tracked view angles to account for spawn and teleport direction changes
	vec3_t angles;

	char layout[MAX_STRING_CHARS];  // general 2D overlay

	int servercount;  // server identification for prespawns
	int serverrate;  // server tick rate (fps)

	qboolean demoserver;  // we're viewing a demo

	char gamedir[MAX_QPATH];
	char configstrings[MAX_CONFIGSTRINGS][MAX_STRING_CHARS];

	// locally derived information from server state
	model_t *model_draw[MAX_MODELS];
	cmodel_t *model_clip[MAX_MODELS];

	s_sample_t *sound_precache[MAX_SOUNDS];
	image_t *image_precache[MAX_IMAGES];

	clientinfo_t clientinfo[MAX_CLIENTS];
	clientinfo_t baseclientinfo;
} client_state_t;

extern client_state_t cl;

// the client_static_t structure is persistent through an arbitrary
// number of server connections

typedef enum {
	ca_uninitialized,
	ca_disconnected,    // not talking to a server
	ca_connecting,   // sending request packets to the server
	ca_connected,   // netchan_t established, waiting for svc_serverdata
	ca_active  // game views should be displayed
} connection_state_t;

typedef enum {
	key_game,
	key_menu,
	key_console,
	key_message
} key_dest_t;

typedef struct {
	char lines[KEY_HISTORYSIZE][KEY_LINESIZE];
	int pos;

	qboolean insert;

	unsigned edit_line;
	unsigned history_line;

	char *binds[K_LAST];
	qboolean down[K_LAST];
} key_state_t;

typedef struct {
	float x, y;
	float old_x, old_y;
	qboolean grabbed;
} mouse_state_t;

typedef struct {
	char buffer[KEY_LINESIZE];
	size_t len;
	qboolean team;
} chat_state_t;

typedef struct {
	qboolean http;
	FILE *file;
	char tempname[MAX_OSPATH];
	char name[MAX_OSPATH];
} download_t;

typedef enum {
	SERVER_SOURCE_INTERNET,
	SERVER_SOURCE_USER,
	SERVER_SOURCE_BCAST
} server_source_t;

typedef struct server_info_s {
	netaddr_t addr;
	server_source_t source;
	int pingtime;
	int ping;
	char info[MAX_MSGLEN];
	int num;
	struct server_info_s *next;
} server_info_t;

#define MAX_SERVER_INFOS 128

typedef struct {
	connection_state_t state;

	key_dest_t key_dest;

	key_state_t key_state;

	mouse_state_t mouse_state;

	chat_state_t chat_state;

	int realtime;  // always increasing, no clamping, etc

	int packet_delta;  // millis since last outgoing packet
	int render_delta;  // millis since last renderer frame

	// connection information
	char servername[MAX_OSPATH];  // name of server to connect to
	float connect_time;  // for connection retransmits

	netchan_t netchan;  // network channel

	int challenge;  // from the server to use for connecting

	int loading;  // loading percentage indicator

	char downloadurl[MAX_OSPATH];  // for http downloads
	download_t download;  // current download (udp or http)

	// demo recording info must be here, so it isn't cleared on level change
	qboolean demowaiting;  // don't record until a non-delta message is received
	FILE *demofile;

	server_info_t *servers;  // list of servers from all sources
	char *servers_text;  // tabular data for servers menu

	int bcasttime;  // time when last broadcast ping was sent
} client_static_t;

extern client_static_t cls;

// cvars
extern cvar_t *cl_addentities;
extern cvar_t *cl_addparticles;
extern cvar_t *cl_async;
extern cvar_t *cl_blend;
extern cvar_t *cl_bob;
extern cvar_t *cl_chatsound;
extern cvar_t *cl_counters;
extern cvar_t *cl_crosshair;
extern cvar_t *cl_crosshaircolor;
extern cvar_t *cl_crosshairscale;
extern cvar_t *cl_emits;
extern cvar_t *cl_fov;
extern cvar_t *cl_fovzoom;
extern cvar_t *cl_hud;
extern cvar_t *cl_ignore;
extern cvar_t *cl_maxfps;
extern cvar_t *cl_maxpps;
extern cvar_t *cl_netgraph;
extern cvar_t *cl_predict;
extern cvar_t *cl_showmiss;
extern cvar_t *cl_shownet;
extern cvar_t *cl_teamchatsound;
extern cvar_t *cl_thirdperson;
extern cvar_t *cl_timeout;
extern cvar_t *cl_viewsize;
extern cvar_t *cl_weapon;
extern cvar_t *cl_weather;

extern cvar_t *rcon_password;
extern cvar_t *rcon_address;

// userinfo
extern cvar_t *color;
extern cvar_t *msg;
extern cvar_t *name;
extern cvar_t *password;
extern cvar_t *rate;
extern cvar_t *skin;

extern cvar_t *recording;

// cl_screen.c
void Cl_CenterPrint(char *s);
void Cl_AddNetgraph(void);
void Cl_UpdateScreen(void);

// cl_emit.c
void Cl_LoadEmits(void);
void Cl_AddEmits(void);

// cl_entity.c
unsigned int Cl_ParseEntityBits(unsigned int *bits);
void Cl_ParseDelta(const entity_state_t *from, entity_state_t *to, int number, int bits);
void Cl_ParseFrame(void);
void Cl_AddEntities(cl_frame_t *frame);

// cl_tentity.c
void Cl_LoadTempEntitySamples(void);
void Cl_ParseTempEntity(void);

// cl_main.c
void Cl_Init(void);
void Cl_Frame(int msec);
void Cl_Shutdown(void);
void Cl_Drop(void);
void Cl_SendDisconnect(void);
void Cl_Disconnect(void);
void Cl_Reconnect_f(void);
void Cl_LoadProgress(int percent);
void Cl_RequestNextDownload(void);
void Cl_WriteDemoMessage(void);

// cl_input.c
void Cl_InitInput(void);
void Cl_HandleEvents(void);
void Cl_Move(usercmd_t *cmd);

// cl_cmd.c
void Cl_UpdateCmd(void);
void Cl_SendCmd(void);

// cl_console.c
extern console_t cl_con;

void Cl_InitConsole(void);
void Cl_DrawConsole(void);
void Cl_DrawNotify(void);
void Cl_UpdateNotify(int lastline);
void Cl_ClearNotify(void);
void Cl_ToggleConsole_f(void);

// cl_keys.c
void Cl_KeyEvent(unsigned int ascii, unsigned short unicode, qboolean down, unsigned time);
char *Cl_EditLine(void);
void Cl_WriteBindings(FILE *f);
void Cl_InitKeys(void);
void Cl_ShutdownKeys(void);
void Cl_ClearTyping(void);

// cl_parse.c
extern cl_entity_t cl_entities[MAX_EDICTS];

#define MAX_PARSE_ENTITIES 16384
extern entity_state_t cl_parse_entities[MAX_PARSE_ENTITIES];

extern netaddr_t net_from;
extern sizebuf_t net_message;

extern char *svc_strings[256];

qboolean Cl_CheckOrDownloadFile(const char *filename);
void Cl_ParseConfigstring(void);
void Cl_ParseClientinfo(int player);
void Cl_ParseMuzzleFlash(void);
void Cl_ParseServerMessage(void);
void Cl_LoadClientinfo(clientinfo_t *ci, const char *s);
void Cl_Download_f(void);

// cl_view.c
void Cl_InitView(void);
void Cl_ClearState(void);
void Cl_AddEntity(entity_t *ent);
void Cl_AddParticle(particle_t *p);
void Cl_UpdateView(void);

// cl_pred.c
extern int cl_gravity;
void Cl_PredictMovement(void);
void Cl_CheckPredictionError(void);

// cl_effect.c
void Cl_LightningEffect(const vec3_t org);
void Cl_LightningTrail(const vec3_t start, const vec3_t end);
void Cl_EnergyTrail(cl_entity_t *ent, float radius, int color);
void Cl_BFGEffect(const vec3_t org);
void Cl_BubbleTrail(const vec3_t start, const vec3_t end, float density);
void Cl_EntityEvent(entity_state_t *ent);
void Cl_BulletTrail(const vec3_t start, const vec3_t end);
void Cl_BulletEffect(const vec3_t org, const vec3_t dir);
void Cl_BurnEffect(const vec3_t org, const vec3_t dir, int scale);
void Cl_BloodEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_GibEffect(const vec3_t org, int count);
void Cl_SparksEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_RailTrail(const vec3_t start, const vec3_t end, int color);
void Cl_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_SmokeFlash(entity_state_t *ent);
void Cl_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent);
void Cl_ExplosionEffect(const vec3_t org);
void Cl_ItemRespawnEffect(const vec3_t org);
void Cl_TeleporterTrail(const vec3_t org, cl_entity_t *ent);
void Cl_LogoutEffect(const vec3_t org);
void Cl_LoadEffectSamples(void);
void Cl_AddParticles(void);
particle_t *Cl_AllocParticle(void);
void Cl_ClearEffects(void);

// cl_http.c
void Cl_InitHttpDownload(void);
void Cl_HttpDownloadCleanup(void);
qboolean Cl_HttpDownload(void);
void Cl_HttpDownloadThink(void);
void Cl_ShutdownHttpDownload(void);

//cl_loc.c
void Cl_InitLocations(void);
void Cl_ShutdownLocations(void);
void Cl_LoadLocations(void);
const char *Cl_LocationHere(void);
const char *Cl_LocationThere(void);

// cl_server.c
void Cl_Ping_f(void);
void Cl_Servers_f(void);
void Cl_ParseStatusMessage(void);
void Cl_ParseServersList(void);
void Cl_FreeServers(void);
server_info_t *Cl_ServerForNum(int num);

#endif /* __CLIENT_H__ */
