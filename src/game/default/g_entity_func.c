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

static void G_func_areaportal_use(edict_t *ent, edict_t *other, edict_t *activator){
	ent->count ^= 1;  // toggle state
	gi.SetAreaPortalState(ent->style, ent->count);
}

/*QUAKED func_areaportal(0 0 0) ?

This is a non-visible object that divides the world into
areas that are seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
void G_func_areaportal(edict_t *ent){
	ent->use = G_func_areaportal_use;
	ent->count = 0;  // always start closed;
}


/*
 * PLATS
 *
 * movement options:
 *
 * linear
 * smooth start, hard stop
 * smooth start, smooth stop
 *
 * start
 * end
 * acceleration
 * speed
 * deceleration
 * begin sound
 * end sound
 * target fired when reaching end
 * wait at end
 *
 * object characteristics that use move segments
 * ---------------------------------------------
 * move_type_push, or move_type_stop
 * action when touched
 * action when blocked
 * action when used
 * 	disabled?
 * auto trigger spawning
 */

#define PLAT_LOW_TRIGGER	1

#define STATE_TOP			0
#define STATE_BOTTOM		1
#define STATE_UP			2
#define STATE_DOWN			3

static void G_MoveInfoDone(edict_t *ent){
	VectorClear(ent->velocity);
	ent->move_info.done(ent);
}

static void G_MoveInfo_End(edict_t *ent){

	if(ent->move_info.remaining_distance == 0){
		G_MoveInfoDone(ent);
		return;
	}

	VectorScale(ent->move_info.dir, ent->move_info.remaining_distance / gi.server_frame, ent->velocity);

	ent->think = G_MoveInfoDone;
	ent->next_think = g_level.time + gi.server_frame;
}

/*
 * G_MoveInfo_Constant
 *
 * Starts a move with constant velocity. The entity will think again when it
 * has reached its destination.
 */
static void G_MoveInfo_Constant(edict_t *ent){
	float frames;

	if((ent->move_info.speed * gi.server_frame) >= ent->move_info.remaining_distance){
		G_MoveInfo_End(ent);
		return;
	}

	VectorScale(ent->move_info.dir, ent->move_info.speed, ent->velocity);
	frames = floor((ent->move_info.remaining_distance / ent->move_info.speed) / gi.server_frame);
	ent->move_info.remaining_distance -= frames * ent->move_info.speed * gi.server_frame;
	ent->next_think = g_level.time + (frames * gi.server_frame);
	ent->think = G_MoveInfo_End;
}


#define AccelerationDistance(target, rate) (target * ((target / rate) + 1) / 2)

/*
 * G_MoveInfo_UpdateAcceleration
 *
 * Updates the acceleration parameters for the specified move. This determines
 * whether we should accelerate or decelerate based on the distance remaining.
 */
static void G_MoveInfo_UpdateAcceleration(g_move_info_t *move_info){
	float accel_dist;
	float decel_dist;

	move_info->move_speed = move_info->speed;

	if(move_info->remaining_distance < move_info->accel){
		move_info->current_speed = move_info->remaining_distance;
		return;
	}

	accel_dist = AccelerationDistance(move_info->speed, move_info->accel);
	decel_dist = AccelerationDistance(move_info->speed, move_info->decel);

	if((move_info->remaining_distance - accel_dist - decel_dist) < 0){
		float f;

		f = (move_info->accel + move_info->decel) / (move_info->accel * move_info->decel);
		move_info->move_speed = (-2 + sqrt(4 - 4 * f * (-2 * move_info->remaining_distance))) / (2 * f);
		decel_dist = AccelerationDistance(move_info->move_speed, move_info->decel);
	}

	move_info->decel_distance = decel_dist;
}


/*
 * G_MoveInfo_Accelerate
 *
 * Applies any acceleration / deceleration based on the distance remaining.
 */
static void G_MoveInfo_Accelerate(g_move_info_t *move_info){
	// are we decelerating?
	if(move_info->remaining_distance <= move_info->decel_distance){
		if(move_info->remaining_distance < move_info->decel_distance){
			if(move_info->next_speed){
				move_info->current_speed = move_info->next_speed;
				move_info->next_speed = 0;
				return;
			}
			if(move_info->current_speed > move_info->decel)
				move_info->current_speed -= move_info->decel;
		}
		return;
	}

	// are we at full speed and need to start decelerating during this move?
	if(move_info->current_speed == move_info->move_speed)
		if((move_info->remaining_distance - move_info->current_speed) < move_info->decel_distance){
			float p1_distance;
			float p2_distance;
			float distance;

			p1_distance = move_info->remaining_distance - move_info->decel_distance;
			p2_distance = move_info->move_speed * (1.0 - (p1_distance / move_info->move_speed));
			distance = p1_distance + p2_distance;
			move_info->current_speed = move_info->move_speed;
			move_info->next_speed = move_info->move_speed - move_info->decel * (p2_distance / distance);
			return;
		}

	// are we accelerating?
	if(move_info->current_speed < move_info->speed){
		float old_speed;
		float p1_distance;
		float p1_speed;
		float p2_distance;
		float distance;

		old_speed = move_info->current_speed;

		// figure simple acceleration up to move_speed
		move_info->current_speed += move_info->accel;
		if(move_info->current_speed > move_info->speed)
			move_info->current_speed = move_info->speed;

		// are we accelerating throughout this entire move?
		if((move_info->remaining_distance - move_info->current_speed) >= move_info->decel_distance)
			return;

		// during this move we will accelrate from current_speed to move_speed
		// and cross over the decel_distance; figure the average speed for the
		// entire move
		p1_distance = move_info->remaining_distance - move_info->decel_distance;
		p1_speed = (old_speed + move_info->move_speed) / 2.0;
		p2_distance = move_info->move_speed * (1.0 - (p1_distance / p1_speed));
		distance = p1_distance + p2_distance;
		move_info->current_speed = (p1_speed * (p1_distance / distance)) + (move_info->move_speed * (p2_distance / distance));
		move_info->next_speed = move_info->move_speed - move_info->decel * (p2_distance / distance);
		return;
	}
}


