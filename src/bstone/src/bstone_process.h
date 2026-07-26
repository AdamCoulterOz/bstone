/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// Process utils.

#ifndef BSTONE_PROCESS_INCLUDED
#define BSTONE_PROCESS_INCLUDED

namespace bstone {
namespace process {

// Opens a file or URL in the system-provided default application (a browser for
// http(s), a viewer/editor for a local file, a file manager for a directory, ...).
//
// Best-effort and sandbox-safe: it delegates to the sys layer's open-URL service,
// so bstone itself never spawns a child process. A local filesystem path is accepted
// and turned into a file:// URL; anything already carrying a scheme is passed through.
void open_file_or_url(const char* file_or_url);

} // namespace process
} // namespace bstone

#endif // BSTONE_PROCESS_INCLUDED
