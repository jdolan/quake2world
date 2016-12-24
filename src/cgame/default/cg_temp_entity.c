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
#include "game/default/bg_pmove.h"

/**
 * @brief
 */
static void Cg_BlasterEffect(const vec3_t org, const vec3_t dir, int32_t color) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i, j;

	if (!color) {
		color = EFFECT_COLOR_ORANGE;
	}

	for (i = 0; i < 24; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_spark))) {
			break;
		}

		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;
		p->lifetime = 450 + Randomf() * 450;

		cgi.ColorFromPalette(color + (Random() & 5), p->color_start);
		p->color_start[3] = 2.0;
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 3.5;
		p->scale_end = Randomf() * 1.0;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 150.0, p->vel);

		for (j = 0; j < 3; j++) {
			p->vel[j] += Randomc() * 50.0;
		}

		p->accel[2] -= 2.0 * PARTICLE_GRAVITY;
	}

	cgi.ColorFromPalette(color, s.light.color);

	VectorScale(s.light.color, 3.0, s.light.color);

	for (i = 0; i < 3; i++) {
		if (s.light.color[i] > 1.0) {
			s.light.color[i] = 1.0;
		}
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		 .color = { s.light.color[0], s.light.color[1], s.light.color[2], 0.2 },
		  .radius = 24.0
	});

	VectorAdd(org, dir, s.light.origin);
	s.light.radius = 150.0;
	s.sustain = 250;

	cgi.AddSustainedLight(&s);

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_blaster_hit,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_NORM,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_TracerEffect(const vec3_t start, const vec3_t end) {
	cg_particle_t *p;

	if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_beam))) {
		return;
	}

	p->lifetime = 250;
	p->effects |= PARTICLE_EFFECT_COLOR;

	cgi.ColorFromPalette(14, p->color_start);
	VectorCopy(p->color_start, p->color_end);
	p->color_start[3] = 0.2;
	p->color_end[3] = 0.0;

	p->part.scale = 1.0;

	VectorCopy(start, p->part.org);
	VectorCopy(end, p->part.end);

	VectorSubtract(end, start, p->vel);
	VectorScale(p->vel, 2.0, p->vel);

	const vec_t v = VectorNormalize(p->vel);
	VectorScale(p->vel, v < 1000.0 ? v : 1000.0, p->vel);
}

/**
 * @brief Make a decal!
 */
static void Cg_DecalEffect(const vec3_t org, const vec3_t dir, const vec_t scale, cg_particles_t *ps) {

	cg_particle_t *p = Cg_AllocParticle(PARTICLE_DECAL, ps);
	vec3_t v;

	if (!p) {
		return;
	}

	p->part.blend = GL_ONE_MINUS_SRC_ALPHA;

	p->effects = PARTICLE_EFFECT_COLOR;
	p->lifetime = 5000;

	cgi.ColorFromPalette(Random() & 1, p->color_start);
	VectorCopy(p->color_start, p->color_end);
	p->color_start[3] = 2.0;
	p->color_end[3] = 0.0;

	p->part.scale = scale;

	VectorScale(dir, -1.0, v);
	VectorAngles(v, p->part.dir);
	p->part.dir[ROLL] = Random() % 360;

	VectorAdd(org, dir, p->part.org);
}

