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

#include "PlayerModelView.h"

#define _Class _PlayerModelView

#pragma mark - Actions

/**
 * @brief ActionFunction for clicking the model's 3D view
 */
static void rotateAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	PlayerModelView *this = (PlayerModelView *) sender;

	this->yaw += event->motion.xrel;
}

/**
 * @brief ActionFunction for zooming the model's 3D view
 */
static void zoomAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	PlayerModelView *this = (PlayerModelView *) sender;

	this->zoom = Clampf(this->zoom + event->wheel.y * 0.0125, 0.0, 1.0);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	PlayerModelView *this = (PlayerModelView *) self;

	release(this->iconView);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @brief
 */
static View *init(View *self) {
	return (View *) $((PlayerModelView *) self, initWithFrame, NULL);
}

/**
 * @brief Renders the given entity stub.
 */
// static void renderMeshEntity(r_entity_t *e) {
//
// 	cgi.view->current_entity = e;
//
// 	cgi.SetMatrixForEntity(e);
//
// 	cgi.EnableBlend(false);
//
// 	cgi.DrawMeshModel(e);
//
// 	cgi.EnableBlend(true);
// }

/**
 * @brief Renders the given entity stub.
 */
// static void renderMeshMaterialsEntity(r_entity_t *e) {
//
//	cgi.view->current_entity = e;
//
//	cgi.DrawMeshModelMaterials(e);
// }

#define NEAR_Z 1.0
#define FAR_Z  MAX_WORLD_COORD

/**
 * @see View::render(View *, Renderer *)
 */
static void render(View *self, Renderer *renderer) {

	super(View, self, render, renderer);

	PlayerModelView *this = (PlayerModelView *) self;

	if (this->client.torso == NULL) {
		$(self, updateBindings);
	}

	if (this->client.torso) {
		$(this, animate);

//		cgi.PushMatrix(R_MATRIX_PROJECTION);
//		cgi.PushMatrix(R_MATRIX_MODELVIEW);
//
//		const SDL_Rect viewport = $(self, viewport);
//		cgi.SetViewport(viewport.x, viewport.y, viewport.w, viewport.h, false);
//
//		// create projection matrix
//		const float aspect = (float) viewport.w / (float) viewport.h;
//
//		const float ymax = NEAR_Z * tanf(radians(40)); // Field of view
//		const float ymin = -ymax;
//
//		const float xmin = ymin * aspect;
//		const float xmax = ymax * aspect;
//		mat4_t mat;
//
//		Matrix4x4_FromFrustum(&mat, xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);
//
//		cgi.SetMatrix(R_MATRIX_PROJECTION, &mat);
//
//		// create base modelview matrix
//		Matrix4x4_CreateIdentity(&mat);
//
//		// Quake is retarded: rotate so that Z is up
//		Matrix4x4_ConcatRotate(&mat, -90.0, 1.0, 0.0, 0.0);
//		Matrix4x4_ConcatRotate(&mat,  90.0, 0.0, 0.0, 1.0);
//		Matrix4x4_ConcatTranslate(&mat, 90.0 - (this->zoom * 45.0), 0.0, 20.0 - (this->zoom * 35.0));
//
//		Matrix4x4_ConcatRotate(&mat, -25.0 - (this->zoom * -10.0), 0.0, 1.0, 0.0);
//
//		Matrix4x4_ConcatRotate(&mat, sinf(radians(cgi.client->unclamped_time * 0.05)) * 10.0, 0.0, 0.0, 1.0);
//
//		cgi.EnableDepthTest(true);
//		cgi.DepthRange(0.0, 0.1);
//
//		// Platform base; doesn't rotate
//
//		cgi.SetMatrix(R_MATRIX_MODELVIEW, &mat);
//
//		renderMeshEntity(&this->platformBase);
//		renderMeshMaterialsEntity(&this->platformBase);
//
//		// Rotating stuff
//
//		Matrix4x4_ConcatRotate(&mat, this->yaw, 0.0, 0.0, 1.0);
//
//		cgi.SetMatrix(R_MATRIX_MODELVIEW, &mat);
//
//		renderMeshEntity(&this->legs);
//		renderMeshEntity(&this->torso);
//		renderMeshEntity(&this->head);
//		renderMeshEntity(&this->weapon);
//		renderMeshEntity(&this->platformCenter);
//
//		renderMeshMaterialsEntity(&this->legs);
//		renderMeshMaterialsEntity(&this->torso);
//		renderMeshMaterialsEntity(&this->head);
//		renderMeshMaterialsEntity(&this->weapon);
//		renderMeshMaterialsEntity(&this->platformCenter);
//
//		cgi.DepthRange(0.0, 1.0);
//		cgi.EnableDepthTest(false);
//
//		cgi.SetViewport(0, 0, cgi.context->width, cgi.context->height, false);
//
//		cgi.PopMatrix(R_MATRIX_MODELVIEW);
//		cgi.PopMatrix(R_MATRIX_PROJECTION);
	}
}

