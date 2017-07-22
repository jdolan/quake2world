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

#include "PlayViewController.h"

#include "CreateServerViewController.h"
#include "JoinServerViewController.h"

#define _Class _PlayViewController

#pragma mark - Object

static void dealloc(Object *self) {

	PlayViewController *this = (PlayViewController *) self;

	release(this->tabViewController);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;

	PlayViewController *this = (PlayViewController *) self;

	this->tabViewController = $(alloc(TabViewController), init);
	assert(this->tabViewController);

	ViewController *joinServerViewController = $((ViewController *) alloc(JoinServerViewController), init);
	assert(joinServerViewController);

	$((ViewController *) this->tabViewController, addChildViewController, joinServerViewController);
	release(joinServerViewController);

	ViewController *createServerViewController = $((ViewController *) alloc(CreateServerViewController), init);
	assert(createServerViewController);

	$((ViewController *) this->tabViewController, addChildViewController, createServerViewController);
	release(createServerViewController);

	$(self, addChildViewController, (ViewController *) this->tabViewController);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *PlayViewController::_PlayViewController(void)
 * @memberof PlayViewController
 */
Class *_PlayViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PlayViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(PlayViewController);
		clazz.interfaceOffset = offsetof(PlayViewController, interface);
		clazz.interfaceSize = sizeof(PlayViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class