/*
 * G_MoveInfo_Accelerative
 *
 * Sets up a non-constant move, i.e. one that will accelerate near the beginning
 * and decelerate towards the end.
 */
static void G_MoveInfo_Accelerative(edict_t *ent){

	ent->move_info.remaining_distance -= ent->move_info.current_speed;

	if(ent->move_info.current_speed == 0)  // starting or blocked
		G_MoveInfo_UpdateAcceleration(&ent->move_info);

	G_MoveInfo_Accelerate(&ent->move_info);

	// will the entire move complete on next frame?
	if(ent->move_info.remaining_distance <= ent->move_info.current_speed){
		G_MoveInfo_End(ent);
		return;
	}

	VectorScale(ent->move_info.dir, ent->move_info.current_speed * gi.frame_rate, ent->velocity);
	ent->next_think = g_level.time + gi.server_frame;
	ent->think = G_MoveInfo_Accelerative;
}


/*
 * G_MoveInfo_Init
 *
 * Sets up movement for the specified entity. Both constant and accelerative
 * movements are initiated through this function.
 */
static void G_MoveInfo_Init(edict_t *ent, vec3_t dest, void(*done)(edict_t*)){

	VectorClear(ent->velocity);

	VectorSubtract(dest, ent->s.origin, ent->move_info.dir);
	ent->move_info.remaining_distance = VectorNormalize(ent->move_info.dir);

	ent->move_info.done = done;

	if(ent->move_info.speed == ent->move_info.accel &&
			ent->move_info.speed == ent->move_info.decel){  // constant
		if(g_level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->team_master : ent)){
			G_MoveInfo_Constant(ent);
		} else {
			ent->next_think = g_level.time + gi.server_frame;
			ent->think = G_MoveInfo_Constant;
		}
	} else {  // accelerative
		ent->move_info.current_speed = 0;
		ent->think = G_MoveInfo_Accelerative;
		ent->next_think = g_level.time + gi.server_frame;
	}
}


/*
 * G_func_plat
 */
static void G_func_plat_go_down(edict_t *ent);

static void G_func_plat_up(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_end)
			gi.Sound(ent, ent->move_info.sound_end, ATTN_IDLE);
		ent->s.sound = 0;
	}
	ent->move_info.state = STATE_TOP;

	ent->think = G_func_plat_go_down;
	ent->next_think = g_level.time + 3;
}

static void G_func_plat_down(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_end)
			gi.Sound(ent, ent->move_info.sound_end, ATTN_IDLE);
		ent->s.sound = 0;
	}
	ent->move_info.state = STATE_BOTTOM;
}

static void G_func_plat_go_down(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_start)
			gi.Sound(ent, ent->move_info.sound_start, ATTN_IDLE);
		ent->s.sound = ent->move_info.sound_middle;
	}
	ent->move_info.state = STATE_DOWN;
	G_MoveInfo_Init(ent, ent->move_info.end_origin, G_func_plat_down);
}

static void G_func_plat_go_up(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_start)
			gi.Sound(ent, ent->move_info.sound_start, ATTN_IDLE);
		ent->s.sound = ent->move_info.sound_middle;
	}
	ent->move_info.state = STATE_UP;
	G_MoveInfo_Init(ent, ent->move_info.start_origin, G_func_plat_up);
}

static void G_func_plat_blocked(edict_t *self, edict_t *other){
	if(!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);

	if(self->move_info.state == STATE_UP)
		G_func_plat_go_down(self);
	else if(self->move_info.state == STATE_DOWN)
		G_func_plat_go_up(self);
}


static void G_func_plat_use(edict_t *ent, edict_t *other, edict_t *activator){
	if(ent->think)
		return;  // already down
	G_func_plat_go_down(ent);
}


static void G_func_plat_touch(edict_t *ent, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	if(!other->client)
		return;

	if(other->health <= 0)
		return;

	ent = ent->enemy;  // now point at the plat, not the trigger
	if(ent->move_info.state == STATE_BOTTOM)
		G_func_plat_go_up(ent);
	else if(ent->move_info.state == STATE_TOP)
		ent->next_think = g_level.time + 1;  // the player is still on the plat, so delay going down
}