/**
 * @see View::renderDeviceDidReset(View *)
 */
static void renderDeviceDidReset(View *self) {

	super(View, self, renderDeviceDidReset);

	PlayerModelView *this = (PlayerModelView *) self;

	this->info[0] = '\0';

	$(self, updateBindings);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {
	
	super(View, self, updateBindings);

	PlayerModelView *this = (PlayerModelView *) self;

	this->animation1.frame = this->animation2.frame = -1;

	char info[MAX_STRING_CHARS];
	g_snprintf(info, sizeof(info), "newbie\\%s\\%s\\%s\\%s\\0",
			   cg_skin->string, cg_shirt->string, cg_pants->string, cg_helmet->string);

	if (strcmp(this->info, info) == 0) {
		return;
	}

	g_strlcpy(this->info, info, sizeof(this->info));
	Cg_LoadClient(&this->client, this->info);

	this->legs.model = this->client.legs;
	this->legs.scale = 1.0;
	this->legs.color = Vec4(1.0, 1.0, 1.0, 1.0);

	this->torso.model = this->client.torso;
	this->torso.scale = 1.0;
	this->torso.color = Vec4(1.0, 1.0, 1.0, 1.0);

	this->head.model = this->client.head;
	this->head.scale = 1.0;
	this->head.color = Vec4(1.0, 1.0, 1.0, 1.0);

	this->weapon.model = cgi.LoadModel("models/weapons/rocketlauncher/tris");
	this->weapon.scale = 1.0;
	this->weapon.color = Vec4(1.0, 1.0, 1.0, 1.0);

	this->platformBase.model = cgi.LoadModel("models/objects/platform/base/tris");
	this->platformBase.scale = 1.0;
	this->platformBase.color = Vec4(1.0, 1.0, 1.0, 1.0);

	this->platformCenter.model = cgi.LoadModel("models/objects/platform/center/tris");
	this->platformCenter.scale = 1.0;
	this->platformCenter.color = Vec4(1.0, 1.0, 1.0, 1.0);

	memcpy(this->legs.skins, this->client.legs_skins, sizeof(this->legs.skins));
	memcpy(this->torso.skins, this->client.torso_skins, sizeof(this->torso.skins));
	memcpy(this->head.skins, this->client.head_skins, sizeof(this->head.skins));

	this->iconView->texture = this->client.icon->texnum;
}

#pragma mark - Control

/**
 * @see Control::captureEvent(Control *, const SDL_Event *)
 */
static _Bool captureEvent(Control *self, const SDL_Event *event) {

	if (event->type == SDL_MOUSEMOTION && event->motion.state & SDL_BUTTON_LMASK) {

		if ($((View *) self, didReceiveEvent, event)) {
			return true;
		}
	} else if (event->type == SDL_MOUSEWHEEL) {

		if ($((View *) self, didReceiveEvent, event)) {
			return true;
		}
	}

	return super(Control, self, captureEvent, event);
}

#pragma mark - PlayerModelView

/**
 * @brief Runs the animation, proceeding to the next in the sequence upon completion.
 */
static void animate_(const r_mesh_model_t *model, cl_entity_animation_t *a, r_entity_t *e) {

	e->frame = e->old_frame = 0;
	e->lerp = 1.0;
	e->back_lerp = 0.0;

	const r_mesh_animation_t *anim = &model->animations[a->animation];

	const uint32_t frameTime = 1500 / anim->hz;
	const uint32_t animationTime = anim->num_frames * frameTime;
	const uint32_t elapsedTime = cgi.client->unclamped_time - a->time;
	uint16_t frame = elapsedTime / frameTime;

	if (elapsedTime >= animationTime) {

		a->time = cgi.client->unclamped_time;

		animate_(model, a, e);
		return;
	}

	frame = anim->first_frame + frame;

	if (frame != a->frame) {
		if (a->frame == -1) {
			a->old_frame = frame;
			a->frame = frame;
		} else {
			a->old_frame = a->frame;
			a->frame = frame;
		}
	}

	a->lerp = (elapsedTime % frameTime) / (float) frameTime;
	a->fraction = elapsedTime / (float) animationTime;

	e->frame = a->frame;
	e->old_frame = a->old_frame;
	e->lerp = a->lerp;
	e->back_lerp = 1.0 - a->lerp;
}

/**
 * @fn void PlayerModelView::animate(PlayerModelView *self)
 * @memberof PlayerModelView
 */
static void animate(PlayerModelView *self) {

	const r_mesh_model_t *model = self->torso.model->mesh;

	animate_(model, &self->animation1, &self->torso);
	animate_(model, &self->animation2, &self->legs);

	self->head.frame = 0;
	self->head.lerp = 1.0;

	self->weapon.frame = 0;
	self->weapon.lerp = 1.0;

	self->legs.origin = Vec3_Zero();
	self->torso.origin = Vec3_Zero();
	self->head.origin = Vec3_Zero();
	self->weapon.origin = Vec3_Zero();
	self->platformBase.origin = Vec3_Zero();
	self->platformCenter.origin = Vec3_Zero();

	self->legs.angles = Vec3_Zero();
	self->torso.angles = Vec3_Zero();
	self->head.angles = Vec3_Zero();
	self->weapon.angles = Vec3_Zero();
	self->platformBase.angles = Vec3_Zero();
	self->platformCenter.angles = Vec3_Zero();

	self->legs.scale = self->torso.scale = self->head.scale = 1.0;
	self->weapon.scale = 1.0;
	self->platformBase.scale = self->platformCenter.scale = 1.0;

	if (self->client.shirt.a) {
		self->torso.tints[0] = Color_Vec4(self->client.shirt);
	} else {
		self->torso.tints[0] = Vec4_Zero();
	}

	if (self->client.pants.a) {
		self->legs.tints[1] = Color_Vec4(self->client.pants);
	} else {
		self->legs.tints[1] = Vec4_Zero();
	}

	if (self->client.helmet.a) {
		self->head.tints[2] = Color_Vec4(self->client.helmet);
	} else {
		self->head.tints[2] = Vec4_Zero();
	}

	Matrix4x4_CreateFromEntity(&self->legs.matrix, self->legs.origin, self->legs.angles, self->legs.scale);
	Matrix4x4_CreateFromEntity(&self->torso.matrix, self->torso.origin, self->torso.angles, self->torso.scale);
	Matrix4x4_CreateFromEntity(&self->head.matrix, self->head.origin, self->head.angles, self->head.scale);
	Matrix4x4_CreateFromEntity(&self->weapon.matrix, self->weapon.origin, self->weapon.angles, self->weapon.scale);
	Matrix4x4_CreateFromEntity(&self->platformBase.matrix, self->platformBase.origin, self->platformBase.angles, self->platformBase.scale);
	Matrix4x4_CreateFromEntity(&self->platformCenter.matrix, self->platformCenter.origin, self->platformCenter.angles, self->platformCenter.scale);

//	Cg_ApplyMeshTag(&self->torso, &self->legs, "tag_torso");
//	Cg_ApplyMeshTag(&self->head, &self->torso, "tag_head");
//	Cg_ApplyMeshTag(&self->weapon, &self->torso, "tag_weapon");
}

/**
 * @fn PlayerModelView *PlayerModelView::initWithFrame(PlayerModelView *self, const SDL_Rect *frame)
 *
 * @memberof PlayerModelView
 */
static PlayerModelView *initWithFrame(PlayerModelView *self, const SDL_Rect *frame) {

	self = (PlayerModelView *) super(Control, self, initWithFrame, frame);
	if (self) {
		self->yaw = 150.0;
		self->zoom = 0.1;

		self->animation1.animation = ANIM_TORSO_STAND1;
		self->animation2.animation = ANIM_LEGS_IDLE;

		self->iconView = $(alloc(ImageView), initWithFrame, NULL);
		assert(self->iconView);

		$((View *) self->iconView, addClassName, "iconView");
		$((View *) self, addSubview, (View *) self->iconView);

		$((Control *) self, addActionForEventType, SDL_MOUSEMOTION, rotateAction, self, NULL);
		$((Control *) self, addActionForEventType, SDL_MOUSEWHEEL, zoomAction, self, NULL);
	}

	return self;
}

#pragma mark - Class lifecycle

/*-*
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->interface)->init = init;
	((ViewInterface *) clazz->interface)->render = render;
	((ViewInterface *) clazz->interface)->renderDeviceDidReset = renderDeviceDidReset;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->interface)->captureEvent = captureEvent;

	((PlayerModelViewInterface *) clazz->interface)->animate = animate;
	((PlayerModelViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *PlayerModelView::_PlayerModelView(void)
 * @memberof PlayerModelView
 */
Class *_PlayerModelView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "PlayerModelView",
			.superclass = _Control(),
			.instanceSize = sizeof(PlayerModelView),
			.interfaceOffset = offsetof(PlayerModelView, interface),
			.interfaceSize = sizeof(PlayerModelViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
