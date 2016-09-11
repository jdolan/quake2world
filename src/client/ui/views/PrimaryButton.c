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

#include <assert.h>

#include "PrimaryButton.h"

#define _Class _PrimaryButton

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 */
static void respondToEvent(View *self, const SDL_Event *event) {

	super(View, self, respondToEvent, event);

	((Control *) self)->bevel = ControlBevelTypeNone;
}

#pragma mark - PrimaryButton

/**
 * @fn PrimaryButton *PrimaryButton::init(PrimaryButton *self)
 *
 * @memberof PrimaryButton
 */
static PrimaryButton *initWithFrame(PrimaryButton *self, const SDL_Rect *frame, ControlStyle style) {

	self = (PrimaryButton *) super(Button, self, initWithFrame, frame, style);
	if (self) {
		$(self->button.title, setFont, $$(Font, defaultFont, FontCategoryPrimaryResponder));

		self->button.control.bevel = ControlBevelTypeNone;

		self->button.control.view.borderWidth = 1;
	}
	
	return self;
}

/**
 * @fn void PrimaryButton::button(View *view, const char *name, ActionFunction action, ident sender, ident data)
 *
 * @memberof PrimaryButton
 */
static void button(View *view, const char *name, ActionFunction action, ident sender, ident data) {

	assert(view);

	PrimaryButton *button = $(alloc(PrimaryButton), initWithFrame, NULL, ControlStyleDefault);
	assert(button);

	$(button->button.title, setText, name);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, action, sender, data);

	$(view, addSubview, (View *) button);
	release(button);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

	((PrimaryButtonInterface *) clazz->interface)->initWithFrame = initWithFrame;
	((PrimaryButtonInterface *) clazz->interface)->button = button;
}

Class _PrimaryButton = {
	.name = "PrimaryButton",
	.superclass = &_Button,
	.instanceSize = sizeof(PrimaryButton),
	.interfaceOffset = offsetof(PrimaryButton, interface),
	.interfaceSize = sizeof(PrimaryButtonInterface),
	.initialize = initialize,
};

#undef _Class