/**
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
	static uint32_t last_ric_time;
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t j, k;

	Cg_DecalEffect(org, dir, 1.5 + Randomc() * 0.4, cg_particles_bullet[Random() % 3]);

	k = 1 + (Random() % 5);

	while (k--) {
		if ((p = Cg_AllocParticle(PARTICLE_SPARK, cg_particles_beam))) {

			p->lifetime = 50 + Randomf() * 150;

			cgi.ColorFromPalette(221 + (Random() & 7), p->part.color);
			p->part.color[3] = 0.7 + Randomf() * 0.3;

			p->part.scale = 0.6 + Randomf() * 0.2;

			VectorCopy(org, p->part.org);

			VectorScale(dir, 290.0 + Randomf() * 50.0, p->vel);

			for (j = 0; j < 3; j++) {
				p->vel[j] += Randomc() * 40.0;
			}

			p->accel[2] = -0.75 * PARTICLE_GRAVITY;
			p->spark.length = 0.025 + Randomf() * 0.01;

			VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
		}
	}

	if ((cgi.PointContents(org) & MASK_LIQUID) == 0) {

		if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_smoke))) {

			p->part.roll = Randomc() * 50.0;
			p->lifetime = 750;
			p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

			cgi.ColorFromPalette(7 + (Random() & 7), p->color_start);
			VectorCopy(p->color_start, p->color_end);
			p->color_end[3] = 0.0;

			p->scale_start = 2.0;
			p->scale_end = 5.0 + Randomf() * 3.0;

			VectorCopy(org, p->part.org);
			VectorScale(dir, 50.0 + (Randomc() * 15.0), p->vel);
			VectorScale(dir, -75.0, p->accel);
			VectorMA(p->accel, 15.5, vec3_up, p->accel);
		}
	}

	VectorAdd(org, dir, s.light.origin);
	s.light.radius = 20.0;
	VectorSet(s.light.color, 0.5, 0.3, 0.2);
	s.sustain = 250;

	cgi.AddSustainedLight(&s);

	cgi.AddStain(&(const r_stain_t) {
		 .origin = { org[0], org[1], org[2] },
		  .color = { 0.0, 0.0, 0.0, 0.4 },
		   .radius = 6.0
	});

	if (cgi.client->ticks < last_ric_time) {
		last_ric_time = 0;
	}

	if (cgi.client->ticks - last_ric_time > 300) {
		last_ric_time = cgi.client->ticks;

		cgi.AddSample(&(const s_play_sample_t) {
			.sample = cg_sample_machinegun_hit[Random() % 3],
			 .origin = { org[0], org[1], org[2] },
			  .attenuation = ATTEN_NORM,
			   .flags = S_PLAY_POSITIONED
		});
	}
}

/**
 * @brief
 */
static void Cg_BurnEffect(const vec3_t org, const vec3_t dir, const vec4_t color, int32_t scale) {

	Cg_DecalEffect(org, dir, scale, cg_particles_burn);

	r_stain_t stain;
	VectorCopy(org, stain.origin);
	Vector4Copy(color, stain.color);
	stain.radius = scale;

	cgi.AddStain(&stain);
}

/**
 * @brief
 */
static void Cg_BloodEffect(const vec3_t org, const vec3_t dir, int32_t count) {
	int32_t i, j;

	for (i = 0; i < count; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_blood))) {
			break;
		}

		p->lifetime = 800 + Randomf() * 200;
		p->effects = PARTICLE_EFFECT_COLOR;

		cgi.ColorFromPalette(232 + (Random() & 7), p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = 6.0;

		const vec_t d = Random() & 31;
		for (j = 0; j < 3; j++) {
			p->part.org[j] = org[j] + ((Random() & 7) - 4.0) + d * dir[j];
			p->vel[j] = Randomc() * 20.0;
		}
		p->part.org[2] += 16.0 * PM_SCALE;

		p->accel[0] = p->accel[1] = 0.0;
		p->accel[2] = -PARTICLE_GRAVITY / 4.0;
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		 .color = { 0.9 + Randomc(), 0.0, 0.0, 0.08 },
		  .radius = count * 4.0
	});
}

#define GIB_STREAM_DIST 180.0
#define GIB_STREAM_COUNT 24

/**
 * @brief
 */
