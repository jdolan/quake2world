#if !defined(__clang__)
#ifndef __attribute__
#define __attribute__(...)

typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int32 int64_t;
typedef unsigned __int32 uint64_t;
#endif

#ifndef __thread
#define __thread __declspec(thread)
#endif
#endif

#if defined(__clang__)
#define stat _stat64
#define unlink _unlink
#define S_IFDIR _S_IFDIR

#define isascii __isascii
#define itoa _itoa
#define getpid _getpid
#define strdup _strdup
#endif

#ifdef _WIN64
    typedef signed __int64 ssize_t;
#else
    typedef signed int     ssize_t;
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR)
#endif

#ifndef usleep
#define usleep(t) Sleep(t / 1000)
#endif

#ifdef _WIN64
    typedef signed __int64 ssize_t;
#else
    typedef signed int     ssize_t;
#endif

#define _WINSOCKAPI_

/* Set to the canonical name of the target machine
 */
#define _strngf(s) #s
#define _strngfx(s) _strngf(s)

#if defined(__clang__)
#define BUILD_MACHINE "Clang " __clang_version__
#elif defined(_MSC_VER)
#define BUILD_MACHINE "MSC " _strngfx(_MSC_VER)
#endif

#if defined(_WIN64)
#define BUILD_HOST "Win64 " BUILD_MACHINE
#elif defined(_WIN32)
#define BUILD_HOST "Win32 " BUILD_MACHINE
#else
#define BUILD_HOST "Windows " BUILD_MACHINE
#endif

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "quetoo"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "jay@jaydolan.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "quetoo"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "quetoo 0.1.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "quetoo"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.1.0"

/* Define to path containing the game data.
 */
#define PKGDATADIR "/share"

/* Define to path containing the shared modules.
 */
#define PKGLIBDIR "/lib"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.1.0"

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

#undef HAVE_UNISTD_H