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


#include "sys.h"
#include "filesystem.h"

#include <signal.h>

#if defined(_MSC_VER)
	#include <DbgHelp.h>
#else
	#include <sys/time.h>
#endif

#if defined(_WIN32)
	#include <shlobj.h>
	#define dlopen(file_name, mode) LoadLibrary(file_name)

	static const char *dlerror() {
		char num_buffer[32];
		const DWORD err = GetLastError();

		itoa(err, num_buffer, 10);
		return va("Error loading library: %lu", err);
	}

	#define dlsym(handle, symbol) GetProcAddress(handle, symbol)
	#define dlclose(handle) FreeLibrary(handle)
#else
	#include <dlfcn.h>
#endif

#if defined(__APPLE__)
	#include <mach-o/dyld.h>
#endif

#if HAVE_EXECINFO
	#include <execinfo.h>
	#define MAX_BACKTRACE_SYMBOLS 50
#endif

#include <SDL2/SDL.h>

/**
 * @return The current executable path (argv[0]).
 */
const char *Sys_ExecutablePath(void) {
	static char path[MAX_OS_PATH];

#if defined(__APPLE__)
	uint32_t i = sizeof(path);

	if (_NSGetExecutablePath(path, &i) > -1) {
		return path;
	}

#elif defined(__linux__)

	if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path)) > -1) {
		return path;
	}

#elif defined(_WIN32)

	if (GetModuleFileName(0, path, sizeof(path))) {
		return path;
	}

#endif

	Com_Warn("Failed to resolve executable path\n");
	return NULL;
}

/**
 * @return The current user's name.
 */
const char *Sys_Username(void) {
	return g_get_user_name();
}

/**
 * @brief Returns the current user's Quetoo directory.
 *
 * @remarks On Windows, this is `\My Documents\My Games\Quetoo`. On POSIX
 * platforms, it's `~/.quetoo`.
 */
const char *Sys_UserDir(void) {
	static char user_dir[MAX_OS_PATH];

#if defined(_WIN32)
	const char *my_documents = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
	g_snprintf(user_dir, sizeof(user_dir), "%s\\My Games\\Quetoo", my_documents);
#else
	const char *home = g_get_home_dir();
	g_snprintf(user_dir, sizeof(user_dir), "%s/.quetoo", home);
#endif

	return user_dir;
}

/**
 * @brief
 */
void Sys_OpenLibrary(const char *name, void **handle) {
	*handle = NULL;

#if defined(_WIN32)
	char *so_name = va("%s.dll", name);
#else
	char *so_name = va("%s.so", name);
#endif

	if (Fs_Exists(so_name)) {
		char path[MAX_OS_PATH];

		g_snprintf(path, sizeof(path), "%s%c%s", Fs_RealDir(so_name), G_DIR_SEPARATOR, so_name);
		Com_Print("Trying %s...\n", path);

		if ((*handle = dlopen(path, RTLD_NOW))) {
			return;
		}

		Com_Error(ERROR_DROP, "%s\n", dlerror());
	}

	Com_Error(ERROR_DROP, "Couldn't find %s\n", so_name);
}

/**
 * @brief Closes an open game module.
 */
void Sys_CloseLibrary(void **handle) {
	if (*handle) {
		dlclose(*handle);
	}
	*handle = NULL;
}

/**
 * @brief Opens and loads the specified shared library. The function identified by
 * entry_point is resolved and invoked with the specified parameters, its
 * return value returned by this function.
 */
void *Sys_LoadLibrary(const char *name, void **handle, const char *entry_point, void *params) {
	typedef void *EntryPointFunc(void *);
	EntryPointFunc *EntryPoint;

	if (*handle) {
		Com_Warn("%s: handle already open\n", name);
		Sys_CloseLibrary(handle);
	}

	Sys_OpenLibrary(name, handle);

	EntryPoint = (EntryPointFunc *) dlsym(*handle, entry_point);

	if (!EntryPoint) {
		Sys_CloseLibrary(handle);
		Com_Error(ERROR_DROP, "%s: Failed to resolve %s\n", name, entry_point);
	}

	return EntryPoint(params);
}

/**
 * @brief On platforms supporting it, print a backtrace.
 */
void Sys_Backtrace(const char *msg) {

#if HAVE_EXECINFO

	void *symbols[MAX_BACKTRACE_SYMBOLS];

	const int32_t count = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);

	if (!Com_WasInit(QUETOO_CLIENT)) {
		backtrace_symbols_fd(symbols, count, STDERR_FILENO);
		return;
	}

	char **strings = backtrace_symbols(symbols, count);

	char message[MAX_STRING_CHARS] = "";
	for (int32_t i = 0; i < count; i++) {
		g_strlcat(message, strings[i], sizeof(message));
		g_strlcat(message, "\n", sizeof(message));
	}

	free(strings);

	const SDL_MessageBoxButtonData buttons[] = {};

#elif defined(__MINGW32__)

	char *message = "Quetoo encountered a fatal error.";
	const SDL_MessageBoxButtonData buttons[] = {};

#elif defined(_MSC_VER)

	char *message = va("%s\n\nGenerate crash dump?", msg);

	const SDL_MessageBoxButtonData buttons[] = {
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" }
	};

#endif

	const SDL_MessageBoxData data = {
		.flags = SDL_MESSAGEBOX_ERROR,
		.window = NULL,
		.title = msg ?: "Fatal Error",
		.message = message,
		.numbuttons = lengthof(buttons),
		.buttons = buttons,
		.colorScheme = NULL
	};

	int32_t button;
	if (SDL_ShowMessageBox(&data, &button) < 0) {
		SDL_Log("Error displaying message box");
		return;
	}

#if defined(_MSC_VER)

	if (button == 1) {
		RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
	}

#endif
}

/**
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
void Sys_Signal(int32_t s) {

	switch (s) {
		case SIGINT:
		case SIGTERM:
#ifndef _WIN32
		case SIGHUP:
		case SIGQUIT:
#endif
			Com_Shutdown("Received signal %d, quitting...\n", s);
			break;
		default:
			Com_Error(ERROR_FATAL, "Received signal %d\n", s);
			break;
	}
}