void Cg_GibEffect(const vec3_t org, int32_t count) {
	cg_particle_t *p;
	vec3_t o, v, tmp;
	cm_trace_t tr;
	vec_t dist;
	int32_t i, j;

	// if a player has died underwater, emit some bubbles
	if (cgi.PointContents(org) & MASK_LIQUID) {
		VectorCopy(org, tmp);
		tmp[2] += 64.0;

		Cg_BubbleTrail(org, tmp, 16.0);
	}

	for (i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		VectorSet(o, Randomc() * 8.0, Randomc() * 8.0, 8.0 + Randomc() * 12.0);
		VectorAdd(o, org, o);

		VectorSet(v, Randomc(), Randomc(), 0.2 + Randomf());

		dist = GIB_STREAM_DIST;
		VectorMA(o, dist, v, tmp);

		tr = cgi.Trace(o, tmp, NULL, NULL, 0, MASK_CLIP_PROJECTILE);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_blood))) {
				break;
			}

			p->lifetime = 350;
			p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

			cgi.ColorFromPalette(232 + (Random() & 7), p->color_start);
			VectorCopy(p->color_start, p->color_end);
			p->color_end[0] = 0.0;

			p->scale_start = 6.0 + Randomf();
			p->scale_end = 0.5 + (Randomc() * 0.5);

			VectorCopy(o, p->part.org);

			VectorScale(v, dist * ((vec_t)j / GIB_STREAM_COUNT), p->vel);
			p->vel[0] += Randomc() * 2.0;
			p->vel[1] += Randomc() * 2.0;
			p->vel[2] += 100.0;

			p->accel[0] = p->accel[1] = 0.0;
			p->accel[2] = -PARTICLE_GRAVITY * 2.0;
		}
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = { org[0], org[1], org[2] },
		 .color = { 0.9 + Randomc(), 0.0, 0.0, 0.09 },
		  .radius = count * 8.0
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_gib,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_NORM,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
void Cg_SparksEffect(const vec3_t org, const vec3_t dir, int32_t count) {
	r_sustained_light_t s;
	int32_t i, j;

	for (i = 0; i < count; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_SPARK, cg_particles_spark))) {
			break;
		}

		p->lifetime = 50 + Randomf() * 150;

		cgi.ColorFromPalette(221 + (Random() & 7), p->part.color);
		p->part.color[3] = 0.7 + Randomf() * 0.3;

		p->part.scale = 0.6 + Randomf() * 0.2;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 4, p->vel);

		for (j = 0; j < 3; j++) {
			p->part.org[j] += Randomc() * 4.0;
			p->vel[j] += Randomc() * 90.0;
		}

		p->accel[0] = Randomc() * 1.0;
		p->accel[1] = Randomc() * 1.0;
		p->accel[2] = -0.5 * PARTICLE_GRAVITY;
		p->spark.length = 0.15;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.7, 0.5, 0.5);
	s.sustain = 650;

	cgi.AddSustainedLight(&s);

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_sparks,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_STATIC,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_ExplosionEffect(const vec3_t org) {
	cg_particle_t *p;
	r_sustained_light_t s;

	if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion))) {

		p->lifetime = 250;
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(224, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 6.0;
		p->scale_end = 128.0;

		p->part.roll = Randomc() * 100.0;

		VectorCopy(org, p->part.org);
	}

	if (!(cgi.PointContents(org) & MASK_LIQUID)) {

		if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_smoke))) {

			p->lifetime = 800;
			p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

			const vec_t smoke_color = 0.7 + Randomf() * 0.1;

			VectorSet(p->color_start, smoke_color, smoke_color, smoke_color);
			VectorClear(p->color_end);

			p->scale_start = 12.0;
			p->scale_end = 58.0;

			p->part.roll = Randomc() * 100.0;

			VectorCopy(org, p->part.org);
			p->part.org[2] += 10;

			VectorSet(p->vel, Randomc(), Randomc(), Randomc());

			p->accel[2] = 20.0;
		}
	}

	for (int32_t i = 0; i < 128; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
			break;
		}

		p->lifetime = 250 + Randomf() * 250;
		p->effects = PARTICLE_EFFECT_COLOR;

		cgi.ColorFromPalette(0xe0 + (Random() & 7), p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = 2.0;

		p->part.org[0] = org[0] + (Random() % 32) - 16.0;
		p->part.org[1] = org[1] + (Random() % 32) - 16.0;
		p->part.org[2] = org[2] + (Random() % 32) - 16.0;

		p->vel[0] = Randomc() * 800.0;
		p->vel[1] = Randomc() * 800.0;
		p->vel[2] = Randomc() * 800.0;

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY);
	}

	for (int32_t i = 0; i < 32; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_debris[Random() % 4]))) {
			break;
		}

		p->lifetime = 700;
		p->effects = PARTICLE_EFFECT_COLOR;

		VectorSet(p->color_start, 1.0, 1.0, 1.0);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->part.scale = 4.0;

		p->part.org[0] = org[0] + (Random() % 32) - 16.0;
		p->part.org[1] = org[1] + (Random() % 32) - 16.0;
		p->part.org[2] = org[2] + (Random() % 32) - 16.0;

		p->vel[0] = Randomc() * 200.0;
		p->vel[1] = Randomc() * 200.0;
		p->vel[2] = Randomc() * 200.0;

		p->part.roll = Randomc() * 1000.0;

		p->part.blend = GL_ONE_MINUS_SRC_ALPHA;

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY * 2.0);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 200.0;
	VectorSet(s.light.color, 0.8, 0.4, 0.2);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_explosion,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_NORM,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_HyperblasterEffect(const vec3_t org) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i;

	for (i = 0; i < 2; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion))) {
			break;
		}

		p->lifetime = 150;
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(113 + Random() % 3, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 1.5;
		p->scale_end = 25.0 * (i + 1);

		p->part.roll = 100.0 * Randomc();

		VectorCopy(org, p->part.org);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.4, 0.7, 1.0);
	s.sustain = 250;

	cgi.AddSustainedLight(&s);

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_hyperblaster_hit,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_NORM,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_LightningEffect(const vec3_t org) {
	r_sustained_light_t s;
	vec3_t tmp;
	int32_t i, j;

	for (i = 0; i < 40; i++) {

		VectorCopy(org, tmp);

		for (j = 0; j < 3; j++) {
			tmp[j] = tmp[j] + (Random() % 96) - 48.0;
		}

		Cg_BubbleTrail(org, tmp, 4.0);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 160.0;
	VectorSet(s.light.color, 0.6, 0.6, 1.0);
	s.sustain = 750;

	cgi.AddSustainedLight(&s);

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_lightning_discharge,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_NORM,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_RailEffect(const vec3_t start, const vec3_t end, const vec3_t dir, int32_t flags, int32_t color) {
	vec3_t vec, right, up, point;
	cg_particle_t *p;
	r_sustained_light_t s;

	color = color ? color : EFFECT_COLOR_BLUE;

	VectorCopy(start, s.light.origin);
	s.light.radius = 100.0;
	cgi.ColorFromPalette(color, s.light.color);
	s.sustain = 500;

	cgi.AddSustainedLight(&s);

	// draw the core with a beam

	if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_beam))) {
		return;
	}

	// white cores for some colors, shifted for others
	switch (color) {
		case EFFECT_COLOR_RED:
			cgi.ColorFromPalette(229, p->color_start);
			break;
		case EFFECT_COLOR_BLUE:
		case EFFECT_COLOR_GREEN:
		case EFFECT_COLOR_PURPLE:
			cgi.ColorFromPalette(216, p->color_start);
			break;
		default:
			cgi.ColorFromPalette(color + 6, p->color_start);
			break;
	}

	p->lifetime = 1000;
	p->effects = PARTICLE_EFFECT_COLOR;

	Vector4Set(p->color_end, 1.0, 1.0, 1.0, 0.0);

	p->part.scale = 3.0;

	VectorCopy(start, p->part.org);
	VectorCopy(end, p->part.end);

	// and the spiral with normal parts
	VectorCopy(start, point);

	VectorSubtract(end, start, vec);
	const vec_t len = VectorNormalize(vec);

	VectorSet(right, vec[2], -vec[0], vec[1]);
	CrossProduct(vec, right, up);

	for (int32_t i = 0; i < len && i < 2048; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
			return;
		}

		p->lifetime = 500 + ((i / len) * 500.0);
		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(color, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 1.5 + Randomc() * 0.2;
		p->scale_end = p->scale_start + 1.0 + Randomc() * 0.2;

		VectorAdd(point, vec, point);
		VectorCopy(point, p->part.org);

		VectorScale(vec, 2.0, p->vel);

		const vec_t cosi = cos(i * 0.1);
		const vec_t sini = sin(i * 0.1);

		VectorMA(p->part.org, cosi * 2.0, right, p->part.org);
		VectorMA(p->part.org, sini * 2.0, up, p->part.org);

		VectorMA(p->vel, cosi * 8.0, right, p->vel);
		VectorMA(p->vel, sini * 8.0, up, p->vel);

		// check for bubble trail
		if (i % 24 == 0 && (cgi.PointContents(point) & MASK_LIQUID)) {
			Cg_BubbleTrail(point, p->part.org, 16.0);
		}

		// add sustained lights
		if (i > 0 && i < len - 64.0 && i % 64 == 0) {
			VectorCopy(point, s.light.origin);
			s.sustain += 25;
			cgi.AddSustainedLight(&s);
		}
	}

	// check for explosion effect on solids
	if (flags & SURF_SKY) {
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_explosion))) {
		return;
	}

	p->lifetime = 250;
	p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

	cgi.ColorFromPalette(color, p->color_start);
	VectorCopy(p->color_start, p->color_end);
	p->color_start[3] = 1.25;
	p->color_end[3] = 0.0;

	p->scale_start = 1.0;
	p->scale_end = 40.0;

	VectorCopy(end, p->part.org);

	VectorMA(end, -12.0, vec, s.light.origin);
	s.light.radius = 120.0;
	s.sustain += 250;

	cgi.AddSustainedLight(&s);

	Cg_BurnEffect(end, dir, (const vec4_t) {
		s.light.color[0], s.light.color[1], s.light.color[2], 0.1
	}, 16);
}

