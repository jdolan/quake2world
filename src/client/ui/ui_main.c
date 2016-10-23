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

#include "ui_local.h"

#include "client.h"

#include "renderers/RendererQuetoo.h"

extern cl_static_t cls;

static WindowController *windowController;

/**
 * @brief Dispatch events to the user interface. Filter most common event types for
 * performance consideration.
 */
void Ui_HandleEvent(const SDL_Event *event) {

	if (cls.key_state.dest != KEY_UI) {

		switch (event->type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
			case SDL_MOUSEMOTION:
			case SDL_TEXTINPUT:
			case SDL_TEXTEDITING:
				return;
		}
	}

	$(windowController, respondToEvent, event);
}

/**
 * @brief
 */
void Ui_UpdateBindings(void) {

	if (windowController) {
		$(windowController->viewController->view, updateBindings);
	}
}

/**
 * @brief
 */
void Ui_Draw(void) {

	if (cls.state == CL_LOADING) {
		return;
	}

	if (cls.key_state.dest != KEY_UI) {
		return;
	}

	// backup all of the matrices
	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix)
		R_PushMatrix(matrix);

	$(windowController, render);

	// restore matrices
	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix)
		R_PopMatrix(matrix);
}

/**
 * @brief
 */
void Ui_AddViewController(ViewController *viewController) {
	if (viewController) {
		$(viewController, moveToParentViewController, windowController->viewController);
	}
}

/**
 * @brief
 */
void Ui_RemoveViewController(ViewController *viewController) {
	if (viewController) {
		$(viewController, moveToParentViewController, NULL);
	}
}

/**
 * @brief Initializes the user interface.
 */
void Ui_Init(void) {

#if defined(__APPLE__)
	const char *path = Fs_BaseDir();
	if (path) {
		char fonts[MAX_OS_PATH];
		g_snprintf(fonts, sizeof(fonts), "%s/Contents/MacOS/etc/fonts", path);

		setenv("FONTCONFIG_PATH", fonts, 0);
	}
#endif

	windowController = $(alloc(WindowController), initWithWindow, r_context.window);

	Renderer *renderer = (Renderer *) $(alloc(RendererQuetoo), init);

	$(windowController, setRenderer, renderer);

	release(renderer);

	ViewController *viewController = $(alloc(ViewController), init);

	$(windowController, setViewController, viewController);

	release(viewController);
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
