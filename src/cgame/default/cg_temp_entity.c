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
static void Cg_BlasterEffect(const vec3_t org, const vec3_t dir, const float hue) {
	const vec4_t color = Vec4(hue, 1.f, 1.f, 1.f);
	const color_t color_rgb = ColorHSVA(color.x, color.y, color.z, color.w);

	for (int32_t i = 0; i < 2; i++)
	{
		const float saturation = RandomRangef(.8f, 1.f);

		// surface aligned blast ring sprite
		Cg_AddSprite(&(cg_sprite_t) {
			.animation = cg_sprite_blaster_ring,
			.lifetime = Cg_AnimationLifetime(cg_sprite_blaster_ring, 17.5f),
			.origin = Vec3_Add(org, Vec3_Scale(dir, 3.0)),
			.size = 22.5f,
			.size_velocity = 75.f,
			.dir = (i == 1) ? dir : Vec3_Zero(),
			.color = Vec4(hue, saturation, 1.f, 0.f),
			.end_color = Vec4(hue, saturation, 0.f, 0.f)
		});
	}

	// radial particles
	for (int32_t i = 0; i < 32; i++) {

		const vec3_t velocity = Vec3_RandomizeDir(Vec3_Scale(dir, 125.f), .6666f);

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.origin = Vec3_Add(org, Vec3_Scale(dir, 3.0)),
			.velocity = velocity,
			.size = 4.f,
			.acceleration = Vec3_Scale(velocity, -2.f),
			.lifetime = 500,
			.color = Vec4(hue, 1.f, 1.f, 0.f),
			.end_color = Vec4(hue, 1.f, 0.f, 0.f)
		});
	}

	// residual flames
	for (int32_t i = 0; i < 3; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.animation = cg_sprite_blaster_flame,
			.lifetime = Cg_AnimationLifetime(cg_sprite_blaster_flame, 30),
			.origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 5.f)),
			.rotation = Randomf() * M_PI * 2.f,
			.rotation_velocity = Randomf() * .1f,
			.size = 25.f,
			.color = Vec4(hue, 1.f, 1.f, 0.f),
			.end_color = Vec4(hue, 1.f, 0.f, 0.f)
		});
	}

	// surface flame
	const float flame_sat = RandomRangef(.8f, 1.f);

	Cg_AddSprite(&(cg_sprite_t) {
		.animation = cg_sprite_blaster_flame,
		.lifetime = Cg_AnimationLifetime(cg_sprite_blaster_flame, 30),
		.origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 5.f)),
		.rotation = Randomf() * M_PI * 2.f,
		.rotation_velocity = Randomf() * .1f,
		.dir = dir,
		.size = 25.f,
		.size_velocity = 20.f,
		.color = Vec4(hue, flame_sat, 1.f, 0.f),
		.end_color = Vec4(hue, flame_sat, 0.f, 0.f),
	});

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, Vec3_Scale(dir, 8.f)),
		.radius = 100.f,
		.color = Color_Vec3(color_rgb),
		.intensity = .125f,
		.decay = 350.f
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 4.0,
		.color = Color_Add(color_rgb, Color4f(0.f, 0.f, 0.f, -.66f))
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_blaster_hit,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_TracerEffect(const vec3_t start, const vec3_t end) {
	vec3_t velocity;
	const float dist = Vec3_DistanceDir(end, start, &velocity);

	Cg_AddSprite(&(cg_sprite_t) {
		.atlas_image = cg_sprite_particle,
		.origin = start,
		.velocity = Vec3_Scale(velocity, 8000.f),
		.acceleration.z = -SPRITE_GRAVITY * 3.f,
		.lifetime = SECONDS_TO_MILLIS(dist / 8000.f),
		.size = 8.f,
		.color = Vec4(27.f, .68f, 1.f, 1.f),
		.end_color = Vec4(27.f, .68f, 0.f, 0.f)
	});
}

/**
 * @brief
 */
