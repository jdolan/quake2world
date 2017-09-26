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
#include "collision/cmodel.h"

s_sample_t *cg_sample_blaster_fire;
s_sample_t *cg_sample_blaster_hit;
s_sample_t *cg_sample_shotgun_fire;
s_sample_t *cg_sample_supershotgun_fire;
s_sample_t *cg_sample_machinegun_fire[4];
s_sample_t *cg_sample_machinegun_hit[3];
s_sample_t *cg_sample_grenadelauncher_fire;
s_sample_t *cg_sample_rocketlauncher_fire;
s_sample_t *cg_sample_hyperblaster_fire;
s_sample_t *cg_sample_hyperblaster_hit;
s_sample_t *cg_sample_lightning_fire;
s_sample_t *cg_sample_lightning_discharge;
s_sample_t *cg_sample_railgun_fire;
s_sample_t *cg_sample_bfg_fire;
s_sample_t *cg_sample_bfg_hit;

s_sample_t *cg_sample_explosion;
s_sample_t *cg_sample_teleport;
s_sample_t *cg_sample_respawn;
s_sample_t *cg_sample_sparks;
s_sample_t *cg_sample_rain;
s_sample_t *cg_sample_snow;
s_sample_t *cg_sample_underwater;
s_sample_t *cg_sample_hits[2];
s_sample_t *cg_sample_gib;

cg_particles_t *cg_particles_normal;
cg_particles_t *cg_particles_explosion;
cg_particles_t *cg_particles_debris[4];
cg_particles_t *cg_particles_teleporter;
cg_particles_t *cg_particles_smoke;
cg_particles_t *cg_particles_steam;
cg_particles_t *cg_particles_bubble;
cg_particles_t *cg_particles_rain;
cg_particles_t *cg_particles_snow;
cg_particles_t *cg_particles_beam;
cg_particles_t *cg_particles_rail_wake;
cg_particles_t *cg_particles_tracer;
cg_particles_t *cg_particles_blood;
cg_particles_t *cg_particles_lightning;
cg_particles_t *cg_particles_rope;
cg_particles_t *cg_particles_flame;
cg_particles_t *cg_particles_spark;
cg_particles_t *cg_particles_inactive;
cg_particles_t *cg_particles_ripple[3];

cg_particles_t *cg_particles_stain_burn;
cg_particles_t *cg_particles_lightning_burn;
cg_particles_t *cg_particles_blood_burn;

static GHashTable *cg_footstep_table;

/**
 * @brief Free callback for footstep table
 */
static void Cg_FootstepsTable_Destroy(gpointer value) {
	g_array_free((GArray *) value, true);
}

/**
 * @brief
 */
static Fs_EnumerateResult Cg_FootstepsTable_EnumerateFiles(const char *file, void *data) {

	(*((size_t *) data))++;

	return FS_ENUM_CONTINUE;
}

/**
 * @brief
 */
static void Cg_FootstepsTable_Load(const char *footsteps) {
	GArray *sounds = (GArray *) g_hash_table_lookup(cg_footstep_table, footsteps);

	if (sounds) { // already loaded
		return;
	}

	size_t count = 0;
	cgi.EnumerateFiles(va("players/common/step_%s_*", footsteps), Cg_FootstepsTable_EnumerateFiles, &count);

	if (!count) {
		cgi.Warn("Map has footsteps material %s but no sound files exist\n", footsteps);
		return;
	}

	sounds = g_array_new(false, false, sizeof(s_sample_t *));

	for (uint32_t i = 0; i < count; i++) {

		char name[MAX_QPATH];
		g_snprintf(name, sizeof(name), "#players/common/step_%s_%" PRIu32, footsteps, i + 1);

		s_sample_t *sample = cgi.LoadSample(name);
		sounds = g_array_append_val(sounds, sample);
	}

	g_hash_table_insert(cg_footstep_table, (gpointer) footsteps, sounds);
}

/**
 * @brief Return a sample for the specified footstep type.
 */
