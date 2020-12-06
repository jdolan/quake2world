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

#include "s_local.h"
#include "client.h"

extern cl_client_t cl;

/**
 * @brief
 */
static int32_t S_AllocChannel(void) {

	for (int32_t i = 0; i < MAX_CHANNELS; i++) {
		if (!s_env.channels[i].sample) {
			return i;
		}
	}

	Com_Debug(DEBUG_SOUND, "Failed\n");
	return -1;
}

/**
 * @brief
 */
void S_FreeChannel(int32_t c) {

	alSourceStop(s_env.sources[c]);
	alSourcei(s_env.sources[c], AL_BUFFER, 0);
	memset(&s_env.channels[c], 0, sizeof(s_env.channels[c]));
}

#define SOUND_MAX_DISTANCE 2048.0

/**
 * @brief Set distance and stereo panning for the specified channel.
 */
static _Bool S_SpatializeChannel(s_channel_t *ch) {
	vec3_t org, delta, center;

	org = r_view.origin;

	if (ch->play.flags & S_PLAY_POSITIONED) {
		org = ch->play.origin;
		ch->velocity = Vec3_Zero();
	} else if (ch->play.flags & S_PLAY_ENTITY) {
		const cl_entity_t *ent = &cl.entities[ch->play.entity];

		if (ent == cl.entity) {
			ch->relative = true;
		}

		if (s_doppler->value) {
			if (!ch->relative) {
				ch->velocity = Vec3_Subtract(ent->current.origin, ent->prev.origin);
			}
		}

		if (ent->current.solid == SOLID_BSP) {
			const r_model_t *mod = cl.model_precache[ent->current.model1];
			center = Vec3_Mix(mod->mins, mod->maxs, 0.5);
			org = Vec3_Add(center, ent->origin);
		} else {
			if (!ch->relative) {
				org = ent->origin;
			}
		}
	}

	ch->position = org;

	const float dist = Vec3_DistanceDir(org, r_view.origin, &delta);

	float atten;
	switch (ch->play.atten) {
		case SOUND_ATTEN_NONE:
			atten = 0.f;
		case SOUND_ATTEN_LINEAR:
			atten = 1.f;
			break;
		case SOUND_ATTEN_SQUARE:
			atten = 2.f;
			break;
		case SOUND_ATTEN_CUBIC:
			atten = 4.f;
			break;
	}

	const float frac = dist * atten / SOUND_MAX_DISTANCE;

	ch->gain = 1.0 - frac;

	// fade out frame sounds if they are no longer in the frame
	if (ch->start_time) {
		if (ch->play.flags & S_PLAY_FRAME) {
			if (ch->frame != cl.frame.frame_num) {
				const uint32_t delta = (cl.frame.frame_num - ch->frame) * QUETOO_TICK_MILLIS;

				if (delta > 250) {
					return false; // x faded out
				}

				ch->gain *= 1.0 - (delta / 250.0);
			}
		}
	}

	ch->pitch = 1.0;
	ch->filter = AL_NONE;

	// adjust pitch in liquids
	if (r_view.contents & CONTENTS_MASK_LIQUID) {
		ch->pitch = 0.5;
	}

	// offset pitch by sound-requested offset
	if (ch->play.pitch) {
		const float octaves = (float) pow(2, 0.69314718 * ((float) ch->play.pitch / TONES_PER_OCTAVE));
		ch->pitch *= octaves;
	}

	// apply sound effects
	if (s_env.effects.loaded) {

		if (Cm_PointContents(ch->position, 0) & CONTENTS_MASK_LIQUID) {
			ch->filter = s_env.effects.underwater;
		} else {
			const cm_trace_t tr = Cl_Trace(r_view.origin, ch->position, Vec3_Zero(), Vec3_Zero(), ch->play.entity, CONTENTS_MASK_CLIP_PROJECTILE);
			if (tr.fraction < 1.0) {
				ch->filter = s_env.effects.occluded;
			}
		}
	}

	return frac <= 1.0;
}

/**
 * @brief Updates all active channels for the current frame.
 */
