/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
Copyright (c) 2013-2024 Boris I. Bendovsky (bibendovsky@hotmail.com) and Contributors
SPDX-License-Identifier: MIT
*/

// Event manager.

#ifndef BSTONE_SYS_EVENT_MGR_INCLUDED
#define BSTONE_SYS_EVENT_MGR_INCLUDED

#include <cstdint>
#include <memory>

#include "bstone_sys_event.h"

namespace bstone {
namespace sys {

class EventMgr
{
public:
	EventMgr();
	virtual ~EventMgr();

	bool is_initialized() const noexcept;

	bool poll_event(Event& e);

	// Drive controller rumble. low_frequency / high_frequency are 0..65535 (SDL's
	// low- and high-frequency motors); duration_ms is how long to run. No-op when
	// no controller is open.
	void set_rumble(std::uint16_t low_frequency, std::uint16_t high_frequency, std::uint32_t duration_ms);

private:
	virtual bool do_is_initialized() const noexcept = 0;

	virtual bool do_poll_event(Event& e) = 0;

	virtual void do_set_rumble(
		std::uint16_t low_frequency, std::uint16_t high_frequency, std::uint32_t duration_ms) = 0;
};

// ==========================================================================

using EventMgrUPtr = std::unique_ptr<EventMgr>;

} // namespace sys
} // namespace bstone

#endif // BSTONE_SYS_EVENT_MGR_INCLUDED
