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

#include "viewcontrollers/EditorViewController.h"

extern cl_static_t cls;

static WindowController *windowController;

static NavigationViewController *navigationViewController;

static EditorViewController *editorViewController;

/**
 * @brief
 */
static void Ui_CheckEditor(void) {
	if (cl_editor->modified) {
		printf("Changed!\n");

		cl_editor->modified = false;

		if (cl_editor->integer) {
			if (cls.state != CL_ACTIVE) {
				return;
			}

			if (editorViewController) {
				Ui_PopToViewController((ViewController *) editorViewController);
				Ui_PopViewController();

				release(editorViewController);
			}

			editorViewController = $(alloc(EditorViewController), init);

			vec3_t end;

			VectorMA(r_view.origin, MAX_WORLD_DIST, r_view.forward, end);

			cm_trace_t tr = Cl_Trace(r_view.origin, end, NULL, NULL, 0, MASK_SOLID);

			if (tr.fraction < 1.0) {
				editorViewController->material = R_LoadMaterial(va("textures/%s", tr.surface->name));

				if (!editorViewController->material) {
					Com_Debug(DEBUG_CLIENT, "Failed to resolve %s\n", tr.surface->name);
				}
			} else {
				editorViewController->material = NULL;
			}

			// editorViewController->bumpSlider and friends seg fault when accessing

			/*
			if (editorViewController->material) {
				$(editorViewController->bumpSlider, setValue, editorViewController->material->cm->bump);
				$(editorViewController->hardnessSlider, setValue, editorViewController->material->cm->hardness);
				$(editorViewController->specularSlider, setValue, editorViewController->material->cm->specular);
				$(editorViewController->parallaxSlider, setValue, editorViewController->material->cm->parallax);
			}
			*/

			Ui_PushViewController((ViewController *) editorViewController);
		} else if (editorViewController) {
			Ui_PopToViewController((ViewController *) editorViewController);
			Ui_PopViewController();

			release(editorViewController);

			editorViewController = NULL;
		}
	}
}

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

	Ui_CheckEditor();

	if (cls.key_state.dest != KEY_UI) {
		return;
	}

	// backup all of the matrices
	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix) {
		R_PushMatrix(matrix);
	}

	R_SetMatrix(R_MATRIX_PROJECTION, &r_view.matrix_base_ui);

	$(windowController, render);

	// restore matrices
	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix) {
		R_PopMatrix(matrix);
	}
}

/**
 * @brief
 */
void Ui_PushViewController(ViewController *viewController) {
	if (viewController) {
		$(navigationViewController, pushViewController, viewController);
	}
}

/**
 * @brief
 */
void Ui_PopToViewController(ViewController *viewController) {
	if (viewController) {
		$(navigationViewController, popToViewController, viewController);
	}
}

/**
 * @brief
 */
void Ui_PopViewController(void) {
	$(navigationViewController, popViewController);
}

/**
 * @brief
 */
void Ui_PopAllViewControllers(void) {
	$(navigationViewController, popToRootViewController);
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

	// ui renderer

	Renderer *renderer = (Renderer *) $(alloc(RendererQuetoo), init);

	// main window

	windowController = $(alloc(WindowController), initWithWindow, r_context.window);

	$(windowController, setRenderer, renderer);

	navigationViewController = $(alloc(NavigationViewController), init);

	$(windowController, setViewController, (ViewController *) navigationViewController);

	editorViewController = NULL;
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	Ui_PopAllViewControllers();

	if (editorViewController != NULL) {
		release(editorViewController);
	}

	release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