static void G_func_plat_spawn_inside_trigger(edict_t *ent){
	edict_t *trigger;
	vec3_t tmin, tmax;

	// middle trigger
	trigger = G_Spawn();
	trigger->touch = G_func_plat_touch;
	trigger->move_type = MOVE_TYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	trigger->enemy = ent;

	tmin[0] = ent->mins[0] + 25;
	tmin[1] = ent->mins[1] + 25;
	tmin[2] = ent->mins[2];

	tmax[0] = ent->maxs[0] - 25;
	tmax[1] = ent->maxs[1] - 25;
	tmax[2] = ent->maxs[2] + 8;

	tmin[2] = tmax[2] - (ent->pos1[2] - ent->pos2[2] + g_game.spawn.lip);

	if(ent->spawn_flags & PLAT_LOW_TRIGGER)
		tmax[2] = tmin[2] + 8;

	if(tmax[0] - tmin[0] <= 0){
		tmin[0] = (ent->mins[0] + ent->maxs[0]) * 0.5;
		tmax[0] = tmin[0] + 1;
	}
	if(tmax[1] - tmin[1] <= 0){
		tmin[1] = (ent->mins[1] + ent->maxs[1]) * 0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy(tmin, trigger->mins);
	VectorCopy(tmax, trigger->maxs);

	gi.LinkEntity(trigger);
}


/*QUAKED func_plat(0 .5 .8) ? PLAT_LOW_TRIGGER

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out
disabled in the extended position until it is trigger, when it will lower
and become a normal plat.

"speed"	overrides default 200.
"accel" overrides default 500
"lip"	overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves,
instead of being implicitly determined by the model's height.
*/
void G_func_plat(edict_t *ent){
	float f;

	VectorClear(ent->s.angles);
	ent->solid = SOLID_BSP;
	ent->move_type = MOVE_TYPE_PUSH;

	gi.SetModel(ent, ent->model);

	ent->blocked = G_func_plat_blocked;

	if(!ent->speed)
		ent->speed = 200;
	ent->speed *= 0.5;

	if(!ent->accel)
		ent->accel = 50;
	ent->accel *= 0.1;

	if(!ent->decel)
		ent->decel = 50;
	ent->decel *= 0.1;

	// normalize move_info based on server rate, stock q2 rate was 10hz
	f = 100.0 / (gi.frame_rate * gi.frame_rate);
	ent->speed *= f;
	ent->accel *= f;
	ent->decel *= f;

	if(!ent->dmg)
		ent->dmg = 2;

	if(!g_game.spawn.lip)
		g_game.spawn.lip = 8;

	// pos1 is the top position, pos2 is the bottom
	VectorCopy(ent->s.origin, ent->pos1);
	VectorCopy(ent->s.origin, ent->pos2);

	if(g_game.spawn.height)  // use the specified height
		ent->pos2[2] -= g_game.spawn.height;
	else  // or derive it from the model height
		ent->pos2[2] -= (ent->maxs[2] - ent->mins[2]) - g_game.spawn.lip;

	ent->use = G_func_plat_use;

	G_func_plat_spawn_inside_trigger(ent);  // the "start moving" trigger

	if(ent->target_name){
		ent->move_info.state = STATE_UP;
	} else {
		VectorCopy(ent->pos2, ent->s.origin);
		gi.LinkEntity(ent);
		ent->move_info.state = STATE_BOTTOM;
	}

	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->pos1, ent->move_info.start_origin);
	VectorCopy(ent->s.angles, ent->move_info.start_angles);
	VectorCopy(ent->pos2, ent->move_info.end_origin);
	VectorCopy(ent->s.angles, ent->move_info.end_angles);

	ent->move_info.sound_start = gi.SoundIndex("world/plat_start");
	ent->move_info.sound_middle = gi.SoundIndex("world/plat_mid");
	ent->move_info.sound_end = gi.SoundIndex("world/plat_end");
}


/*QUAKED func_rotating(0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP
You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"speed" determines how fast it moves; default value is 100.
"dmg"	damage to inflict when blocked(2 default)

REVERSE will cause the it to rotate in the opposite direction.
STOP mean it will stop moving instead of pushing entities
*/