s_sample_t *Cg_GetFootstepSample(const char *footsteps) {

	if (!footsteps || !*footsteps) {
		footsteps = "default";
	}

	GArray *sounds = (GArray *) g_hash_table_lookup(cg_footstep_table, footsteps);

	if (!sounds) { // bad mapper!
		cgi.Warn("Footstep sound %s not valid\n", footsteps);
		return Cg_GetFootstepSample("default"); // this should never recurse
	}

	static int32_t last_index = -1;
	int32_t index = Randomr(0, sounds->len);

	if (last_index == index) {
		index ^= 1;
	}

	last_index = index;

	return g_array_index(sounds, s_sample_t *, index);
}

/**
 * @brief Loads all of the footstep sounds for this map.
 */
static void Cg_InitFootsteps(void) {

	if (cg_footstep_table) {
		g_hash_table_destroy(cg_footstep_table);
	}

	cg_footstep_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Cg_FootstepsTable_Destroy);

	// load the hardcoded default set
	GArray *default_samples = g_array_new(false, false, sizeof(s_sample_t *));

	default_samples = g_array_append_vals(default_samples, (const s_sample_t *[]) {
		cgi.LoadSample("#players/common/step_1"),
		cgi.LoadSample("#players/common/step_2"),
		cgi.LoadSample("#players/common/step_3"),
		cgi.LoadSample("#players/common/step_4")
	}, 4);

	g_hash_table_insert(cg_footstep_table, "default", default_samples);

	r_material_t **material = cgi.WorldModel()->materials;
	for (size_t i = 0; i < cgi.WorldModel()->num_materials; i++, material++) {

		if (strlen((*material)->cm->footsteps)) {
			Cg_FootstepsTable_Load((*material)->cm->footsteps);
		}
	}
}

/**
 * @brief Updates all media references for the client game.
 */
