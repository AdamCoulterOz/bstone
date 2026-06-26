/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bstone_rumble.h"

#include <array>
#include <cmath>
#include <cstdint>

#include "bstone_globals.h"
#include "bstone_sys_event_mgr.h"

namespace bstone {
namespace rumble {
namespace {

constexpr int k_max = 65535;     // SDL rumble per-motor max
constexpr int k_tic_rate = 70;   // game tics per second

// Continuous channels (mixed additively each frame; refreshed by the *_continuous
// events and auto-faded when no longer refreshed).
enum { ch_weapon, ch_contact, ch_search, ch_count };

// A one-shot effect: optional start delay, then a linear ramp from (low0,high0)
// to (low1,high1) over dur tics. A flat bump has low1/high1 == low0/high0; a
// tail-off has them at 0.
struct Shot
{
	int delay;
	int low0, high0;
	int low1, high1;
	int t, dur;
	bool active;
};

struct Chan
{
	int low, high;
	int hold; // tics remaining before the channel auto-clears
};

std::array<Shot, 12> g_shots{};
std::array<Chan, ch_count> g_chans{};
bool g_idle = true; // true when the last value pushed to the controller was zero

int pct(int p) noexcept
{
	return (p * k_max) / 100;
}

int ms2t(int ms) noexcept
{
	const int t = ((ms * k_tic_rate) + 500) / 1000;
	return t < 1 ? 1 : t;
}

void add_shot(int delay_t, int low0, int high0, int low1, int high1, int dur_t)
{
	if (dur_t < 1)
	{
		dur_t = 1;
	}

	const Shot ns{delay_t, low0, high0, low1, high1, 0, dur_t, true};

	for (auto& s : g_shots)
	{
		if (!s.active)
		{
			s = ns;
			return;
		}
	}

	g_shots[0] = ns; // table full (rare): replace the oldest slot
}

void flat(int low, int high, int dur_ms)
{
	add_shot(0, low, high, low, high, ms2t(dur_ms));
}

void fall(int low, int high, int dur_ms)
{
	add_shot(0, low, high, 0, 0, ms2t(dur_ms));
}

void refresh_chan(int ch, int low, int high)
{
	auto& c = g_chans[static_cast<std::size_t>(ch)];
	c.low = low;
	c.high = high;
	c.hold = ms2t(80); // stays alive ~80ms past the last refresh -> smooth release
}

} // namespace

void update(int tics)
{
	if (tics < 1)
	{
		tics = 1;
	}

	long low = 0;
	long high = 0;

	for (auto& s : g_shots)
	{
		if (!s.active)
		{
			continue;
		}

		if (s.delay > 0)
		{
			s.delay -= tics;
			continue;
		}

		s.t += tics;
		const int tt = s.t > s.dur ? s.dur : s.t;
		low += s.low0 + (static_cast<long>(s.low1 - s.low0) * tt) / s.dur;
		high += s.high0 + (static_cast<long>(s.high1 - s.high0) * tt) / s.dur;

		if (s.t >= s.dur)
		{
			s.active = false;
		}
	}

	for (auto& c : g_chans)
	{
		if (c.hold <= 0)
		{
			c.low = 0;
			c.high = 0;
			continue;
		}

		c.hold -= tics;
		low += c.low;
		high += c.high;
	}

	if (low > k_max)
	{
		low = k_max;
	}

	if (high > k_max)
	{
		high = k_max;
	}

	auto* const mgr = globals::sys_event_mgr;

	if (mgr == nullptr)
	{
		return;
	}

	if (low == 0 && high == 0)
	{
		if (!g_idle)
		{
			mgr->set_rumble(0, 0, 0);
			g_idle = true;
		}
	}
	else
	{
		// Re-issued every frame; 200ms covers the gap so continuous effects never
		// lapse. SDL's Core Haptics backend updates the live intensity smoothly.
		mgr->set_rumble(static_cast<std::uint16_t>(low), static_cast<std::uint16_t>(high), 200);
		g_idle = false;
	}
}

void stop()
{
	g_shots = {};
	g_chans = {};

	auto* const mgr = globals::sys_event_mgr;

	if (mgr != nullptr && !g_idle)
	{
		mgr->set_rumble(0, 0, 0);
	}

	g_idle = true;
}

// --- one-shots ---------------------------------------------------------------
void weapon_light_shot() { flat(pct(25), pct(35), 60); }
void weapon_heavy_shot() { fall(pct(55), pct(30), 150); }
void weapon_blast()      { fall(pct(90), pct(40), 200); }

void player_hit(int damage)
{
	if (damage < 1)
	{
		damage = 1;
	}

	int p = 30 + (damage * 3);
	if (p > 95)
	{
		p = 95;
	}

	int dur = 100 + (damage * 6);
	if (dur > 280)
	{
		dur = 280;
	}

	fall(pct(p), pct((p * 2) / 3), dur);
}

void player_death() { fall(pct(100), pct(60), 700); }
void pushwall_open() { fall(pct(80), pct(30), 220); }
void door_open()     { flat(pct(15), pct(12), 70); }
void item_basic()    { flat(pct(12), pct(18), 55); }

void loot(int tier)
{
	switch (tier)
	{
	case 0: // money bag: single tick
		flat(pct(20), pct(30), 50);
		break;

	case 1: // loot: double tick
		flat(pct(22), pct(32), 45);
		add_shot(ms2t(90), pct(22), pct(32), pct(22), pct(32), ms2t(45));
		break;

	case 2: // gold bars: ascending triple
		add_shot(0, pct(20), pct(24), pct(20), pct(24), ms2t(45));
		add_shot(ms2t(80), pct(30), pct(30), pct(30), pct(30), ms2t(45));
		add_shot(ms2t(160), pct(45), pct(38), pct(45), pct(38), ms2t(60));
		break;

	default: // Xylan orb: jackpot bloom (thump + shimmering tail)
		fall(pct(70), pct(45), 120);
		add_shot(ms2t(120), pct(15), pct(50), 0, 0, ms2t(380));
		break;
	}
}

// --- continuous (refresh each frame the condition holds) ---------------------
void weapon_auto_light()  { refresh_chan(ch_weapon, pct(18), pct(25)); }
void weapon_auto_medium() { refresh_chan(ch_weapon, pct(45), pct(30)); }
void weapon_blast_tail()  { refresh_chan(ch_weapon, pct(20), pct(12)); }
void contact()            { refresh_chan(ch_contact, pct(20), pct(15)); }
void wall_search()        { refresh_chan(ch_search, pct(10), pct(8)); }

// --- teleporter (sine), driven directly from the blocking warp loop ----------
void teleport_tick(int elapsed_tics, int period_tics)
{
	if (period_tics < 1)
	{
		period_tics = 1;
	}

	const double ph = (2.0 * 3.14159265358979 * (elapsed_tics % period_tics)) / period_tics;
	const double s = 0.5 - (0.5 * std::cos(ph)); // 0 -> 1 -> 0

	auto* const mgr = globals::sys_event_mgr;

	if (mgr != nullptr)
	{
		mgr->set_rumble(
			static_cast<std::uint16_t>(pct(50) * s),
			static_cast<std::uint16_t>(pct(30) * s),
			120);
		g_idle = false;
	}
}

} // namespace rumble
} // namespace bstone