void S_MixChannels(void) {

	if (s_effects->modified) {
		S_Restart_f();

		s_effects->modified = false;
		return;
	}

	if (s_doppler->modified) {
		alDopplerFactor(0.05 * s_doppler->value);
	}

	alListenerfv(AL_POSITION, r_view.origin.xyz);
	S_CheckALError();

	alListenerfv(AL_ORIENTATION, (float []) {
		r_view.forward.x, r_view.forward.y, r_view.forward.z,
		r_view.up.x, r_view.up.y, r_view.up.z
	});
	S_CheckALError();

	if (s_doppler->value) {
		alListenerfv(AL_VELOCITY, cl.frame.ps.pm_state.velocity.xyz);
	} else {
		alListenerfv(AL_VELOCITY, Vec3_Zero().xyz);
	}

	s_env.num_active_channels = 0;

	s_channel_t *ch = s_env.channels;
	for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++) {

		if (ch->sample) {

			const ALuint src = s_env.sources[i];
			assert(src);

			if (S_SpatializeChannel(ch)) {

				if (ch->relative) {
					alSourcefv(src, AL_POSITION, Vec3_Zero().xyz);
					S_CheckALError();
				} else {
					alSourcefv(src, AL_POSITION, ch->position.xyz);
					S_CheckALError();

					if (s_doppler->value) {
						alSourcefv(src, AL_VELOCITY, ch->velocity.xyz);
					} else {
						alSourcefv(src, AL_VELOCITY, Vec3_Zero().xyz);
					}
				}

				float volume;

				if (ch->play.flags & S_PLAY_AMBIENT) {
					volume = s_volume->value * s_ambient_volume->value;
				} else {
					volume = s_volume->value * s_effects_volume->value;
				}

				alSourcef(src, AL_GAIN, ch->gain * volume);
				S_CheckALError();

				alSourcef(src, AL_PITCH, ch->pitch);
				S_CheckALError();

				if (s_env.effects.loaded) {
					alSourcei(src, AL_DIRECT_FILTER, ch->filter);
					S_CheckALError();
				}

				if (!ch->start_time) {
					ch->start_time = quetoo.ticks;

					alSourcei(src, AL_SOURCE_RELATIVE, ch->relative ? 1 : 0);
					S_CheckALError();

					alSourcei(src, AL_BUFFER, ch->sample->buffer);
					S_CheckALError();

					alSourcei(src, AL_LOOPING, !!(ch->play.flags & S_PLAY_LOOP));
					S_CheckALError();

					if (ch->play.flags & S_PLAY_AMBIENT) {
						alSourcei(src, AL_SAMPLE_OFFSET, (ALint) (Randomf() * ch->sample->num_samples));
						S_CheckALError();
					}

					alSourcePlay(src);
					S_CheckALError();

				} else {
					ALenum state;
					alGetSourcei(src, AL_SOURCE_STATE, &state);
					S_CheckALError();

					if (state != AL_PLAYING) {
						S_FreeChannel(i);
						continue;
					}
				}

				s_env.num_active_channels++;
			} else {

				S_FreeChannel(i);
			}
		}
	}
}

/**
 * @brief
 */
void S_AddSample(const s_play_sample_t *play) {

	if (!s_env.context) {
		return;
	}

	if (!s_volume->value) {
		return;
	}

	if (!play) {
		return;
	}

	if (!play->sample) {
		return;
	}

	if (play->entity >= MAX_ENTITIES) {
		return;
	}

	if (play->flags & S_PLAY_AMBIENT) {
		if (!s_ambient_volume->value) {
			return;
		}
	} else {
		if (!s_effects_volume->value) {
			return;
		}
	}
	
	const s_sample_t *sample = play->sample;

	const char *name = sample->media.name;
	if (name[0] == '*') {
		if (play->entity != -1) {
			const entity_state_t *ent = &cl.entities[play->entity].current;
			sample = S_LoadEntitySample(ent, sample->media.name);
			if (sample == NULL) {
				Com_Debug(DEBUG_SOUND, "Failed to load player model sound %s\n", name);
				return;
			}
		} else {
			Com_Warn("Player model sound %s requested without entity\n", name);
			return;
		}
	}

	// for sounds added every frame, if an instance of the sound already exists, cull this one

	if (play->flags & S_PLAY_FRAME) {
		s_channel_t *ch = s_env.channels;
		for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++) {
			if (ch->sample && (ch->play.flags & S_PLAY_FRAME)) {
				if (memcmp(play, &ch->play, sizeof(*play)) == 0) {
					ch->frame = cl.frame.frame_num;
					return;
				}
			}
		}
	}

	// warn on spatialized stereo samples

	if (play->sample->stereo) {
		if (play->atten) {
			Com_Warn("%s is a stereo sound sample and is being spatialized\n", play->sample->media.name);
		}
	}

	const int32_t i = S_AllocChannel();
	if (i != -1) {
		s_env.channels[i].play = *play;
		s_env.channels[i].sample = sample;
		s_env.channels[i].frame = cl.frame.frame_num;
	}
}