static void G_func_rotating_blocked(edict_t *self, edict_t *other){
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void G_func_rotating_touch(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	if(self->avelocity[0] || self->avelocity[1] || self->avelocity[2])
		G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void G_func_rotating_use(edict_t *self, edict_t *other, edict_t *activator){
	if(!VectorCompare(self->avelocity, vec3_origin)){
		self->s.sound = 0;
		VectorClear(self->avelocity);
		self->touch = NULL;
	} else {
		self->s.sound = self->move_info.sound_middle;
		VectorScale(self->movedir, self->speed, self->avelocity);
		if(self->spawn_flags & 16)
			self->touch = G_func_rotating_touch;
	}
}

void G_func_rotating(edict_t *ent){
	ent->solid = SOLID_BSP;
	if(ent->spawn_flags & 32)
		ent->move_type = MOVE_TYPE_STOP;
	else
		ent->move_type = MOVE_TYPE_PUSH;

	// set the axis of rotation
	VectorClear(ent->movedir);
	if(ent->spawn_flags & 4)
		ent->movedir[2] = 1.0;
	else if(ent->spawn_flags & 8)
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if(ent->spawn_flags & 2)
		VectorNegate(ent->movedir, ent->movedir);

	if(!ent->speed)
		ent->speed = 100;

	if(!ent->dmg)
		ent->dmg = 2;

	ent->use = G_func_rotating_use;
	if(ent->dmg)
		ent->blocked = G_func_rotating_blocked;

	if(ent->spawn_flags & 1)
		ent->use(ent, NULL, NULL);

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}

/*
 *
 * BUTTONS
 *
 */

/*QUAKED func_button(0 .5 .8) ?
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"sounds"
1) silent
2) steam metal
3) wooden clunk
4) metallic click
5) in-out
*/

static void G_func_button_done(edict_t *self){
	self->move_info.state = STATE_BOTTOM;
}

static void G_func_button_reset(edict_t *self){
	self->move_info.state = STATE_DOWN;

	G_MoveInfo_Init(self, self->move_info.start_origin, G_func_button_done);

	if(self->health)
		self->take_damage = true;
}

static void G_func_button_wait(edict_t *self){
	self->move_info.state = STATE_TOP;

	G_UseTargets(self, self->activator);

	if(self->move_info.wait >= 0){
		  self->next_think = g_level.time + self->move_info.wait;
		  self->think = G_func_button_reset;
	}
}

static void G_func_button_activate(edict_t *self){
	if(self->move_info.state == STATE_UP || self->move_info.state == STATE_TOP)
		 return;

	self->move_info.state = STATE_UP;

	if(self->move_info.sound_start && !(self->flags & FL_TEAMSLAVE))
		  gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);

	G_MoveInfo_Init(self, self->move_info.end_origin, G_func_button_wait);
}

static void G_func_button_use(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;
	G_func_button_activate(self);
}

static void G_func_button_touch(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	if(!other->client)
		  return;

	if(other->health <= 0)
		  return;

	self->activator = other;
	G_func_button_activate(self);
}

static void G_func_button_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	self->activator = attacker;
	self->health = self->max_health;
	self->take_damage = false;
	G_func_button_activate(self);
}

void G_func_button(edict_t *ent){
	vec3_t abs_movedir;
	float dist;

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->move_type = MOVE_TYPE_STOP;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	if(ent->sounds != 1)
		  ent->move_info.sound_start = gi.SoundIndex("world/switch");

	if(!ent->speed)
		  ent->speed = 40;
	if(!ent->accel)
		  ent->accel = ent->speed;
	if(!ent->decel)
		  ent->decel = ent->speed;

	if(!ent->wait)
		  ent->wait = 3;
	if(!g_game.spawn.lip)
		  g_game.spawn.lip = 4;

	VectorCopy(ent->s.origin, ent->pos1);
	abs_movedir[0] = fabsf(ent->movedir[0]);
	abs_movedir[1] = fabsf(ent->movedir[1]);
	abs_movedir[2] = fabsf(ent->movedir[2]);
	dist = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - g_game.spawn.lip;
	VectorMA(ent->pos1, dist, ent->movedir, ent->pos2);

	ent->use = G_func_button_use;

	if(ent->health){
		  ent->max_health = ent->health;
		  ent->die = G_func_button_die;
		  ent->take_damage = true;
	} else if(! ent->target_name)
		  ent->touch = G_func_button_touch;

	ent->move_info.state = STATE_BOTTOM;

	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->pos1, ent->move_info.start_origin);
	VectorCopy(ent->s.angles, ent->move_info.start_angles);
	VectorCopy(ent->pos2, ent->move_info.end_origin);
	VectorCopy(ent->s.angles, ent->move_info.end_angles);

	gi.LinkEntity(ent);
}


#define DOOR_START_OPEN		1
#define DOOR_TOGGLE			2

/*QUAKED func_door(0 .5 .8) ? START_OPEN TOGGLE
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered(not useful for touch or takedamage doors).
TOGGLE		wait in both the start and end states for a trigger event.

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed(100 default)
"wait"		wait before returning(3 default, -1 = never return)
"lip"		lip remaining at end of move(8 default)
"dmg"		damage to inflict when blocked(2 default)
*/
static void G_func_door_use_areaportals(edict_t *self, qboolean open){
	edict_t *t = NULL;

	if(!self->target)
		  return;

	while((t = G_Find(t, FOFS(target_name), self->target))){
		if(strcasecmp(t->class_name, "func_areaportal") == 0){
		  gi.SetAreaPortalState(t->style, open);
		}
	}
}

static void G_func_door_go_down(edict_t *self);

static void G_func_door_up(edict_t *self){
	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_end)
			gi.Sound(self, self->move_info.sound_end, ATTN_IDLE);
		self->s.sound = 0;
	}
	self->move_info.state = STATE_TOP;

	if(self->spawn_flags & DOOR_TOGGLE)
		return;

	if(self->move_info.wait >= 0){
		self->think = G_func_door_go_down;
		self->next_think = g_level.time + self->move_info.wait;
	}
}

static void G_func_door_down(edict_t *self){
	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_end)
			gi.Sound(self, self->move_info.sound_end, ATTN_IDLE);
		self->s.sound = 0;
	}
	self->move_info.state = STATE_BOTTOM;
	G_func_door_use_areaportals(self, false);
}

static void G_func_door_go_down(edict_t *self){
	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_start)
			gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->move_info.sound_middle;
	}
	if(self->max_health){
		self->take_damage = true;
		self->health = self->max_health;
	}

	self->move_info.state = STATE_DOWN;
	G_MoveInfo_Init(self, self->move_info.start_origin, G_func_door_down);
}