/**
 * @brief
 */
static void Cg_BfgLaserEffect(const vec3_t org, const vec3_t end) {
	cg_particle_t *p;
	r_sustained_light_t s;

	if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_beam))) {
		return;
	}

	VectorCopy(org, p->part.org);
	VectorCopy(end, p->part.end);

	p->lifetime = 50;

	cgi.ColorFromPalette(200 + Random() % 3, p->part.color);

	p->part.scale = 6.0;

	p->part.scroll_s = -4.0;

	VectorCopy(end, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.8, 1.0, 0.5);
	s.sustain = 50;

	cgi.AddSustainedLight(&s);
}

/**
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
	cg_particle_t *p;
	r_sustained_light_t s;
	int32_t i;

	for (i = 0; i < 4; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion))) {
			break;
		}

		p->lifetime = 500;
		p->effects |= PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		cgi.ColorFromPalette(200 + Random() % 3, p->color_start);
		VectorCopy(p->color_start, p->color_end);
		p->color_end[3] = 0.0;

		p->scale_start = 6.0;
		p->scale_end = 48.0 * (i + 1);

		p->part.roll = 100.0 * Randomc();

		VectorCopy(org, p->part.org);
	}

	for (i = 0; i < 128; i++) {

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
			break;
		}

		p->lifetime = 750;
		p->effects |= PARTICLE_EFFECT_COLOR;

		cgi.ColorFromPalette(206, p->color_start);
		Vector4Set(p->color_end, 1.0, 2.0, 0.0, 0.0);

		p->part.scale = 2.0;

		p->part.org[0] = org[0] + (Random() % 48) - 24;
		p->part.org[1] = org[1] + (Random() % 48) - 24;
		p->part.org[2] = org[2] + (Random() % 48) - 24;

		p->vel[0] = (Random() % 512) - 256;
		p->vel[1] = (Random() % 512) - 256;
		p->vel[2] = (Random() % 512) - 256;

		VectorSet(p->accel, 0.0, 0.0, -3.0 * PARTICLE_GRAVITY);
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 200.0;
	VectorSet(s.light.color, 0.8, 1.0, 0.5);
	s.sustain = 1000;

	cgi.AddSustainedLight(&s);

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_bfg_hit,
		 .origin = { org[0], org[1], org[2] },
		  .attenuation = ATTEN_NORM,
		   .flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_RippleEffect(const vec3_t org, const vec3_t dir, const vec_t size, const uint8_t viscosity) {
	cg_particle_t *p;
	int32_t i;

	if (!(p = Cg_AllocParticle(PARTICLE_SPLASH, cg_particles_ripple[Random() % 3]))) {
		return;
	}

	p->lifetime = (500 + (Random() % 1500)) * (viscosity * 0.1);

	p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;
	p->part.flags = PARTICLE_FLAG_NO_DEPTH;

	Vector4Set(p->color_start, 1.0, 1.0, 1.0, 0.5 + (Randomf() * 1.5));
	Vector4Set(p->color_end, 1.0, 1.0, 1.0, 0.0);

	p->scale_start = size / 4.0;
	p->scale_end = size * ((Randomf() * 0.1) + 0.9);

	VectorCopy(org, p->part.org);

	for (i = 0; i < 10; i++) {
		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_normal))) {
			break;
		}

		p->lifetime = 200 + Randomc() * 50;

		p->effects = PARTICLE_EFFECT_COLOR | PARTICLE_EFFECT_SCALE;

		Vector4Set(p->color_start, 1.0, 1.0, 1.0, 2.0);
		Vector4Set(p->color_end, 1.0, 1.0, 1.0, 0.0);

		p->scale_start = 0.8;
		p->scale_end = 0.5;

		p->part.org[0] = org[0] + (Randomc() * 8.0);
		p->part.org[1] = org[1] + (Randomc() * 8.0);
		p->part.org[2] = org[2] + (Randomf() * 8.0);

		p->vel[0] = Randomc() * 64.0;
		p->vel[1] = Randomc() * 64.0;
		p->vel[2] = 36.0 + Randomf() * 64.0;

		VectorSet(p->accel, 0.0, 0.0, -PARTICLE_GRAVITY / 2.0);
	}

	if ((p = Cg_AllocParticle(PARTICLE_SPARK, cg_particles_beam))) {

		p->lifetime = 150 + Randomf() * 150;
		p->effects = PARTICLE_EFFECT_COLOR;

		Vector4Set(p->color_start, 0.8, 0.8, 0.8, 0.45);
		Vector4Set(p->color_end, 0.8, 0.8, 0.8, 0.0);

		p->part.scale = 1.2 + Randomf() * 0.6;

		VectorCopy(org, p->part.org);

		VectorScale(dir, 24.0 + Randomf() * 25.0, p->vel);

		p->spark.length = 0.7 + Randomf() * 0.01;

		VectorMA(p->part.org, p->spark.length, p->vel, p->part.end);
	}
}

/**
 * @brief
 */