static void Cg_BulletEffect(const vec3_t org, const vec3_t dir) {
	static uint32_t last_ric_time;

	if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {

		Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3_Scale(dir, 8.f)), 32.0);
	} else {

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.origin = Vec3_Add(org, dir),
			.velocity = Vec3_Scale(dir, RandomRangef(100.f, 200.f)),
			.size = 4.f,
			.acceleration = Vec3_Subtract(Vec3_RandomRange(-40.f, 40.f), Vec3(0.f, 0.f, SPRITE_GRAVITY)),
			.lifetime = RandomRangef(150.f, 300.f),
			.color = Vec4(27.f, .68f, RandomRangef(.5f, 1.f), RandomRangef(.5f, 1.f)),
			.end_color = Vec4(27.f, .68f, 0.f, 0.f)
		});

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.origin = Vec3_Add(org, dir),
			.lifetime = 1000,
			.size = 8.f,
			.color = Vec4(27.f, .68f, 1.f, 1.f),
			.end_color = Vec4(27.f, .68f, 0.f, 0.f)
		});

		if (Randomf() < .75f) {
			Cg_AddSprite(&(cg_sprite_t) {
				.animation = cg_sprite_poof_02,
				.lifetime = Cg_AnimationLifetime(cg_sprite_poof_02, 20),
				.origin = Vec3_Add(org, dir),
				.size_velocity = 100.f,
				.rotation = Randomf() * M_PI * 2.f,
				.color = Vec4(0.f, 0.f, 1.f, 0.f)
			});
		} else {
			Cg_AddSprite(&(cg_sprite_t) {
				.animation = cg_sprite_smoke_05,
				.lifetime = Cg_AnimationLifetime(cg_sprite_smoke_05, 80),
				.origin = Vec3_Add(org, Vec3_Scale(dir, 3.f)),
				.dir = dir,
				.rotation = Randomf() * M_PI * 2.f,
				.color = Vec4(0.f, 0.f, 1.f, 0.f)
			});
		}
	}

	/*
	if ((p = Cg_AllocSprite())) {

		p->origin = org;
		p->velocity = Vec3_Scale(dir, RandomRangef(20.f, 30.f));
		p->acceleration = Vec3_Scale(dir, -20.f);
		p->acceleration.z = RandomRangef(20.f, 30.f);

		p->lifetime = RandomRangef(600.f, 1000.f);

		p->color = Color_Add(Color3bv(0xa0a0a0), Color3fv(Vec3_RandomRange(-1.f, .1f)));

		p->size = RandomRangef(1.f, 2.f);
		p->size_velocity = 1.f;
	}
	*/

	/*
	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 20.0,
		.color = Vec3(0.5f, 0.3f, 0.2f),
		.decay = 250
	});
	*/

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 1.f,
		.color = Color4bv(0xbb202020),
	});

	if (cgi.client->unclamped_time < last_ric_time) {
		last_ric_time = 0;
	}

	if (cgi.client->unclamped_time - last_ric_time > 300) {
		last_ric_time = cgi.client->unclamped_time;

		cgi.AddSample(&(const s_play_sample_t) {
			.sample = cg_sample_machinegun_hit[RandomRangeu(0, 3)],
			.origin = org,
			.attenuation = ATTEN_NORM,
			.flags = S_PLAY_POSITIONED,
			.pitch = RandomRangei(-8, 9)
		});
	}
}

/**
 * @brief
 */
static void Cg_BloodEffect(const vec3_t org, const vec3_t dir, int32_t count) {

	for (int32_t i = 0; i < count; i++) {

		if (!Cg_AddSprite(&(cg_sprite_t) {
				.animation = cg_sprite_blood_01,
				.lifetime = Cg_AnimationLifetime(cg_sprite_blood_01, 30) + Randomf() * 500,
				.size = RandomRangef(40.f, 64.f),
				.rotation = RandomRadian(),
				.origin = Vec3_Add(Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f)), Vec3_Scale(dir, RandomRangef(0.f, 32.f))),
				.velocity = Vec3_RandomRange(-30.f, 30.f),
				.acceleration.z = -SPRITE_GRAVITY / 2.0,
				.flags = SPRITE_LERP,
				.color = Vec4(0.f, 1.f, .5f, .66f),
				.end_color = Vec4(0.f, 1.f, 0.f, 0.f)
			})) {
			break;
		}
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = count * 6.0,
		.color = Color4bv(0xAA2222AA),
	});
}