static void G_func_door_go_up(edict_t *self, edict_t *activator){
	if(self->move_info.state == STATE_UP)
		return;  // already going up

	if(self->move_info.state == STATE_TOP){  // reset top wait time
		if(self->move_info.wait >= 0)
			self->next_think = g_level.time + self->move_info.wait;
		return;
	}

	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_start)
			gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->move_info.sound_middle;
	}
	self->move_info.state = STATE_UP;
	G_MoveInfo_Init(self, self->move_info.end_origin, G_func_door_up);

	G_UseTargets(self, activator);
	G_func_door_use_areaportals(self, true);
}

static void G_func_door_use(edict_t *self, edict_t *other, edict_t *activator){
	edict_t *ent;

	if(self->flags & FL_TEAMSLAVE)
		return;

	if(self->spawn_flags & DOOR_TOGGLE){
		if(self->move_info.state == STATE_UP || self->move_info.state == STATE_TOP){
			// trigger all paired doors
			for(ent = self; ent; ent = ent->team_chain){
				ent->message = NULL;
				ent->touch = NULL;
				G_func_door_go_down(ent);
			}
			return;
		}
	}

	// trigger all paired doors
	for(ent = self; ent; ent = ent->team_chain){
		ent->message = NULL;
		ent->touch = NULL;
		G_func_door_go_up(ent, activator);
	}
}

static void G_func_door_touch_trigger(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	if(other->health <= 0)
		return;

	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	self->touch_time = g_level.time + 1.0;

	G_func_door_use(self->owner, other, other);
}

static void G_func_door_calc_move(edict_t *self){
	edict_t *ent;
	float min;
	float time;
	float newspeed;
	float ratio;
	float dist;

	if(self->flags & FL_TEAMSLAVE)
		return;  // only the team master does this

	// find the smallest distance any member of the team will be moving
	min = fabsf(self->move_info.distance);
	for(ent = self->team_chain; ent; ent = ent->team_chain){
		dist = fabsf(ent->move_info.distance);
		if(dist < min)
			min = dist;
	}

	time = min / self->move_info.speed;

	// adjust speeds so they will all complete at the same time
	for(ent = self; ent; ent = ent->team_chain){
		newspeed = fabsf(ent->move_info.distance) / time;
		ratio = newspeed / ent->move_info.speed;
		if(ent->move_info.accel == ent->move_info.speed)
			ent->move_info.accel = newspeed;
		else
			ent->move_info.accel *= ratio;
		if(ent->move_info.decel == ent->move_info.speed)
			ent->move_info.decel = newspeed;
		else
			ent->move_info.decel *= ratio;
		ent->move_info.speed = newspeed;
	}
}

static void G_func_door_create_trigger(edict_t *ent){
	edict_t *other;
	vec3_t mins, maxs;

	if(ent->flags & FL_TEAMSLAVE)
		return;  // only the team leader spawns a trigger

	VectorCopy(ent->abs_mins, mins);
	VectorCopy(ent->abs_maxs, maxs);

	for(other = ent->team_chain; other; other = other->team_chain){
		AddPointToBounds(other->abs_mins, mins, maxs);
		AddPointToBounds(other->abs_maxs, mins, maxs);
	}

	// expand
	mins[0] -= 60;
	mins[1] -= 60;
	maxs[0] += 60;
	maxs[1] += 60;

	other = G_Spawn();
	VectorCopy(mins, other->mins);
	VectorCopy(maxs, other->maxs);
	other->owner = ent;
	other->solid = SOLID_TRIGGER;
	other->move_type = MOVE_TYPE_NONE;
	other->touch = G_func_door_touch_trigger;
	gi.LinkEntity(other);

	if(ent->spawn_flags & DOOR_START_OPEN)
		G_func_door_use_areaportals(ent, true);

	G_func_door_calc_move(ent);
}

static void G_func_door_blocked(edict_t *self, edict_t *other){
	edict_t *ent;

	if(!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if(self->move_info.wait >= 0){
		if(self->move_info.state == STATE_DOWN){
			for(ent = self->team_master; ent; ent = ent->team_chain)
				G_func_door_go_up(ent, ent->activator);
		} else {
			for(ent = self->team_master; ent; ent = ent->team_chain)
				G_func_door_go_down(ent);
		}
	}
}

static void G_func_door_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	edict_t *ent;

	for(ent = self->team_master; ent; ent = ent->team_chain){
		ent->health = ent->max_health;
		ent->take_damage = false;
	}

	G_func_door_use(self->team_master, attacker, attacker);
}

static void G_func_door_touch(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	self->touch_time = g_level.time + 5.0;

	if(self->message && strlen(self->message))
		gi.ClientCenterPrint(other, "%s", self->message);

	gi.Sound(other, gi.SoundIndex("misc/chat"), ATTN_NORM);
}

