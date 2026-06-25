/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2013-2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// iCloud key-value store backing for saved games on tvOS.

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_TV

#include "bstone_icloud.h"

#import <Foundation/Foundation.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

// Key prefix for saved-game blobs in the iCloud key-value store.
NSString* const k_save_key_prefix = @"sg.";

// NSUbiquitousKeyValueStore caps a single value at 1 MiB.
constexpr std::size_t k_max_value_size = 1024u * 1024u;

NSString* make_key_from_path(const std::string& path)
{
	const auto slash = path.find_last_of("/\\");
	const auto base = (slash == std::string::npos) ? path : path.substr(slash + 1);
	return [k_save_key_prefix stringByAppendingString:[NSString stringWithUTF8String:base.c_str()]];
}

bool read_file(const std::string& path, std::vector<unsigned char>& out)
{
	std::FILE* file = std::fopen(path.c_str(), "rb");

	if (file == nullptr)
	{
		return false;
	}

	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);

	if (size <= 0)
	{
		std::fclose(file);
		return false;
	}

	out.resize(static_cast<std::size_t>(size));
	const std::size_t read = std::fread(out.data(), 1, out.size(), file);
	std::fclose(file);
	return read == out.size();
}

bool write_file(const std::string& path, const void* data, std::size_t size)
{
	std::FILE* file = std::fopen(path.c_str(), "wb");

	if (file == nullptr)
	{
		return false;
	}

	const std::size_t written = std::fwrite(data, 1, size, file);
	std::fclose(file);
	return written == size;
}

bool file_exists(const std::string& path)
{
	std::FILE* file = std::fopen(path.c_str(), "rb");

	if (file == nullptr)
	{
		return false;
	}

	std::fclose(file);
	return true;
}

} // namespace

namespace bstone {
namespace icloud {

void push_save_file(const std::string& path)
{
	@autoreleasepool
	{
		std::vector<unsigned char> bytes;

		if (!read_file(path, bytes))
		{
			return;
		}

		if (bytes.size() > k_max_value_size)
		{
			NSLog(@"[icloud] save '%s' is %zu bytes (over the 1 MB KVS limit); not synced.",
				path.c_str(), bytes.size());
			return;
		}

		NSData* const data = [NSData dataWithBytes:bytes.data() length:bytes.size()];
		NSUbiquitousKeyValueStore* const store = [NSUbiquitousKeyValueStore defaultStore];
		[store setData:data forKey:make_key_from_path(path)];
		[store synchronize];
	}
}

void pull_save_file_if_missing(const std::string& path)
{
	@autoreleasepool
	{
		if (file_exists(path))
		{
			return;
		}

		NSData* const data = [[NSUbiquitousKeyValueStore defaultStore] dataForKey:make_key_from_path(path)];

		if (data == nil || data.length == 0)
		{
			return;
		}

		write_file(path, data.bytes, static_cast<std::size_t>(data.length));
	}
}

void restore_saves(const std::string& profile_dir)
{
	@autoreleasepool
	{
		NSUbiquitousKeyValueStore* const store = [NSUbiquitousKeyValueStore defaultStore];
		[store synchronize]; // pull the latest values from iCloud

		NSDictionary<NSString*, id>* const all = [store dictionaryRepresentation];

		for (NSString* key in all)
		{
			if (![key hasPrefix:k_save_key_prefix])
			{
				continue;
			}

			id value = all[key];

			if (![value isKindOfClass:[NSData class]])
			{
				continue;
			}

			NSData* const data = (NSData*)value;
			NSString* const base = [key substringFromIndex:k_save_key_prefix.length];
			const std::string local_path = profile_dir + std::string([base UTF8String]);

			// Keep an existing local copy; only restore what tvOS purged.
			if (file_exists(local_path))
			{
				continue;
			}

			write_file(local_path, data.bytes, static_cast<std::size_t>(data.length));
		}
	}
}

} // namespace icloud
} // namespace bstone

#endif // __APPLE__ && TARGET_OS_TV
