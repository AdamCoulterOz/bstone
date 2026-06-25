/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2013-2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// iCloud persistence for saved games (tvOS uses NSUbiquitousKeyValueStore).
//
// tvOS has no durable local storage and no iCloud Drive document container, so
// saved games are mirrored into the iCloud key-value store (per the signed-in
// Apple TV profile). Each save file is stored as one value keyed by its base
// name; the engine keeps writing/reading ordinary files and these helpers mirror
// them to/from iCloud.

#ifndef BSTONE_ICLOUD_INCLUDED
#define BSTONE_ICLOUD_INCLUDED

#include <string>

namespace bstone {
namespace icloud {

// Mirror a just-written save file to iCloud (keyed by its base name). No-op if
// iCloud is unavailable or the file is too large for the key-value store.
void push_save_file(const std::string& path);

// If the local save file is missing, restore it from iCloud.
void pull_save_file_if_missing(const std::string& path);

// Restore every iCloud-stored save into profile_dir whose local copy is missing.
// Call once at startup so purged saves reappear (and saves from other devices
// signed into the same Apple TV profile show up).
void restore_saves(const std::string& profile_dir);

} // namespace icloud
} // namespace bstone

#endif // BSTONE_ICLOUD_INCLUDED
