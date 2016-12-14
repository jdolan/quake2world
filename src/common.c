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

#include "common.h"

static const char *DEBUG_CATEGORIES[] = {
	"ai",
	"cgame",
	"client",
	"collision",
	"console",
	"filesystem",
	"game",
	"net",
	"pmove",
	"renderer",
	"server",
	"sound"
};

/**
 * @return A string containing all enabled debug categories.
 */
const char *Com_GetDebug(void) {
	static char debug[MAX_STRING_CHARS];

	debug[0] = '\0';

	for (size_t i = 0; i < lengthof(DEBUG_CATEGORIES); i++) {
		if (quetoo.debug_mask & (1 << i)) {
			if (i > 0) {
				g_strlcat(debug, " ", sizeof(debug));
			}
			g_strlcat(debug, DEBUG_CATEGORIES[i], sizeof(debug));
		}
	}

	if (quetoo.debug_mask & DEBUG_BREAKPOINT) {
		g_strlcat(debug, " breakpoint", sizeof(debug));
	}

	return debug;
}

/**
 * @brief Parses a debug string and sets up the quetoo.debug value
 */
void Com_SetDebug(const char *debug) {

	const char *buf = debug;
	while (true) {

		const char *c = ParseToken(&buf);

		if (*c == '\0') {
			break;
		}

		if (!g_strcmp0(c, "none") || !g_strcmp0(c, "0")) {
			quetoo.debug_mask = 0;
		} else if (!g_strcmp0(c, "breakpoint") || !g_strcmp0(c, "bp")) {
			quetoo.debug_mask ^= DEBUG_BREAKPOINT;
		} else if (!g_strcmp0(c, "any") || !g_strcmp0(c, "all")) {
			quetoo.debug_mask ^= DEBUG_ALL;
		} else {
			for (size_t i = 0; i < lengthof(DEBUG_CATEGORIES); i++) {
				if (!g_strcmp0(c, DEBUG_CATEGORIES[i])) {
					quetoo.debug_mask ^= (1 << i);
				}
			}
		}
	}
}

/**
 * @brief Print a debug statement. If the format begins with '!', the function
 * name is omitted.
 */
void Com_Debug_(const debug_t debug, const char *func, const char *fmt, ...) {

	if ((quetoo.debug_mask & debug) == 0) {
		return;
	}

	char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
		fmt++;
	}

	const size_t len = strlen(msg);

	va_list args;
	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	OutputDebugString(msg);
#endif

	if (quetoo.Debug) {
		quetoo.Debug(debug, (const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/**
 * @brief An error condition has occurred. This function does not return.
 */
void Com_Error_(const char *func, err_t err, const char *fmt, ...) {

	if (quetoo.recursive_error) {

		if (quetoo.Error) {
			quetoo.Error(err, (const char *) "Recursive error\n");
		} else {
			fputs("Recursive error\n", stderr);
			fflush(stderr);
			exit(err);
		}
	} else {
		quetoo.recursive_error = true;
	}

	char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
		fmt++;
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	OutputDebugString(msg);
#endif

	if (quetoo.Error) {
		quetoo.Error(err, (const char *) msg);
	} else {
		fputs(msg, stderr);
		fflush(stderr);
		exit(err);
	}

	quetoo.recursive_error = false;
}

/**
 * @brief
 */
void Com_Print(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	OutputDebugString(msg);
#endif

	if (quetoo.Print) {
		quetoo.Print((const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/**
 * @brief Prints a warning message.
 */
void Com_Warn_(const char *func, const char *fmt, ...) {

	static char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
		fmt++;
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	OutputDebugString(msg);
#endif

	if (quetoo.Warn) {
		quetoo.Warn((const char *) msg);
	} else {
		fprintf(stderr, "WARNING: %s", msg);
		fflush(stderr);
	}
}

/**
 * @brief
 */
void Com_Verbose(const char *fmt, ...) {
	va_list args;
	static char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	OutputDebugString(msg);
#endif

	if (quetoo.Verbose) {
		quetoo.Verbose((const char *) msg);
	} else {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

/**
 * @brief Initializes the global arguments list and dispatches to an underlying
 * implementation, if provided. Should be called shortly after program
 * execution begins.
 */
void Com_Init(int32_t argc, char *argv[]) {

	quetoo.argc = argc;
	quetoo.argv = argv;

	if (quetoo.Init) {
		quetoo.Init();
	}
}

/**
 * @brief Program exit point under normal circumstances. Dispatches to a
 * specialized implementation, if provided, or simply prints the message and
 * exits. This function does not return.
 */
void Com_Shutdown(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	fmt = fmt ? : ""; // normal shutdown will actually pass NULL

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quetoo.Shutdown) {
		quetoo.Shutdown(msg);
	} else {
		Com_Print("%s", msg);
	}

	exit(0);
}

/**
 * @brief
 */
uint32_t Com_WasInit(uint32_t s) {
	return quetoo.subsystems & s;
}

/**
 * @brief
 */
void Com_InitSubsystem(uint32_t s) {
	quetoo.subsystems |= s;
}

/**
 * @brief
 */
void Com_QuitSubsystem(uint32_t s) {
	quetoo.subsystems &= ~s;
}



/**
 * @brief Returns the command line argument count.
 */
int32_t Com_Argc(void) {
	return quetoo.argc;
}

/**
 * @brief Returns the command line argument at the specified index.
 */
char *Com_Argv(int32_t arg) {
	if (arg < 0 || arg >= Com_Argc()) {
		return "";
	}
	return quetoo.argv[arg];
}

/**
 * @brief
 */
void Com_PrintInfo(const char *s) {
	char key[512];
	char value[512];
	char *o;
	intptr_t l;

	if (*s == '\\') {
		s++;
	}
	while (*s) {
		o = key;
		while (*s && *s != '\\') {
			*o++ = *s++;
		}

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else {
			*o = 0;
		}
		Com_Print("%s", key);

		if (!*s) {
			Com_Print("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\') {
			*o++ = *s++;
		}
		*o = 0;

		if (*s) {
			s++;
		}
		Com_Print("%s\n", value);
	}
}
