/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2011  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef COMPAT_H
#define COMPAT_H

// Set BOOST_FILEYSTEM_VERSION to 2 since boost-1.46 defaults to 3
#define BOOST_FILESYSTEM_VERSION 2

//Define cross platform helpers
// TODO: This should be reworked to use CMake feature detection where possible

// gettext support
#include <locale.h>
#include <libintl.h>
#include <stddef.h>
#include <assert.h>
#define _(STRING) gettext(STRING)

#include <cstdlib>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <intrin.h>
#undef min
#undef max
#undef RGB
#undef exception_info // Let's hope MS functions always use _exception_info
#define snprintf _snprintf
#define isnan _isnan

// No real functionality for now
typedef int pid_t;

// WINTODO: Hopefully, the MSVC instrinsics are similar enough
//          to what the standard mandates
#ifdef _MSC_VER

#define TLSDATA __declspec( thread )

#include <malloc.h>
inline int aligned_malloc(void **memptr, std::size_t alignment, std::size_t size)
{
	*memptr = _aligned_malloc(size, alignment);
	return (*memptr != NULL) ? 0: -1;
}
inline void aligned_free(void *mem)
{
	_aligned_free(mem);
}

// Emulate these functions
int round(double f);
long lrint(double f);


// WINTODO: Should be set by CMake?
#define PATH_MAX 260
#define DATADIR "."
#define GNASH_PATH "NONEXISTENT_PATH_GNASH_SUPPORT_DISABLED"

#else
#error At the moment, only Visual C++ is supported on Windows
#endif


#else //GCC
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#define TLSDATA __thread
#define CALLBACK

//Support both atomic header ( gcc >= 4.6 ), and earlier ( stdatomic.h )
#ifdef HAVE_ATOMIC
#include <atomic>
#else
#include <cstdatomic>
#endif

//Boolean type con acquire release barrier semantics
#define ACQUIRE_RELEASE_FLAG(x) std::atomic_bool x
#define ACQUIRE_READ(x) x.load(std::memory_order_acquire)
#define RELEASE_WRITE(x, v) x.store(v, std::memory_order_release)
int aligned_malloc(void **memptr, std::size_t alignment, std::size_t size);
void aligned_free(void *mem);
#endif

#include <iostream>

#if defined _WIN32 || defined __CYGWIN__
// No DLLs, for now
#   define DLL_PUBLIC
#	define DLL_LOCAL
#else
	#if __GNUC__ >= 4
		#define DLL_PUBLIC __attribute__ ((visibility("default")))
		#define DLL_LOCAL  __attribute__ ((visibility("hidden")))
	#else
		#error GCC version less than 4
	#endif
#endif

#ifndef WIN32
  #define HMODULE void *
#endif

/***********
Used for compatibility for loading library between Windows and POSIX
************/
HMODULE LoadLib(const std::string filename);

void *ExtractLibContent(HMODULE hLib, std::string WhatToExtract);

void CloseLib(HMODULE hLib);
/*****************/

template<class T>
inline T minTmpl(T a, T b)
{
	return (a<b)?a:b;
}
template<class T>
inline T maxTmpl(T a, T b)
{
	return (a>b)?a:b;
}
#define imin minTmpl<int>
#define imax maxTmpl<int>
#define dmin minTmpl<double>
#define dmax maxTmpl<double>

#include <cstdint>
#include <sys/types.h>
std::uint64_t compat_msectiming();
void compat_msleep(unsigned int time);
std::uint64_t compat_get_current_time_ms();
std::uint64_t compat_get_current_time_us();
std::uint64_t compat_get_thread_cputime_us();

int kill_child(pid_t p);

#endif