#define GIB_STREAM_DIST 220.0
#define GIB_STREAM_COUNT 12

/**
 * @brief
 */
void Cg_GibEffect(const vec3_t org, int32_t count) {

	// if a player has died underwater, emit some bubbles
	if (cgi.PointContents(org) & CONTENTS_MASK_LIQUID) {

		Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3(0.f, 0.f, 64.f)), 16.0);
	}

	for (int32_t i = 0; i < count; i++) {

		// set the origin and velocity for each gib stream
		const vec3_t o = Vec3_Add(Vec3(RandomRangef(-8.f, 8.f), RandomRangef(-8.f, 8.f), RandomRangef(-4.f, 20.f)), org);
		const vec3_t v = Vec3(RandomRangef(-1.f, 1.f), RandomRangef(-1.f, 1.f), RandomRangef(.2f, 1.2f));

		float dist = GIB_STREAM_DIST;
		vec3_t tmp = Vec3_Add(o, Vec3_Scale(v, dist));

		const cm_trace_t tr = cgi.Trace(o, tmp, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE);
		dist = GIB_STREAM_DIST * tr.fraction;

		for (int32_t j = 1; j < GIB_STREAM_COUNT; j++) {

			if (!Cg_AddSprite(&(cg_sprite_t) {
					.animation = cg_sprite_blood_01,
					.lifetime = Cg_AnimationLifetime(cg_sprite_blood_01, 30) + Randomf() * 500,
					.origin = o,
					.velocity = Vec3_Add(Vec3_Add(Vec3_Scale(v, dist * ((float)j / GIB_STREAM_COUNT)), Vec3_RandomRange(-2.f, 2.f)), Vec3(0.f, 0.f, 100.f)),
					.acceleration.z = -SPRITE_GRAVITY * 2.0,
					.size = RandomRangef(24.f, 56.f),
					.color = Vec4(0.f, 1.f, .5f, .97f)
				})) {
				break;
			}
		}
	}

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = count * 6.0,
		.color = Color4bv(0x88111188),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_gib,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
