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
 * along with self program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <assert.h>

#include "EditorView.h"
#include "ui_local.h"
#include "client.h"

#define _Class _EditorView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	EditorView *this = (EditorView *) self;

	release(this->bumpSlider);
	release(this->hardnessSlider);
	release(this->specularSlider);
	release(this->parallaxSlider);
	release(this->saveButton);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	EditorView *this = (EditorView *) self;

	vec3_t end;
	VectorMA(r_view.origin, MAX_WORLD_DIST, r_view.forward, end);

	const cm_trace_t tr = Cl_Trace(r_view.origin, end, NULL, NULL, 0, MASK_SOLID);

	if (tr.fraction < 1.0) {
		if (tr.surface->material) {
			this->material = R_LoadMaterial(tr.surface->material->diffuse);
		} else {
			this->material = NULL;
		}
	} else {
		this->material = NULL;
	}

	if (this->material) {
		this->materialName->defaultText = this->material->cm->base;
		this->diffuseTexture->defaultText = this->material->cm->diffuse;
		this->normalmapTexture->defaultText = this->material->cm->normalmap;

		$(this->bumpSlider, setValue, (double) this->material->cm->bump);
		$(this->hardnessSlider, setValue, (double) this->material->cm->hardness);
		$(this->specularSlider, setValue, (double) this->material->cm->specular);
		$(this->parallaxSlider, setValue, (double) this->material->cm->parallax);
	} else {
		strcpy(this->materialName->defaultText, "none");
		strcpy(this->diffuseTexture->defaultText, "none");
		strcpy(this->normalmapTexture->defaultText, "none");

		$(this->bumpSlider, setValue, DEFAULT_BUMP);
		$(this->hardnessSlider, setValue, DEFAULT_HARDNESS);
		$(this->specularSlider, setValue, DEFAULT_SPECULAR);
		$(this->parallaxSlider, setValue, DEFAULT_PARALLAX);
	}
}

#pragma mark - EditorView

#define INPUT_LABEL_WIDTH 140

/**
 * @brief Adds a new Label and the specified Control to the given View.
 * @remarks This function releases the Control for convenience.
 */
static void addInput(View *view, const char *label, Control *control) {

	assert(view);
	assert(control);

	Input *input = $(alloc(Input), initWithFrame, NULL);
	assert(input);

	$(input, setControl, control);

	$(input->label->text, setText, label);

	input->label->view.autoresizingMask &= ~ViewAutoresizingContain;
	input->label->view.frame.w = INPUT_LABEL_WIDTH;

	$(view, addSubview, (View *) input);

	release(input);
}

/**
 * @fn EditorView *EditorView::initWithFrame(EditorView *self, const SDL_Rect *frame)
 * @memberof EditorView
 */
static EditorView *initWithFrame(EditorView *self, const SDL_Rect *frame) {

	self = (EditorView *) super(View, self, initWithFrame, frame);
	if (self) {

		Panel *panel = $(alloc(Panel), initWithFrame, NULL);

		((View *) self)->alignment = ViewAlignmentMiddleCenter;

		panel->stackView.view.alignment = ViewAlignmentMiddleCenter;
		panel->stackView.view.needsLayout = true;

		panel->accessoryView->view.hidden = false;

		StackView *columns = $(alloc(StackView), initWithFrame, NULL);

		columns->axis = StackViewAxisHorizontal;
		columns->spacing = DEFAULT_PANEL_SPACING;

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);
			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "MATERIAL");

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				const SDL_Rect frame = { .w = 300, .h = 0 };

				self->materialName = $(alloc(TextView), initWithFrame, &frame, ControlStyleDefault);
				self->materialName->isEditable = false;
				addInput((View *) stackView, "Material name", (Control *) self->materialName);

				self->diffuseTexture = $(alloc(TextView), initWithFrame, &frame, ControlStyleDefault);
				self->diffuseTexture->isEditable = false;
				addInput((View *) stackView, "Diffuse texture", (Control *) self->diffuseTexture);

				self->normalmapTexture = $(alloc(TextView), initWithFrame, &frame, ControlStyleDefault);
				self->normalmapTexture->isEditable = false;
				addInput((View *) stackView, "Normalmap texture", (Control *) self->normalmapTexture);

				self->bumpSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
				self->bumpSlider->min = 0.0;
				self->bumpSlider->max = 20.0;
				self->bumpSlider->step = 0.125;
				addInput((View *) stackView, "Bump", (Control *) self->bumpSlider);

				self->hardnessSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
				self->hardnessSlider->min = 0.0;
				self->hardnessSlider->max = 20.0;
				self->hardnessSlider->step = 0.1;
				addInput((View *) stackView, "Hardness", (Control *) self->hardnessSlider);

				self->specularSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
				self->specularSlider->min = 0.0;
				self->specularSlider->max = 20.0;
				self->specularSlider->step = 0.1;
				addInput((View *) stackView, "Specular", (Control *) self->specularSlider);

				self->parallaxSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
				self->parallaxSlider->min = 0.0;
				self->parallaxSlider->max = 20.0;
				self->parallaxSlider->step = 0.1;
				addInput((View *) stackView, "Parallax", (Control *) self->parallaxSlider);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) panel->contentView, addSubview, (View *) columns);
		release(columns);

		self->saveButton = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);
		$(self->saveButton->title, setText, "Save");

		$((View *) panel->accessoryView, addSubview, (View *) self->saveButton);

		$((View *) self, addSubview, (View *) panel);
		release(panel);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((EditorViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *EditorView::_EditorView(void)
 * @memberof EditorView
 */
Class *_EditorView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "EditorView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(EditorView);
		clazz.interfaceOffset = offsetof(EditorView, interface);
		clazz.interfaceSize = sizeof(EditorViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
