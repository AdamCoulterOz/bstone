/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// Process utils.

#include "bstone_process.h"

#include <cstdio>
#include <string>

#include "bstone_globals.h"

namespace bstone {
namespace process {

namespace {

// True if the string already carries a URL scheme (a "://" appearing before any
// path separator, e.g. "https://..." or "file://...").
bool has_url_scheme(const char* text) noexcept
{
	for (const char* it = text; *it != '\0'; ++it)
	{
		if (it[0] == ':' && it[1] == '/' && it[2] == '/')
		{
			return true;
		}

		if (*it == '/' || *it == '\\')
		{
			break;
		}
	}

	return false;
}

// Turns a local filesystem path into the "file:///full/path" URL that the system
// "open URL" service wants for local files: reserved bytes are percent-encoded
// (e.g. the space in "Application Support"), Windows back-slashes become forward
// slashes, and a drive path ("C:\...") gains the leading slash of "file:///C:/...".
//
// The path is expected to be absolute (the only caller passes an absolute
// profile-dir path); a relative path would be rooted at "/", not the CWD.
std::string path_to_file_url(const char* path)
{
	auto normalized = std::string{path};

	for (auto& ch : normalized)
	{
		if (ch == '\\')
		{
			ch = '/';
		}
	}

	auto url = std::string{"file://"};

	if (normalized.empty() || normalized.front() != '/')
	{
		url += '/';
	}

	for (const auto ch : normalized)
	{
		const auto byte = static_cast<unsigned char>(ch);
		const auto is_unreserved =
			(byte >= 'A' && byte <= 'Z') ||
			(byte >= 'a' && byte <= 'z') ||
			(byte >= '0' && byte <= '9') ||
			byte == '/' || byte == ':' ||
			byte == '-' || byte == '_' || byte == '.' || byte == '~';

		if (is_unreserved)
		{
			url += static_cast<char>(byte);
		}
		else
		{
			char buffer[4];
			std::snprintf(buffer, sizeof(buffer), "%%%02X", byte);
			url += buffer;
		}
	}

	return url;
}

} // namespace

void open_file_or_url(const char* file_or_url)
{
	if (file_or_url == nullptr || file_or_url[0] == '\0')
	{
		return;
	}

	auto& system_mgr = globals::sys_system_mgr;

	if (system_mgr == nullptr)
	{
		return;
	}

	// The actual "open" lives in the sys backend; here we only turn a local path
	// into the file:// URL that service expects.
	if (has_url_scheme(file_or_url))
	{
		system_mgr->open_url(file_or_url);
	}
	else
	{
		system_mgr->open_url(path_to_file_url(file_or_url).c_str());
	}
}

} // namespace process
} // namespace bstone