void Cg_SparksEffect(const vec3_t org, const vec3_t dir, int32_t count) {

	for (int32_t i = 0; i < count; i++) {
		const float hue = color_hue_yellow + RandomRangef(-20.f, 20.f);
		const float sat = RandomRangef(.7f, 1.f);

		if (!Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_spark,
				.origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f)),
				.velocity = Vec3_Add(Vec3_Scale(dir, 4.f), Vec3_RandomRange(-90.f, 90.f)),
				.acceleration = Vec3_Add(Vec3_RandomRange(-1.f, 1.f), Vec3(0.f, 0.f, -0.5 * SPRITE_GRAVITY)),
				.lifetime = 500 + Randomf() * 250,
				.size = RandomRangef(.4f, .8f),
				.bounce = .6f,
				.color = Vec4(hue, sat, RandomRangef(.7f, 1.f), RandomRangef(.56f, 1.f)),
				.end_color = Vec4(hue, sat, 0.f, 0.f)
			})) {
			break;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 80.0,
		.color = Vec3(0.7, 0.5, 0.5),
		.decay = 650
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_sparks,
		.origin = org,
		.attenuation = ATTEN_STATIC,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_ExplosionEffect(const vec3_t org, const vec3_t dir) {
	// TODO: Bubbles in water?

	// ember sparks
	if ((cgi.PointContents(org) & CONTENTS_MASK_LIQUID) == 0) {
		for (int32_t i = 0; i < 100; i++) {
			const uint32_t lifetime = 3000 + Randomf() * 300;
			const float size = 2.f + Randomf() * 2.f;
			const float hue = RandomRangef(10.f, 50.f);

			if (!Cg_AddSprite(&(cg_sprite_t) {
					.atlas_image = cg_sprite_particle2,
					.origin = Vec3_Add(org, Vec3_RandomRange(-10.f, 10.f)),
					.velocity = Vec3_RandomRange(-300.f, 300.f),
					.acceleration.z = -SPRITE_GRAVITY * 2.f,
					.lifetime = lifetime,
					.size = size,
					.size_velocity = -size / MILLIS_TO_SECONDS(lifetime),
					.bounce = .4f,
					.color = Vec4(hue, 1.f, 1.f, 8.f),
					.end_color = Vec4(hue, 1.f, -.7f, 0.f)
				})) {
				break;
			}
		}
	}

	// billboard explosion 16
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.animation = cg_sprite_explosion,
		.lifetime = Cg_AnimationLifetime(cg_sprite_explosion, 40),
		.size = 100.f,
		.size_velocity = 25.f,
		.rotation = Randomf() * 2.f * M_PI,
		.flags = SPRITE_LERP,
		.color = Vec4(0.f, 0.f, 1.f, .5f)
	});

	// billboard explosion 2
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.animation = cg_sprite_explosion,
		.lifetime = Cg_AnimationLifetime(cg_sprite_explosion, 30),
		.size = 175.f,
		.size_velocity = 25.f,
		.rotation = Randomf() * 2.f * M_PI,
		.flags = SPRITE_LERP,
		.color = Vec4(0.f, 0.f, 1.f, .5f)
	});

	// decal explosion
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.animation = cg_sprite_explosion,
		.lifetime = Cg_AnimationLifetime(cg_sprite_explosion, 30),
		.size = 175.f,
		.size_velocity = 25.f,
		.rotation = Randomf() * 2.f * M_PI,
		.flags = SPRITE_LERP,
		.color = Vec4(0.f, 0.f, 1.f, .5f),
		.dir = dir
	});

	// decal blast ring
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.animation = cg_sprite_explosion_ring_02,
		.lifetime = Cg_AnimationLifetime(cg_sprite_explosion_ring_02, 20),
		.size = 65.f,
		.size_velocity = 500.f,
		.size_acceleration = -500.f,
		.rotation = Randomf() * 2.f * M_PI,
		.flags = SPRITE_LERP,
		.color = Vec4(0.f, 0.f, 1.f, 0.f),
		.dir = dir
	});

	// blast glow
	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.lifetime = 325,
		.size = 200.f,
		.rotation = Randomf() * 2.f * M_PI,
		.atlas_image = cg_sprite_explosion_glow,
		.color = Vec4(0.f, 0.f, 1.f, 1.f)
	});

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 150.0,
		.color = Vec3(0.8, 0.4, 0.2),
		.decay = 825
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = RandomRangef(32.f, 48.f),
		.color = Color4bv(0xaa202020),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_explosion,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_HyperblasterEffect(const vec3_t org, const vec3_t dir) {
	// impact "splash"
	for (uint32_t i = 0; i < 6; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.origin = org,
			.animation = cg_sprite_electro_01,
			.lifetime = Cg_AnimationLifetime(cg_sprite_electro_01, 20),
			.size = 50.f,
			.size_velocity = 400.f,
			.rotation = Randomf() * 2.f * M_PI,
			.flags = SPRITE_LERP,
			.dir = Vec3_RandomRange(-1.f, 1.f),
			.color = Vec4(204.f, .55f, .9f, 0.f),
			.end_color = Vec4(204.f, .55f, 0.f, 0.f)
		});
	}

	Cg_AddSprite(&(cg_sprite_t) {
		.origin = org,
		.animation = cg_sprite_electro_01,
		.lifetime = Cg_AnimationLifetime(cg_sprite_electro_01, 8),
		.size = 100.f,
		.size_velocity = 25.f,
		.rotation = Randomf() * 2.f * M_PI,
		.flags = SPRITE_LERP,
		.dir = dir,
		.color = Vec4(204.f, .55f, .9f, 0.f),
		.end_color = Vec4(204.f, .55f, 0.f, 0.f)
	});

	// impact flash
	for (uint32_t i = 0; i < 2; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.origin = org,
			.atlas_image = cg_sprite_flash,
			.lifetime = 150,
			.size = RandomRangef(75.f, 100.f),
			.rotation = RandomRadian(),
			.rotation_velocity = i == 0 ? .66f : -.66f,
			.color = Vec4(204.f, .55f, .9f, 0.f),
			.end_color = Vec4(204.f, .55f, 0.f, 0.f)
		});
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 80.f,
		.color = Vec3(0.4, 0.7, 1.0),
		.decay = 250,
		.intensity = 0.05
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 16.f,
		.color = Color4f(.4f, .7f, 1.f, .33f),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_hyperblaster_hit,
		.origin = Vec3_Add(org, dir),
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
static void Cg_LightningDischargeEffect(const vec3_t org) {

	for (int32_t i = 0; i < 40; i++) {
		Cg_BubbleTrail(NULL, org, Vec3_Add(org, Vec3_RandomRange(-48.f, 48.f)), 4.0);
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 160.f,
		.color = Vec3(.6f, .6f, 1.f),
		.decay = 750
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_lightning_discharge,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief  
 */
static float Cg_EaseInExpo(float life) {
	return life == 0 ? 0 : powf(2, 10 * life - 10);
}

/**
 * @brief
 */
static void Cg_RailEffect(const vec3_t start, const vec3_t end, const vec3_t dir, int32_t flags, const float hue) {
	vec3_t forward;
	vec4_t color = Vec4(hue, 1.f, 1.f, 1.f);
	color_t color_rgb = ColorHSVA(color.x, color.y, color.z, color.w);

	Cg_AddLight(&(cg_light_t) {
		.origin = start,
		.radius = 100.f,
		.color = Color_Vec3(color_rgb),
		.decay = 500,
	});

	// Check for bubble trail
	Cg_BubbleTrail(NULL, start, end, 16.f);

	const float dist = Vec3_DistanceDir(end, start, &forward);
	const vec3_t right = Vec3(forward.z, -forward.x, forward.y);
	const vec3_t up = Vec3_Cross(forward, right);

	for (int32_t i = 0; i < dist; i++) {
		const float cosi = cosf(i * 0.1f);
		const float sini = sinf(i * 0.1f);
		const float frac = (1.0 - (i / dist));
		const uint32_t lifetime = RandomRangeu(1500, 1550);
		const float sat = RandomRangef(.8f, 1.f);

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.origin = Vec3_Add(Vec3_Add(Vec3_Add(start, Vec3_Scale(forward, i)), Vec3_Scale(right, cosi)), Vec3_Scale(up, sini)),
			.velocity = Vec3_Add(Vec3_Add(Vec3_Scale(forward, 20.f), Vec3_Scale(right, cosi * frac)), Vec3_Scale(up, sini * frac)),
			.acceleration = Vec3_Add(Vec3_Add(Vec3_Scale(right, cosi * 8.f), Vec3_Scale(up, sini * 8.f)), Vec3(0.f, 0.f, 1.f)),
			.lifetime = lifetime,
			.size = frac * 8.f,
			.size_velocity = 1.f / MILLIS_TO_SECONDS(lifetime),
			.life_easing = Cg_EaseInExpo,
			.color = Vec4(hue, sat, RandomRangef(.8f, 1.f), 0.f),
			.end_color = Vec4(hue, sat, 0.f, 0.f)
		});
	}

	Cg_AddSprite(&(cg_sprite_t) {
		.type = SPRITE_BEAM,
		.image = cg_beam_rail,
		.origin = start,
		.termination = end,
		.size = 20.f,
		.velocity = Vec3_Scale(forward, 20.f),
		.lifetime = RandomRangeu(1500, 1550),
		.color = Vec4(0.f, 0.f, 1.f, 0.f)
	});

	// Check for explosion effect on solids

	if (flags & SURF_SKY) {
		return;
	}

	// Rail impact cloud
	// TODO: use a different sprite
	for (int32_t i = 0; i < 2; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.origin = Vec3_Add(end, Vec3_Scale(dir, 8.f)),
			.animation = cg_sprite_poof_01,
			.lifetime = Cg_AnimationLifetime(cg_sprite_poof_01, 30),
			.size = 128.f,
			.size_velocity = 20.f,
			.dir = (i == 1) ? dir : Vec3_Zero(),
			.color = Vec4(0.f, 0.f, 1.f, .25f)
		});
	}

	// TODO: what DIS?
	if (cg_particle_quality->integer && (cgi.PointContents(end) & CONTENTS_MASK_LIQUID) == 0) {

		for (int32_t i = 0; i < 16; i++) {
			const vec3_t velocity = Vec3_Scale(Vec3_Add(Vec3_Scale(dir, .5f), Vec3_RandomDir()), 100.f);

			Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_particle,
				.origin = end,
				.velocity = velocity,
				.acceleration = Vec3_Scale(velocity, -1.f),
				.lifetime = 1000,
				.color = color,
				.size = 8.f,
				.size_velocity = -8.f
			});
		}
	}

	// Impact light
	Cg_AddLight(&(cg_light_t) {
		.origin = Vec3_Add(end, Vec3_Scale(dir, 20.f)),
		.radius = 120.f,
		.color = Color_Vec3(color_rgb),
		.decay = 350.f,
		.intensity = .1f
	});

	cgi.AddStain(&(const r_stain_t) {
		.origin = end,
		.radius = 8.0,
		.color = color_rgb,
	});
}