void Cg_UpdateMedia(void) {
	char name[MAX_QPATH];

	Cg_FreeParticles();

	cgi.FreeTag(MEM_TAG_CGAME);
	cgi.FreeTag(MEM_TAG_CGAME_LEVEL);

	cg_sample_blaster_fire = cgi.LoadSample("weapons/blaster/fire");
	cg_sample_blaster_hit = cgi.LoadSample("weapons/blaster/hit");
	cg_sample_shotgun_fire = cgi.LoadSample("weapons/shotgun/fire");
	cg_sample_supershotgun_fire = cgi.LoadSample("weapons/supershotgun/fire");
	cg_sample_grenadelauncher_fire = cgi.LoadSample("weapons/grenadelauncher/fire");
	cg_sample_rocketlauncher_fire = cgi.LoadSample("weapons/rocketlauncher/fire");
	cg_sample_hyperblaster_fire = cgi.LoadSample("weapons/hyperblaster/fire");
	cg_sample_hyperblaster_hit = cgi.LoadSample("weapons/hyperblaster/hit");
	cg_sample_lightning_fire = cgi.LoadSample("weapons/lightning/fire");
	cg_sample_lightning_discharge = cgi.LoadSample("weapons/lightning/discharge");
	cg_sample_railgun_fire = cgi.LoadSample("weapons/railgun/fire");
	cg_sample_bfg_fire = cgi.LoadSample("weapons/bfg/fire");
	cg_sample_bfg_hit = cgi.LoadSample("weapons/bfg/hit");

	cg_sample_explosion = cgi.LoadSample("weapons/common/explosion");
	cg_sample_teleport = cgi.LoadSample("world/teleport");
	cg_sample_respawn = cgi.LoadSample("world/respawn");
	cg_sample_sparks = cgi.LoadSample("world/sparks");
	cg_sample_rain = cgi.LoadSample("world/rain");
	cg_sample_snow = cgi.LoadSample("world/snow");
	cg_sample_underwater = cgi.LoadSample("world/underwater");
	cg_sample_gib = cgi.LoadSample("gibs/common/gib");

	for (uint32_t i = 0; i < lengthof(cg_sample_hits); i++) {
		g_snprintf(name, sizeof(name), "misc/hit_%" PRIu32, i + 1);
		cg_sample_hits[i] = cgi.LoadSample(name);
	}

	for (uint32_t i = 0; i < lengthof(cg_sample_machinegun_fire); i++) {
		g_snprintf(name, sizeof(name), "weapons/machinegun/fire_%" PRIu32, i + 1);
		cg_sample_machinegun_fire[i] = cgi.LoadSample(name);
	}

	for (uint32_t i = 0; i < lengthof(cg_sample_machinegun_hit); i++) {
		g_snprintf(name, sizeof(name), "weapons/machinegun/hit_%" PRIu32, i + 1);
		cg_sample_machinegun_hit[i] = cgi.LoadSample(name);
	}

	Cg_InitFootsteps();

	Cg_InitParticles();

	cg_particles_normal = Cg_AllocParticles(cgi.LoadImage("particles/particle.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_explosion = Cg_AllocParticles(cgi.LoadImage("particles/explosion.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);

	for (uint32_t i = 0; i < lengthof(cg_particles_debris); i++) {
		g_snprintf(name, sizeof(name), "particles/debris_%" PRIu32, i);
		cg_particles_debris[i] = Cg_AllocParticles(cgi.LoadImage(name, IT_EFFECT | IT_MASK_MULTIPLY), true);
	}

	cg_particles_teleporter = Cg_AllocParticles(cgi.LoadImage("particles/teleport.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_smoke = Cg_AllocParticles(cgi.LoadImage("particles/smoke.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_steam = Cg_AllocParticles(cgi.LoadImage("particles/steam.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_bubble = Cg_AllocParticles(cgi.LoadImage("particles/bubble.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_rain = Cg_AllocParticles(cgi.LoadImage("particles/rain.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_snow = Cg_AllocParticles(cgi.LoadImage("particles/snow.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_beam = Cg_AllocParticles(cgi.LoadImage("particles/beam.tga", IT_EFFECT | IT_MASK_MULTIPLY), false);
	cg_particles_rail_wake = Cg_AllocParticles(cgi.LoadImage("particles/rail_wake.tga", IT_EFFECT | IT_MASK_MULTIPLY), false);
	cg_particles_tracer = Cg_AllocParticles(cgi.LoadImage("particles/tracer.tga", IT_EFFECT | IT_MASK_MULTIPLY), false);
	cg_particles_blood = Cg_AllocParticles(cgi.LoadImage("particles/blood.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_lightning = Cg_AllocParticles(cgi.LoadImage("particles/lightning.tga", IT_EFFECT | IT_MASK_MULTIPLY), false);
	cg_particles_rope = Cg_AllocParticles(cgi.LoadImage("particles/rope.tga", IT_EFFECT), false);
	cg_particles_flame = Cg_AllocParticles(cgi.LoadImage("particles/flame.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_spark = Cg_AllocParticles(cgi.LoadImage("particles/spark.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);
	cg_particles_inactive = Cg_AllocParticles(cgi.LoadImage("particles/inactive.tga", IT_EFFECT | IT_MASK_MULTIPLY), true);

	for (uint32_t i = 0; i < lengthof(cg_particles_ripple); i++) {
		g_snprintf(name, sizeof(name), "particles/ripple_%" PRIu32, i);
		cg_particles_ripple[i] = Cg_AllocParticles(cgi.LoadImage(name, IT_EFFECT | IT_MASK_MULTIPLY), true);
	}

	cg_particles_stain_burn = Cg_AllocParticles(cgi.LoadImage("particles/stain_burn.tga", IT_EFFECT), true);
	cg_particles_lightning_burn = Cg_AllocParticles(cgi.LoadImage("particles/lightning_burn.tga", IT_EFFECT), true);
	cg_particles_blood_burn = Cg_AllocParticles(cgi.LoadImage("particles/stain_blood.tga", IT_EFFECT), true);

	Cg_SetupParticleAtlas();

	cg_draw_crosshair->modified = true;
	cg_draw_crosshair_color->modified = true;

	Cg_LoadEmits();

	Cg_LoadEffects();

	Cg_LoadClients();

	Cg_LoadHudMedia();

	cgi.Debug("Complete\n");
}