void G_func_door(edict_t *ent){
	vec3_t abs_movedir;

	if(ent->sounds != 1){
		ent->move_info.sound_start = gi.SoundIndex("world/door_start");
		ent->move_info.sound_end = gi.SoundIndex("world/door_end");
	}

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->blocked = G_func_door_blocked;
	ent->use = G_func_door_use;

	if(!ent->speed)
		ent->speed = 100;
	ent->speed *= 2;

	if(!ent->accel)
		ent->accel = ent->speed;
	if(!ent->decel)
		ent->decel = ent->speed;

	if(!ent->wait)
		ent->wait = 3;
	if(!g_game.spawn.lip)
		g_game.spawn.lip = 8;
	if(!ent->dmg)
		ent->dmg = 2;

	// calculate second position
	VectorCopy(ent->s.origin, ent->pos1);
	abs_movedir[0] = fabsf(ent->movedir[0]);
	abs_movedir[1] = fabsf(ent->movedir[1]);
	abs_movedir[2] = fabsf(ent->movedir[2]);
	ent->move_info.distance = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - g_game.spawn.lip;
	VectorMA(ent->pos1, ent->move_info.distance, ent->movedir, ent->pos2);

	// if it starts open, switch the positions
	if(ent->spawn_flags & DOOR_START_OPEN){
		VectorCopy(ent->pos2, ent->s.origin);
		VectorCopy(ent->pos1, ent->pos2);
		VectorCopy(ent->s.origin, ent->pos1);
	}

	ent->move_info.state = STATE_BOTTOM;

	if(ent->health){
		ent->take_damage = true;
		ent->die = G_func_door_die;
		ent->max_health = ent->health;
	} else if(ent->target_name && ent->message){
		gi.SoundIndex("misc/chat");
		ent->touch = G_func_door_touch;
	}

	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->pos1, ent->move_info.start_origin);
	VectorCopy(ent->s.angles, ent->move_info.start_angles);
	VectorCopy(ent->pos2, ent->move_info.end_origin);
	VectorCopy(ent->s.angles, ent->move_info.end_angles);

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if(!ent->team)
		ent->team_master = ent;

	gi.LinkEntity(ent);

	ent->next_think = g_level.time + gi.server_frame;
	if(ent->health || ent->target_name)
		ent->think = G_func_door_calc_move;
	else
		ent->think = G_func_door_create_trigger;
}


/*QUAKED func_wall(0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON
This is just a solid wall if not inhibited

TRIGGER_SPAWN	the wall will not be present until triggered
				it will then blink in to existence; it will
				kill anything that was in it's way

TOGGLE			only valid for TRIGGER_SPAWN walls
				this allows the wall to be turned on and off

START_ON		only valid for TRIGGER_SPAWN walls
				the wall will initially be present
*/

static void G_func_wall_use(edict_t *self, edict_t *other, edict_t *activator){
	if(self->solid == SOLID_NOT){
		self->solid = SOLID_BSP;
		self->sv_flags &= ~SVF_NOCLIENT;
		G_KillBox(self);
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NOCLIENT;
	}
	gi.LinkEntity(self);

	if(!(self->spawn_flags & 2))
		self->use = NULL;
}

void G_func_wall(edict_t *self){
	self->move_type = MOVE_TYPE_PUSH;
	gi.SetModel(self, self->model);

	// just a wall
	if((self->spawn_flags & 7) == 0){
		self->solid = SOLID_BSP;
		gi.LinkEntity(self);
		return;
	}

	// it must be TRIGGER_SPAWN
	if(!(self->spawn_flags & 1)){
		gi.Debug("func_wall missing TRIGGER_SPAWN\n");
		self->spawn_flags |= 1;
	}

	// yell if the spawnflags are odd
	if(self->spawn_flags & 4){
		if(!(self->spawn_flags & 2)){
			gi.Debug("func_wall START_ON without TOGGLE\n");
			self->spawn_flags |= 2;
		}
	}

	self->use = G_func_wall_use;

	if(self->spawn_flags & 4){
		self->solid = SOLID_BSP;
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NOCLIENT;
	}
	gi.LinkEntity(self);
}


#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

/*QUAKED func_train(0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
noise	looping sound to play when the train is in motion

*/
static void G_func_train_next(edict_t *self);

static void G_func_train_blocked(edict_t *self, edict_t *other){
	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	if(!self->dmg)
		return;

	self->touch_time = g_level.time + 0.5;
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void G_func_train_wait(edict_t *self){
	if(self->target_ent->path_target){
		char *savetarget;
		edict_t *ent;

		ent = self->target_ent;
		savetarget = ent->target;
		ent->target = ent->path_target;
		G_UseTargets(ent, self->activator);
		ent->target = savetarget;

		// make sure we didn't get killed by a killtarget
		if(!self->in_use)
			return;
	}

	if(self->move_info.wait){
		if(self->move_info.wait > 0){
			self->next_think = g_level.time + self->move_info.wait;
			self->think = G_func_train_next;
		} else if(self->spawn_flags & TRAIN_TOGGLE)  // && wait < 0
		{
			G_func_train_next(self);
			self->spawn_flags &= ~TRAIN_START_ON;
			VectorClear(self->velocity);
			self->next_think = 0;
		}

		if(!(self->flags & FL_TEAMSLAVE)){
			if(self->move_info.sound_end)
				gi.Sound(self, self->move_info.sound_end, ATTN_IDLE);
			self->s.sound = 0;
		}
	} else {
		G_func_train_next(self);
	}

}

static void G_func_train_next(edict_t *self){
	edict_t *ent;
	vec3_t dest;
	qboolean first;

	first = true;
again:
	if(!self->target)
		return;

	ent = G_PickTarget(self->target);
	if(!ent){
		gi.Debug("train_next: bad target %s\n", self->target);
		return;
	}

	self->target = ent->target;

	// check for a teleport path_corner
	if(ent->spawn_flags & 1){
		if(!first){
			gi.Debug("connected teleport path_corners, see %s at %s\n", ent->class_name, vtos(ent->s.origin));
			return;
		}
		first = false;
		VectorSubtract(ent->s.origin, self->mins, self->s.origin);
		VectorCopy(self->s.origin, self->s.old_origin);
		self->s.event = EV_TELEPORT;
		gi.LinkEntity(self);
		goto again;
	}

	self->move_info.wait = ent->wait;
	self->target_ent = ent;

	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_start)
			gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->move_info.sound_middle;
	}

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->move_info.state = STATE_TOP;
	VectorCopy(self->s.origin, self->move_info.start_origin);
	VectorCopy(dest, self->move_info.end_origin);
	G_MoveInfo_Init(self, dest, G_func_train_wait);
	self->spawn_flags |= TRAIN_START_ON;
}