/**
 * @brief
 */
static void Cg_BfgLaserEffect(const vec3_t org, const vec3_t end) {

	// FIXME: beam
	Cg_AddSprite(&(cg_sprite_t) {
		.atlas_image = cg_sprite_particle,
		.origin = org,
		.lifetime = 50,
		.size = 48.f,
		.color = Vec4(color_hue_green, .0f, 1.f, 1.f),
		.end_color = Vec4(color_hue_green, .0f, 1.f, 1.f)
	});

	Cg_AddLight(&(cg_light_t) {
		.origin = end,
		.radius = 80.0,
		.color = Vec3(0.8, 1.0, 0.5),
		.decay = 50,
	});
}

/**
 * @brief
 */
static void Cg_BfgEffect(const vec3_t org) {
	// explosion 1
	for (int32_t i = 0; i < 4; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.animation = cg_sprite_bfg_explosion_2,
			.lifetime = Cg_AnimationLifetime(cg_sprite_bfg_explosion_2, 15),
			.size = RandomRangef(200.f, 300.f),
			.size_velocity = 100.f,
			.size_acceleration = -10.f,
			.rotation = RandomRangef(0.f, 2.f * M_PI),
			.origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 50.f)),
			.flags = SPRITE_LERP,
			.color = Vec4(0.f, 0.f, 1.f, .15f)
		});
	}

	// explosion 2
	for (int32_t i = 0; i < 4; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.animation = cg_sprite_bfg_explosion_3,
			.lifetime = Cg_AnimationLifetime(cg_sprite_bfg_explosion_3, 15),
			.size = RandomRangef(200.f, 300.f),
			.size_velocity = 100.f,
			.size_acceleration = -10.f,
			.rotation = RandomRangef(0.f, 2.f * M_PI),
			.origin = Vec3_Add(org, Vec3_Scale(Vec3_RandomDir(), 50.f)),
			.flags = SPRITE_LERP,
			.color = Vec4(0.f, 0.f, 1.f, .15f)
		});
	}

	// particles 1
	for (int32_t i = 0; i < 50; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.lifetime = 3000,
			.size = 8.f,
			.size_velocity = -8.f / MILLIS_TO_SECONDS(3000),
			.bounce = .5f,
			.velocity = Vec3_Scale(Vec3_RandomDir(), 400.f),
			.acceleration = Vec3(0.f, 0.f, -3.f * SPRITE_GRAVITY),
			.origin = org,
			.color = Vec4(0.f, 0.f, 1.f, 1.f)
		});
	}

	// particles 2
	for (int32_t i = 0; i < 20; i++) {
		const float sat = RandomRangef(.5f, 1.f);

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_particle,
			.lifetime = 10000,
			.size = 4.f,
			.size_velocity = -4.f / MILLIS_TO_SECONDS(10000),
			.bounce = .5f,
			.velocity = Vec3_Scale(Vec3_RandomDir(), 300.f),
			.origin = org,
			.color = Vec4(color_hue_green, RandomRangef(.5f, 1.f), .1f, 1.f),
			.end_color = Vec4(color_hue_green, sat, 0.f, 0.f)
		});
	}

	// impact flash 1
	for (uint32_t i = 0; i < 4; i++) {

		Cg_AddSprite(&(cg_sprite_t) {
			.atlas_image = cg_sprite_flash,
			.origin = org,
			.lifetime = 600,
			.size = RandomRangef(300, 400),
			.rotation = RandomRadian(),
			.dir = Vec3_Random(),
			.color = Vec4(120.f, .87f, .80f, .0f),
			.end_color = Vec4(120.f, .87f, 0.f, 0.f)
		});
	}

	// impact flash 2
	Cg_AddSprite(&(cg_sprite_t) {
		.atlas_image = cg_sprite_flash,
		.origin = org,
		.lifetime = 600,
		.size = 400.f,
		.rotation = RandomRadian(),
		.color = Vec4(120.f, .87f, .80f, .0f),
		.end_color = Vec4(120.f, .87f, .0f, .0f)
	});

	// glow
	Cg_AddSprite(&(cg_sprite_t) {
		.atlas_image = cg_sprite_particle,
		.origin = org,
		.lifetime = 1000,
		.size = 600.f,
		.rotation = RandomRadian(),
		.color = Vec4(120.f, .87f, .80f, .0f),
		.end_color = Vec4(120.f, .87f, 0.f, 0.f)
	});

	Cg_AddLight(&(const cg_light_t) {
		.origin = org,
		.radius = 200.f,
		.color = Vec3(.8f, 1.f, .5f),
		.intensity = .1f,
		.decay = 1500
	});
	
	cgi.AddStain(&(const r_stain_t) {
		.origin = org,
		.radius = 45.f,
		.color = Color4f(.8f, 1.f, .5f, .5f),
	});

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_bfg_hit,
		.origin = org,
		.attenuation = ATTEN_NORM,
		.flags = S_PLAY_POSITIONED
	});
}

