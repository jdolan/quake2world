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

#include <Objectively/Value.h>

#include "cg_local.h"

#include "CreateServerViewController.h"
#include "MapListCollectionItemView.h"

#define _Class _CreateServerViewController

#pragma mark - Actions and delegate callbacks

/**
 * @brief Select gameplay mode
 */
static void selectGameplay(Select *select, Option *option) {
	if (option->value == (ident) 0) {
		cgi.CvarSet(g_gameplay->name, "default");
	} else if (option->value == (ident) 1) {
		cgi.CvarSet(g_gameplay->name, "deathmatch");
	} else if (option->value == (ident) 2) {
		cgi.CvarSet(g_gameplay->name, "instagib");
	} else if (option->value == (ident) 3) {
		cgi.CvarSet(g_gameplay->name, "arena");
	} else if (option->value == (ident) 4) {
		cgi.CvarSet(g_gameplay->name, "duel");
	}
}

/**
 * @brief Select teamplay mode
 */
static void selectTeamsplay(Select *select, Option *option) {
	if (option->value == (ident) 0) {
		cgi.CvarSet(g_teams->name, "0");
		cgi.CvarSet(g_ctf->name, "0");
	} else if (option->value == (ident) 1) {
		cgi.CvarSet(g_teams->name, "1");
		cgi.CvarSet(g_ctf->name, "0");
	} else if (option->value == (ident) 2) {
		cgi.CvarSet(g_teams->name, "0");
		cgi.CvarSet(g_ctf->name, "1");
	}
}

/**
 * @brief ActionFunction for the Create button.
 */
static void createAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	CreateServerViewController *this = (CreateServerViewController *) sender;

	Array *selectedMaps = $(this->mapList, selectedMaps);
	if (selectedMaps->count) {
		char map[MAX_QPATH];

		file_t *file = cgi.OpenFileWrite(MAP_LIST_UI);
		if (file) {

			MutableString *string = mstr("");

			for (size_t i = 0; i < selectedMaps->count; i++) {

				const Value *value = $(selectedMaps, objectAtIndex, i);
				const MapListItemInfo *info = (MapListItemInfo *) value->value;

				char name[MAX_QPATH];
				StripExtension(Basename(info->mapname), name);

				if (i == 0) {
					g_strlcpy(map, name, sizeof(map));
				}

				$(string, appendFormat, "{\n\tname %s\n}\n", name);
			}

			const int64_t len = cgi.WriteFile(file, string->string.chars, string->string.length, 1);

			if (len == -1) {
				cgi.Warn("Failed to write %s\n", MAP_LIST_UI);
			} else {
				cgi.Debug("Wrote %s %"PRId64" bytes\n", MAP_LIST_UI, len);
			}

			release(string);
		}

		cgi.CloseFile(file);

		cgi.CvarSet("g_map_list", MAP_LIST_UI);
		cgi.Cbuf(va("map %s", map));

	} else {
		cgi.Print("No maps selected\n");
	}

	release(selectedMaps);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	CreateServerViewController *this = (CreateServerViewController *) self;

	release(this->gameplay);
	release(this->mapList);
	release(this->matchMode);
	release(this->teamsplay);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	CreateServerViewController *this = (CreateServerViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "CREATE SERVER");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_CvarTextView((View *) stackView, "Hostname", "sv_hostname");
			Cg_CvarTextView((View *) stackView, "Clients", "sv_max_clients");
			Cg_CvarCheckboxInput((View *) stackView, "Public", "sv_public");
			Cg_CvarTextView((View *) stackView, "Password", "password");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "GAME");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			this->gameplay = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

			$(this->gameplay, addOption, "Default", (ident) 0);
			$(this->gameplay, addOption, "Deathmatch", (ident) 1);
			$(this->gameplay, addOption, "Instagib", (ident) 2);
			$(this->gameplay, addOption, "Arena", (ident) 3);
			$(this->gameplay, addOption, "Duel", (ident) 4);

			this->gameplay->control.view.frame.w = DEFAULT_TEXTVIEW_WIDTH;
			this->gameplay->delegate.didSelectOption = selectGameplay;

			if (!g_strcmp0(g_gameplay->string, "default")) {
				$(this->gameplay, selectOptionWithValue, (ident) 0);
			} else if (!g_strcmp0(g_gameplay->string, "deathmatch")) {
				$(this->gameplay, selectOptionWithValue, (ident) 1);
			} else if (!g_strcmp0(g_gameplay->string, "instagib")) {
				$(this->gameplay, selectOptionWithValue, (ident) 2);
			} else if (!g_strcmp0(g_gameplay->string, "arena")) {
				$(this->gameplay, selectOptionWithValue, (ident) 3);
			} else if (!g_strcmp0(g_gameplay->string, "duel")) {
				$(this->gameplay, selectOptionWithValue, (ident) 4);
			}

			Cg_Input((View *) stackView, "Gameplay", (Control *) this->gameplay);

			this->teamsplay = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

			$(this->teamsplay, addOption, "Free for All", (ident) 0);
			$(this->teamsplay, addOption, "Team Deathmatch", (ident) 1);
			$(this->teamsplay, addOption, "Capture the Flag", (ident) 2);

			this->teamsplay->control.view.frame.w = DEFAULT_TEXTVIEW_WIDTH;
			this->teamsplay->delegate.didSelectOption = selectTeamsplay;

			if (g_ctf->integer != 0) {
				$(this->teamsplay, selectOptionWithValue, (ident) 2);
			} else {
				$(this->teamsplay, selectOptionWithValue, (ident) (ptrdiff_t) g_teams->integer);
			}

			Cg_Input((View *) stackView, "Teams play", (Control *) this->teamsplay);

			Cg_CvarCheckboxInput((View *) stackView, "Match mode", "g_match");

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
			$(box->label, setText, "MAP LIST");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);
			stackView->spacing = DEFAULT_PANEL_SPACING;

			const SDL_Rect frame = { .w = 760, .h = 600 };
			this->mapList = $(alloc(MapListCollectionView), initWithFrame, &frame, ControlStyleDefault);

			$((View *) stackView, addSubview, (View *) this->mapList);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->menuViewController.panel->contentView, addSubview, (View *) columns);
	release(columns);

	this->menuViewController.panel->accessoryView->view.hidden = false;
	Cg_Button((View *) this->menuViewController.panel->accessoryView, "Create", createAction, self, NULL);

}

#pragma mark - MapListCollectionView

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

Class _CreateServerViewController = {
	.name = "CreateServerViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(CreateServerViewController),
	.interfaceOffset = offsetof(CreateServerViewController, interface),
	.interfaceSize = sizeof(CreateServerViewControllerInterface),
	.initialize = initialize,
};

#undef _Class