static void G_func_train_resume(edict_t *self){
	edict_t *ent;
	vec3_t dest;

	ent = self->target_ent;

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->move_info.state = STATE_TOP;
	VectorCopy(self->s.origin, self->move_info.start_origin);
	VectorCopy(dest, self->move_info.end_origin);
	G_MoveInfo_Init(self, dest, G_func_train_wait);
	self->spawn_flags |= TRAIN_START_ON;
}

static void G_func_train_find(edict_t *self){
	edict_t *ent;

	if(!self->target){
		gi.Debug("train_find: no target\n");
		return;
	}
	ent = G_PickTarget(self->target);
	if(!ent){
		gi.Debug("train_find: target %s not found\n", self->target);
		return;
	}
	self->target = ent->target;

	VectorSubtract(ent->s.origin, self->mins, self->s.origin);
	gi.LinkEntity(self);

	// if not triggered, start immediately
	if(!self->target_name)
		self->spawn_flags |= TRAIN_START_ON;

	if(self->spawn_flags & TRAIN_START_ON){
		self->next_think = g_level.time + gi.server_frame;
		self->think = G_func_train_next;
		self->activator = self;
	}
}

static void G_func_train_use(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;

	if(self->spawn_flags & TRAIN_START_ON){
		if(!(self->spawn_flags & TRAIN_TOGGLE))
			return;
		self->spawn_flags &= ~TRAIN_START_ON;
		VectorClear(self->velocity);
		self->next_think = 0;
	} else {
		if(self->target_ent)
			G_func_train_resume(self);
		else
			G_func_train_next(self);
	}
}

void G_func_train(edict_t *self){
	self->move_type = MOVE_TYPE_PUSH;

	VectorClear(self->s.angles);
	self->blocked = G_func_train_blocked;
	if(self->spawn_flags & TRAIN_BLOCK_STOPS)
		self->dmg = 0;
	else {
		if(!self->dmg)
			self->dmg = 100;
	}
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	if(g_game.spawn.noise)
		self->move_info.sound_middle = gi.SoundIndex(g_game.spawn.noise);

	if(!self->speed)
		self->speed = 100;

	self->move_info.speed = self->speed;
	self->move_info.accel = self->move_info.decel = self->move_info.speed;

	self->use = G_func_train_use;

	gi.LinkEntity(self);

	if(self->target){
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->next_think = g_level.time + gi.server_frame;
		self->think = G_func_train_find;
	} else {
		gi.Debug("func_train without a target at %s\n", vtos(self->abs_mins));
	}
}


/*QUAKED func_timer(0.3 0.1 0.6)(-8 -8 -8)(8 8 8) START_ON
"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and(wait + random)

"delay"			delay before first firing when turned on, default is 0

"pausetime"		additional delay used only the very first time
				and only if spawned with START_ON

These can used but not touched.
*/
static void G_func_timer_think(edict_t *self){
	G_UseTargets(self, self->activator);
	self->next_think = g_level.time + self->wait + crand() * self->random;
}

static void G_func_timer_use(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;

	// if on, turn it off
	if(self->next_think){
		self->next_think = 0;
		return;
	}

	// turn it on
	if(self->delay)
		self->next_think = g_level.time + self->delay;
	else
		G_func_timer_think(self);
}

void G_func_timer(edict_t *self){
	if(!self->wait)
		self->wait = 1.0;

	self->use = G_func_timer_use;
	self->think = G_func_timer_think;

	if(self->random >= self->wait){
		self->random = self->wait - gi.server_frame;
		gi.Debug("func_timer at %s has random >= wait\n", vtos(self->s.origin));
	}

	if(self->spawn_flags & 1){
		self->next_think = g_level.time + 1.0 + g_game.spawn.pausetime + self->delay + self->wait + crand() * self->random;
		self->activator = self;
	}

	self->sv_flags = SVF_NOCLIENT;
}


/*QUAKED func_conveyor(0 .5 .8) ? START_ON TOGGLE
Conveyors are stationary brushes that move what's on them.
The brush should be have a surface with at least one current content enabled.
speed	default 100
*/

static void G_func_conveyor_use(edict_t *self, edict_t *other, edict_t *activator){
	if(self->spawn_flags & 1){
		self->speed = 0;
		self->spawn_flags &= ~1;
	} else {
		self->speed = self->count;
		self->spawn_flags |= 1;
	}

	if(!(self->spawn_flags & 2))
		self->count = 0;
}

