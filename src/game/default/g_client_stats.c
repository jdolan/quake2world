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

#include "g_local.h"

/*
 * @brief
 */
void G_ClientToIntermission(g_edict_t *ent) {

	VectorCopy(g_level.intermission_origin, ent->s.origin);

	PackVector(g_level.intermission_origin, ent->client->ps.pm_state.origin);

	VectorClear(ent->client->ps.pm_state.view_angles);
	PackAngles(g_level.intermission_angle, ent->client->ps.pm_state.delta_angles);

	VectorClear(ent->client->ps.pm_state.view_offset);

	ent->client->ps.pm_state.type = PM_FREEZE;

	ent->s.model1 = 0;
	ent->s.model2 = 0;
	ent->s.model3 = 0;
	ent->s.model4 = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;
	ent->locals.dead = true;

	// show scores
	ent->client->locals.show_scores = true;

	// hide the hud
	ent->client->locals.persistent.weapon = NULL;
	ent->client->locals.ammo_index = 0;
	ent->client->locals.persistent.armor = 0;
	ent->client->locals.pickup_msg_time = 0;
}

/*
 * @brief Write the scores information for the specified client.
 */
static void G_UpdateScore(const g_edict_t *ent, g_score_t *s) {

	memset(s, 0, sizeof(*s));

	s->client = ent - g_game.edicts - 1;
	s->ping = ent->client->ping < 999 ? ent->client->ping : 999;

	if (ent->client->locals.persistent.spectator) {
		s->color = 0;
		s->flags |= SCORES_SPECTATOR;
	} else {
		if (g_level.match) {
			if (!ent->client->locals.persistent.ready)
				s->flags |= SCORES_NOT_READY;
		}
		if (g_level.ctf) {
			if (ent->s.effects & (EF_CTF_BLUE | EF_CTF_RED))
				s->flags |= SCORES_CTF_FLAG;
		}
		if (ent->client->locals.persistent.team) {
			if (ent->client->locals.persistent.team == &g_team_good) {
				s->color = TEAM_COLOR_GOOD;
				s->flags |= SCORES_TEAM_GOOD;
			} else {
				s->color = TEAM_COLOR_EVIL;
				s->flags |= SCORES_TEAM_EVIL;
			}
		} else {
			s->color = ent->client->locals.persistent.color;
		}
	}

	s->score = ent->client->locals.persistent.score;
	s->captures = ent->client->locals.persistent.captures;
}

/*
 * @brief Returns the number of scores written to the buffer.
 */
static size_t G_UpdateScores(g_score_t *scores) {
	g_score_t *s = scores;
	int32_t i;

	// assemble the client scores
	for (i = 0; i < sv_max_clients->integer; i++) {
		const g_edict_t *e = &g_game.edicts[i + 1];

		if (!e->in_use)
			continue;

		G_UpdateScore(e, s++);
	}

	// and optionally concatenate the team scores
	if (g_level.teams || g_level.ctf) {
		memset(s, 0, sizeof(s) * 2);

		s->client = MAX_CLIENTS;
		s->score = g_team_good.score;
		s->captures = g_team_good.captures;
		s->flags = SCORES_TEAM_GOOD;
		s++;

		s->client = MAX_CLIENTS;
		s->score = g_team_evil.score;
		s->captures = g_team_evil.captures;
		s->flags = SCORES_TEAM_EVIL;
		s++;
	}

	return (size_t) (s - scores);
}

/*
 * @brief Assemble the binary scores data for the client. Scores are sent in
 * chunks to overcome the 1400 byte UDP packet limitation.
 */
void G_ClientScores(g_edict_t *ent) {
	g_score_t scores[MAX_CLIENTS + 2];

	if (!ent->client->locals.show_scores || (ent->client->locals.scores_time > g_level.time))
		return;

	ent->client->locals.scores_time = g_level.time + 500;

	size_t i = 0, j = 0, count = G_UpdateScores(scores);

	while (i < count) {
		const size_t bytes = (i - j) * sizeof(g_score_t);
		if (bytes > 512) {
			gi.WriteByte(SV_CMD_SCORES);
			gi.WriteShort(j);
			gi.WriteShort(i);
			gi.WriteData((const void *) (scores + j), bytes);
			gi.Unicast(ent, false);

			j = i;
		}
		i++;
	}

	const size_t bytes = (i - j) * sizeof(g_score_t);
	if (bytes) {
		gi.WriteByte(SV_CMD_SCORES);
		gi.WriteShort(j);
		gi.WriteShort(i);
		gi.WriteData((const void *) (scores + j), bytes);
		gi.Unicast(ent, false);
	}
}

/*
 * @brief Writes the stats array of the player state structure. The client's HUD is
 * largely derived from this information.
 */
