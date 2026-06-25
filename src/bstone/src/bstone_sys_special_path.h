/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2023-2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// Special path primitives.

#ifndef BSTONE_SYS_SPECIAL_PATH_INCLUDED
#define BSTONE_SYS_SPECIAL_PATH_INCLUDED

#include <cstdint>

namespace bstone {
namespace sys {

struct SpecialPath
{
	/*
	Get the user-specific path where files can be written.

	Params:
	  - organization_name - organization name (optional, could be null).
	  - application_name - application name (required).
	  - buffer - a buffer to store a null-terminated path.
	  - buffer_size - buffer size.

	Returns:
	  Path length without null symbol.
	*/
	static std::intptr_t get_user_specific_data_path(
		const char* organization_name,
		const char* application_name,
		char* buffer,
		std::intptr_t buffer_size);

	/*
	Get the read-only base path where the application and its bundled resources
	live (on Apple bundles this is the .app resource directory).

	Params:
	  - buffer - a buffer to store a null-terminated path.
	  - buffer_size - buffer size.

	Returns:
	  Path length without null symbol.
	*/
	static std::intptr_t get_base_path(char* buffer, std::intptr_t buffer_size);
};

} // namespace sys
} // namespace bstone

#endif // BSTONE_SYS_SPECIAL_PATH_INCLUDED