void G_func_conveyor(edict_t *self){
	if(!self->speed)
		self->speed = 100;

	if(!(self->spawn_flags & 1)){
		self->count = self->speed;
		self->speed = 0;
	}

	self->use = G_func_conveyor_use;

	gi.SetModel(self, self->model);
	self->solid = SOLID_BSP;
	gi.LinkEntity(self);
}


/*QUAKED G_func_door_secret(0 .5 .8) ? always_shoot 1st_left 1st_down
A secret door.  Slide back and then to the side.

open_once		doors never closes
1st_left		1st move is left of arrow
1st_down		1st move is down from arrow
always_shoot	door is shootebale even if targeted

"angle"		determines the direction
"dmg"		damage to inflic when blocked(default 2)
"wait"		how long to hold in the open position(default 5, -1 means hold)
*/

#define SECRET_ALWAYS_SHOOT	1
#define SECRET_1ST_LEFT		2
#define SECRET_1ST_DOWN		4

static void G_func_door_secret_move1(edict_t *self);
static void G_func_door_secret_move2(edict_t *self);
static void G_func_door_secret_move3(edict_t *self);
static void G_func_door_secret_move4(edict_t *self);
static void G_func_door_secret_move5(edict_t *self);
static void G_func_door_secret_move6(edict_t *self);
static void G_func_door_secret_done(edict_t *self);

static void G_func_door_secret_use(edict_t *self, edict_t *other, edict_t *activator){
	// make sure we're not already moving
	if(!VectorCompare(self->s.origin, vec3_origin))
		return;

	G_MoveInfo_Init(self, self->pos1, G_func_door_secret_move1);
	G_func_door_use_areaportals(self, true);
}

static void G_func_door_secret_move1(edict_t *self){
	self->next_think = g_level.time + 1.0;
	self->think = G_func_door_secret_move2;
}

static void G_func_door_secret_move2(edict_t *self){
	G_MoveInfo_Init(self, self->pos2, G_func_door_secret_move3);
}

static void G_func_door_secret_move3(edict_t *self){
	if(self->wait == -1)
		return;
	self->next_think = g_level.time + self->wait;
	self->think = G_func_door_secret_move4;
}

static void G_func_door_secret_move4(edict_t *self){
	G_MoveInfo_Init(self, self->pos1, G_func_door_secret_move5);
}

static void G_func_door_secret_move5(edict_t *self){
	self->next_think = g_level.time + 1.0;
	self->think = G_func_door_secret_move6;
}

static void G_func_door_secret_move6(edict_t *self){
	G_MoveInfo_Init(self, vec3_origin, G_func_door_secret_done);
}

static void G_func_door_secret_done(edict_t *self){
	if(!(self->target_name) ||(self->spawn_flags & SECRET_ALWAYS_SHOOT)){
		self->health = 0;
		self->take_damage = true;
	}
	G_func_door_use_areaportals(self, false);
}

static void G_func_door_secret_blocked(edict_t *self, edict_t *other){
	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	self->touch_time = g_level.time + 0.5;
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void G_func_door_secret_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	self->take_damage = false;
	G_func_door_secret_use(self, attacker, attacker);
}

void G_func_door_secret(edict_t *ent){
	vec3_t forward, right, up;
	float side;
	float width;
	float length;

	ent->move_info.sound_start = gi.SoundIndex("world/door_start");
	ent->move_info.sound_end = gi.SoundIndex("world/door_end");

	ent->move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->blocked = G_func_door_secret_blocked;
	ent->use = G_func_door_secret_use;

	if(!(ent->target_name) ||(ent->spawn_flags & SECRET_ALWAYS_SHOOT)){
		ent->health = 0;
		ent->take_damage = true;
		ent->die = G_func_door_secret_die;
	}

	if(!ent->dmg)
		ent->dmg = 2;

	if(!ent->wait)
		ent->wait = 5;

	ent->move_info.accel =
		ent->move_info.decel =
			ent->move_info.speed = 50;

	// calculate positions
	AngleVectors(ent->s.angles, forward, right, up);
	VectorClear(ent->s.angles);
	side = 1.0 -(ent->spawn_flags & SECRET_1ST_LEFT);
	if(ent->spawn_flags & SECRET_1ST_DOWN)
		width = fabsf(DotProduct(up, ent->size));
	else
		width = fabsf(DotProduct(right, ent->size));
	length = fabsf(DotProduct(forward, ent->size));
	if(ent->spawn_flags & SECRET_1ST_DOWN)
		VectorMA(ent->s.origin, -1 * width, up, ent->pos1);
	else
		VectorMA(ent->s.origin, side * width, right, ent->pos1);
	VectorMA(ent->pos1, length, forward, ent->pos2);

	if(ent->health){
		ent->take_damage = true;
		ent->die = G_func_door_die;
		ent->max_health = ent->health;
	} else if(ent->target_name && ent->message){
		gi.SoundIndex("misc/chat");
		ent->touch = G_func_door_touch;
	}

	ent->class_name = "func_door";

	gi.LinkEntity(ent);
}

/*QUAKED func_killbox(1 0 0) ?
Kills everything inside when fired, regardless of protection.
*/
static void G_func_killbox_use(edict_t *self, edict_t *other, edict_t *activator){
	G_KillBox(self);
}

void G_func_killbox(edict_t *ent){
	gi.SetModel(ent, ent->model);
	ent->use = G_func_killbox_use;
	ent->sv_flags = SVF_NOCLIENT;
}