void G_ClientStats(g_edict_t *ent) {

	g_client_t *client = ent->client;
	const g_client_persistent_t *persistent = &client->locals.persistent;

	// ammo
	if (!client->locals.ammo_index) {
		client->ps.stats[STAT_AMMO_ICON] = 0;
		client->ps.stats[STAT_AMMO] = 0;
	} else {
		const g_item_t *item = &g_items[client->locals.ammo_index];
		client->ps.stats[STAT_AMMO_ICON] = gi.ImageIndex(item->icon);
		client->ps.stats[STAT_AMMO] = persistent->inventory[client->locals.ammo_index];
		client->ps.stats[STAT_AMMO_LOW] = item->quantity;
	}

	// armor
	if (persistent->armor >= 200)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("pics/i_bodyarmor");
	else if (persistent->armor >= 100)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("pics/i_combatarmor");
	else if (persistent->armor >= 50)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("pics/i_jacketarmor");
	else if (persistent->armor > 0)
		client->ps.stats[STAT_ARMOR_ICON] = gi.ImageIndex("pics/i_shard");
	else
		client->ps.stats[STAT_ARMOR_ICON] = 0;
	client->ps.stats[STAT_ARMOR] = persistent->armor;

	// captures
	client->ps.stats[STAT_CAPTURES] = persistent->captures;

	// damage received and inflicted
	client->ps.stats[STAT_DAMAGE_ARMOR] = client->locals.damage_armor;
	client->ps.stats[STAT_DAMAGE_HEALTH] = client->locals.damage_health;
	client->ps.stats[STAT_DAMAGE_INFLICT] = client->locals.damage_inflicted;

	// frags
	client->ps.stats[STAT_FRAGS] = persistent->score;

	// health
	if (persistent->spectator || ent->locals.dead) {
		client->ps.stats[STAT_HEALTH_ICON] = 0;
		client->ps.stats[STAT_HEALTH] = 0;
	} else {
		client->ps.stats[STAT_HEALTH_ICON] = gi.ImageIndex("pics/i_health");
		client->ps.stats[STAT_HEALTH] = ent->locals.health;
	}

	// pickup message
	if (g_level.time > client->locals.pickup_msg_time) {
		client->ps.stats[STAT_PICKUP_ICON] = 0;
		client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	// ready
	client->ps.stats[STAT_READY] = 0;
	if (g_level.match && g_level.match_time)
		client->ps.stats[STAT_READY] = persistent->ready;

	// rounds
	if (g_level.rounds)
		client->ps.stats[STAT_ROUND] = g_level.round_num + 1;

	// scores
	client->ps.stats[STAT_SCORES] = 0;
	if (g_level.intermission_time || client->locals.show_scores)
		client->ps.stats[STAT_SCORES] |= 1;

	if ((g_level.teams || g_level.ctf) && persistent->team) { // send team_name
		if (persistent->team == &g_team_good)
			client->ps.stats[STAT_TEAM] = CS_TEAM_GOOD;
		else
			client->ps.stats[STAT_TEAM] = CS_TEAM_EVIL;
	} else
		client->ps.stats[STAT_TEAM] = 0;

	// time
	if (g_level.intermission_time)
		client->ps.stats[STAT_TIME] = 0;
	else
		client->ps.stats[STAT_TIME] = CS_TIME;

	// vote
	if (g_level.vote_time)
		client->ps.stats[STAT_VOTE] = CS_VOTE;
	else
		client->ps.stats[STAT_VOTE] = 0;

	// weapon
	if (persistent->weapon) {
		client->ps.stats[STAT_WEAPON_ICON] = gi.ImageIndex(persistent->weapon->icon);
		client->ps.stats[STAT_WEAPON] = gi.ModelIndex(persistent->weapon->model);
	} else {
		client->ps.stats[STAT_WEAPON_ICON] = 0;
		client->ps.stats[STAT_WEAPON] = 0;
	}

}

/*
 * @brief
 */
void G_ClientSpectatorStats(g_edict_t *ent) {
	g_client_t *client = ent->client;

	client->ps.stats[STAT_SPECTATOR] = 1;

	// chase camera inherits stats from their chase target
	if (client->locals.chase_target && client->locals.chase_target->in_use) {
		client->ps.stats[STAT_CHASE] = CS_CLIENTS + (client->locals.chase_target - g_game.edicts)
				- 1;

		// scores are independent of chase camera target
		if (g_level.intermission_time || client->locals.show_scores)
			client->ps.stats[STAT_SCORES] = 1;
		else {
			client->ps.stats[STAT_SCORES] = 0;
		}
	} else {
		G_ClientStats(ent);
		client->ps.stats[STAT_CHASE] = 0;
	}
}