void Cg_ParseTempEntity(void) {
	vec3_t pos, pos2, dir;
	vec_t size;
	int32_t i, j;
	uint8_t viscosity;

	const uint8_t type = cgi.ReadByte();

	switch (type) {

		case TE_BLASTER:
			cgi.ReadPosition(pos);
			cgi.ReadPosition(dir);
			i = cgi.ReadByte();
			Cg_BlasterEffect(pos, dir, i);
			break;

		case TE_TRACER:
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			Cg_TracerEffect(pos, pos2);
			break;

		case TE_BULLET: // bullet hitting wall
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_BulletEffect(pos, dir);
			break;

		case TE_BURN: // burn mark on wall
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			i = cgi.ReadByte();
			Cg_BurnEffect(pos, dir, (const vec4_t) {
				0.0, 0.0, 0.0, 0.25
			}, i);
			break;

		case TE_BLOOD: // projectile hitting flesh
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_BloodEffect(pos, dir, 12);
			break;

		case TE_GIB: // player over-death
			cgi.ReadPosition(pos);
			Cg_GibEffect(pos, 12);
			break;

		case TE_SPARKS: // colored sparks
			cgi.ReadPosition(pos);
			cgi.ReadDir(dir);
			Cg_SparksEffect(pos, dir, 12);
			break;

		case TE_HYPERBLASTER: // hyperblaster hitting wall
			cgi.ReadPosition(pos);
			Cg_HyperblasterEffect(pos);
			break;

		case TE_LIGHTNING: // lightning discharge in water
			cgi.ReadPosition(pos);
			Cg_LightningEffect(pos);
			break;

		case TE_RAIL: // railgun effect
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			cgi.ReadDir(dir);
			i = cgi.ReadLong();
			j = cgi.ReadByte();
			Cg_RailEffect(pos, pos2, dir, i, j);
			break;

		case TE_EXPLOSION: // rocket and grenade explosions
			cgi.ReadPosition(pos);
			Cg_ExplosionEffect(pos);
			break;

		case TE_BFG_LASER:
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			Cg_BfgLaserEffect(pos, pos2);
			break;

		case TE_BFG: // bfg explosion
			cgi.ReadPosition(pos);
			Cg_BfgEffect(pos);
			break;

		case TE_BUBBLES: // bubbles chasing projectiles in water
			cgi.ReadPosition(pos);
			cgi.ReadPosition(pos2);
			Cg_BubbleTrail(pos, pos2, 1.0);
			break;

		case TE_RIPPLE: // liquid surface ripples
			cgi.ReadPosition(pos);
			size = cgi.ReadVector();
			viscosity = cgi.ReadByte();
			cgi.ReadDir(dir);
			Cg_RippleEffect(pos, dir, size, viscosity);
			break;

		default:
			cgi.Warn("Unknown type: %d\n", type);
			return;
	}
}