/**
 * @brief
 */
void Cg_RippleEffect(const vec3_t org, const float size, const uint8_t viscosity) {

	Cg_AddSprite(&(cg_sprite_t) {
		.animation = cg_sprite_poof_01,
		.lifetime = Cg_AnimationLifetime(cg_sprite_poof_01, 17.5f) * (viscosity * .1f),
		.origin = org,
		.size = size * 8.f,
		.dir = Vec3_Up(),
		.flags = SPRITE_LERP
	});
}

/**
 * @brief
 */
static void Cg_SplashEffect(const vec3_t org, const vec3_t dir) {

	for (int32_t i = 0; i < 10; i++) {
		if (!Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_particle,
				.origin = Vec3_Add(org, Vec3_RandomRange(-8.f, 8.f)),
				.velocity = Vec3_Add(Vec3_RandomRange(-64.f, 64.f), Vec3(0.f, 0.f, 36.f)),
				.acceleration = Vec3(0.f, 0.f, -SPRITE_GRAVITY / 2.f),
				.lifetime = RandomRangeu(100, 400),
				.color = Vec4(0.f, 0.f, 1.f, 1.f),
				.size = 6.4f 
			})) {
			break;
		}
	}

	Cg_AddSprite(&(cg_sprite_t) {
		.atlas_image = cg_sprite_particle,
		.origin = org,
		.velocity = Vec3_Add(Vec3_Scale(dir, 70.f + Randomf() * 30.f), Vec3_RandomRange(-8.f, 8.f)),
		.acceleration = Vec3(0.f, 0.f, -SPRITE_GRAVITY / 2.f),
		.lifetime = 120 + Randomf() * 80,
		.color = Vec4(180.f, .42f, .87f, .87f),
		.end_color = Vec4(180.f, .42f, 0.f, 0.f),
		.size = 11.2f + Randomf() * 5.6f
	});
}

