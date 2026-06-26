/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2013-2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// Event manager.

#include "bstone_sys_event_mgr.h"

namespace bstone {
namespace sys {

EventMgr::EventMgr() = default;

EventMgr::~EventMgr() = default;

bool EventMgr::is_initialized() const noexcept
{
	return do_is_initialized();
}

bool EventMgr::poll_event(Event& e)
{
	return do_poll_event(e);
}

void EventMgr::set_rumble(std::uint16_t low_frequency, std::uint16_t high_frequency, std::uint32_t duration_ms)
{
	do_set_rumble(low_frequency, high_frequency, duration_ms);
}

} // namespace sys
} // namespace bstone
