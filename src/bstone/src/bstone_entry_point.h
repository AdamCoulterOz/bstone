/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2013-2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

#ifndef BSTONE_ENTRY_POINT_INCLUDED
#define BSTONE_ENTRY_POINT_INCLUDED

#if defined(__APPLE__)
	#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
	#ifndef BSTONE_ENTRY_POINT_IMPLEMENTATION
		#undef main
		#define main bstone_entry_point
	#endif

	extern int bstone_entry_point(int argc, char** argv);
#elif defined(__APPLE__) && TARGET_OS_TV
	// tvOS: the real OS entry point lives in bstone_entry_point.cpp and hands
	// control to SDL's UIKit application delegate via SDL_UIKitRunApp(). The
	// game's own main() (in 3d_main.cpp) is renamed so that it becomes the
	// SDL_main-style callback that SDL invokes once the run loop is up.
	#ifndef BSTONE_ENTRY_POINT_IMPLEMENTATION
		#undef main
		#define main bstone_entry_point
	#endif

	extern int bstone_entry_point(int argc, char** argv);
#endif

#endif // BSTONE_ENTRY_POINT_INCLUDED
