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

#include "VideoView.h"

#include "CvarSelect.h"
#include "VideoModeSelect.h"

#define _Class _VideoView

#pragma mark - Actions

/**
 * @brief ActionFunction for Apply Button.
 */
static void applyAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	cgi.Cbuf("r_restart\n");
}

#pragma mark - VideoView

/**
 * @fn VideoView *VideoView::initWithFrame(VideoView *self, const SDL_Rect *frame)
 *
 * @memberof VideoView
 */
 static VideoView *initWithFrame(VideoView *self, const SDL_Rect *frame) {

	self = (VideoView *) super(View, self, initWithFrame, frame);
	if (self) {

		StackView *columns = $(alloc(StackView), initWithFrame, NULL);

		columns->spacing = DEFAULT_PANEL_SPACING;

		columns->axis = StackViewAxisHorizontal;
		columns->distribution = StackViewDistributionFillEqually;

		columns->view.autoresizingMask = ViewAutoresizingFill;

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);

			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Video");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				Control *videoModeSelect = (Control *) $(alloc(VideoModeSelect), initWithFrame, NULL, ControlStyleDefault);

				Cgui_Input((View *) stackView, "Video mode", videoModeSelect);
				release(videoModeSelect);

				cvar_t *r_fullscreen = cgi.CvarGet("r_fullscreen");
				Select *fullscreenSelect = (Select *) $(alloc(CvarSelect), initWithVariable, r_fullscreen);

				$(fullscreenSelect, addOption, "Windowed", (ident) 0);
				$(fullscreenSelect, addOption, "Fullscreen", (ident) 1);
				$(fullscreenSelect, addOption, "Borderless windowed", (ident) 2);

				Cgui_Input((View *) stackView, "Window mode", (Control *) fullscreenSelect);
				release(fullscreenSelect);

				Cgui_CvarCheckboxInput((View *) stackView, "Vertical sync", "r_swap_interval");

				Cgui_CvarSliderInput((View *) stackView, "Maximum FPS", "cl_max_fps", 30.0, 250.0, 10.0);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Rendering");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				Select *anisoSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_anisotropy");

				$(anisoSelect, addOption, "16x", (ident) 16);
				$(anisoSelect, addOption, "8x", (ident) 8);
				$(anisoSelect, addOption, "4x", (ident) 4); // Why isn't 2x an option?
				$(anisoSelect, addOption, "Off", (ident) 0);

				Cgui_Input((View *) stackView, "Anisotropy", (Control *) anisoSelect);
				release(anisoSelect);

				Select *multisampleSelect = (Select *) $(alloc(CvarSelect), initWithVariableName, "r_multisample");

				$(multisampleSelect, addOption, "8x", (ident) 4);
				$(multisampleSelect, addOption, "4x", (ident) 2);
				$(multisampleSelect, addOption, "2x", (ident) 1);
				$(multisampleSelect, addOption, "Off", (ident) 0);

				Cgui_Input((View *) stackView, "Multisample", (Control *) multisampleSelect);
				release(multisampleSelect);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Picture");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				Cgui_CvarSliderInput((View *) stackView, "Brightness", "r_brightness", 0.1, 2.0, 0.1);
				Cgui_CvarSliderInput((View *) stackView, "Contrast", "r_contrast", 0.1, 2.0, 0.1);
				Cgui_CvarSliderInput((View *) stackView, "Gamma", "r_gamma", 0.1, 2.0, 0.1);
				Cgui_CvarSliderInput((View *) stackView, "Modulate", "r_modulate", 0.1, 5.0, 0.1);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);

			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Effects");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				cvar_t *r_shadows = cgi.CvarGet("r_shadows");
				Select *shadowsSelect = (Select *) $(alloc(CvarSelect), initWithVariable, r_shadows);

				$(shadowsSelect, addOption, "Highest", (ident) 3);
				$(shadowsSelect, addOption, "High", (ident) 2);
				$(shadowsSelect, addOption, "Low", (ident) 1);
				$(shadowsSelect, addOption, "Off", (ident) 0);

				Cgui_Input((View *) stackView, "Shadows", (Control *) shadowsSelect);
				release(shadowsSelect);

				Cgui_CvarCheckboxInput((View *) stackView, "Caustics", "r_caustics");
				Cgui_CvarCheckboxInput((View *) stackView, "Warp", "r_warp");

				Cgui_CvarCheckboxInput((View *) stackView, "Weather effects", "cg_add_weather");

				Cgui_CvarCheckboxInput((View *) stackView, "Bump mapping", "r_bumpmap");
				Cgui_CvarCheckboxInput((View *) stackView, "Parallax mapping", "r_parallax");
				Cgui_CvarCheckboxInput((View *) stackView, "Deluxe mapping", "r_deluxemap");

				Cgui_CvarCheckboxInput((View *) stackView, "Stainmaps", "r_stainmap");

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) self, addSubview, (View *) columns);
		release(columns);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((VideoViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *VideoView::_VideoView(void)
 * @memberof VideoView
 */
Class *_VideoView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "VideoView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(VideoView);
		clazz.interfaceOffset = offsetof(VideoView, interface);
		clazz.interfaceSize = sizeof(VideoViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class