/**
 * @brief
 */
static void Cg_HookImpactEffect(const vec3_t org, const vec3_t dir) {

	for (int32_t i = 0; i < 32; i++) {

		if (!Cg_AddSprite(&(cg_sprite_t) {
				.atlas_image = cg_sprite_particle,
				.origin = Vec3_Add(org, Vec3_RandomRange(-4.f, 4.f)),
				.velocity = Vec3_Add(Vec3_Scale(dir, 9.f), Vec3_RandomRange(-90.f, 90.f)),
				.acceleration = Vec3_Add(Vec3_RandomRange(-2.f, 2.f), Vec3(0.f, 0.f, -0.5 * SPRITE_GRAVITY)),
				.lifetime = 100 + (Randomf() * 150),
				.color = Vec4(53.f, .83f, .97f, RandomRangef(.8f, 1.f)),
				.end_color = Vec4(53.f, .83f, 0.f, 0.f),
				.size = 6.4f + Randomf() * 3.2f
			})) {
			break;
		}
	}

	Cg_AddLight(&(const cg_light_t) {
		.origin = Vec3_Add(org, dir),
		.radius = 80.0,
		.color = Vec3(0.7, 0.5, 0.5),
		.decay = 850
	});
}

/**
 * @brief
 */
void Cg_ParseTempEntity(void) {
	vec3_t pos, pos2, dir;
	int32_t i, j;

	const uint8_t type = cgi.ReadByte();

	switch (type) {

		case TE_BLASTER:
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			i = cgi.ReadByte();
			Cg_BlasterEffect(pos, dir, Cg_ResolveEffectHue(i ? i - 1 : 0, color_hue_orange));
			break;

		case TE_TRACER:
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			Cg_TracerEffect(pos, pos2);
			break;

		case TE_BULLET: // bullet hitting wall
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_BulletEffect(pos, dir);
			break;

		case TE_BLOOD: // projectile hitting flesh
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_BloodEffect(pos, dir, 12);
			break;

		case TE_GIB: // player over-death
			pos = cgi.ReadPosition();
			Cg_GibEffect(pos, 12);
			break;

		case TE_SPARKS: // colored sparks
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_SparksEffect(pos, dir, 12);
			break;

		case TE_HYPERBLASTER: // hyperblaster hitting wall
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_HyperblasterEffect(pos, dir);
			break;

		case TE_LIGHTNING_DISCHARGE: // lightning discharge in water
			pos = cgi.ReadPosition();
			Cg_LightningDischargeEffect(pos);
			break;

		case TE_RAIL: // railgun effect
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			dir = cgi.ReadDir();
			i = cgi.ReadLong();
			j = cgi.ReadByte();
			Cg_RailEffect(pos, pos2, dir, i, Cg_ResolveEffectHue(j ? j - 1 : 0, color_hue_cyan));
			break;

		case TE_EXPLOSION: // rocket and grenade explosions
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_ExplosionEffect(pos, dir);
			break;

		case TE_BFG_LASER:
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			Cg_BfgLaserEffect(pos, pos2);
			break;

		case TE_BFG: // bfg explosion
			pos = cgi.ReadPosition();
			Cg_BfgEffect(pos);
			break;

		case TE_BUBBLES: // bubbles chasing projectiles in water
			pos = cgi.ReadPosition();
			pos2 = cgi.ReadPosition();
			Cg_BubbleTrail(NULL, pos, pos2, 1.0);
			break;

		case TE_RIPPLE: // liquid surface ripples
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			i = cgi.ReadByte();
			j = cgi.ReadByte();
			Cg_RippleEffect(pos, i, j);
			if (cgi.ReadByte()) {
				Cg_SplashEffect(pos, dir);
			}
			break;

		case TE_HOOK_IMPACT: // grapple hook impact
			pos = cgi.ReadPosition();
			dir = cgi.ReadDir();
			Cg_HookImpactEffect(pos, dir);
			break;

		default:
			cgi.Warn("Unknown type: %d\n", type);
			return;
	}
}
