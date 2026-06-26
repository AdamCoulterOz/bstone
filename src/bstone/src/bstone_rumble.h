/*
BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
SPDX-License-Identifier: GPL-2.0-or-later
*/

//
// Apple TV controller rumble (haptics).
//
// Game code calls the semantic event functions below; the intensity/envelope of
// each lives in bstone_rumble.cpp (one place to tune the "feel"). A per-frame
// update() mixes active one-shots + continuous channels and drives the controller
// through the sys event manager (SDL_GameControllerRumble). It is a harmless no-op
// when no haptic controller is open (e.g. desktop, or no gamepad on tvOS).
//

#ifndef BSTONE_RUMBLE_INCLUDED
#define BSTONE_RUMBLE_INCLUDED

namespace bstone {
namespace rumble {

// Per-frame tick (call once in the play loop, passing the frame's elapsed tics).
void update(int tics);

// Clear all rumble immediately. (Used at the teleport-warp exit; other transitions
// such as pause / level load rely on the 200ms auto-off built into update().)
void stop();

// --- One-shot events ---------------------------------------------------------
void weapon_light_shot();   // Auto-Charge Pistol: short sharp bump (per shot)
void weapon_heavy_shot();   // Slow-Fire Protector: bigger thump (per shot)
void weapon_blast();        // Plasma Discharge Unit: heavy initial thump
void player_hit(int damage);// projectile/discrete damage, scaled by amount
void player_death();        // sudden heavy rumble, fast fade
void pushwall_open();       // a secret wall panel starts moving: heavy thump
void door_open();           // very light bump
void item_basic();          // basic pickup (ammo/health/key): very light bump
void loot(int tier);        // 0=money bag, 1=loot, 2=gold bars, 3=Xylan orb

// --- Continuous events (call EVERY frame the condition holds; auto-fades) -----
void weapon_auto_light();   // Rapid Assault Weapon: light continuous while held
void weapon_auto_medium();  // Dual Neutron Disruptor: medium continuous while held
void weapon_blast_tail();   // Plasma Discharge Unit: light sustained tail while held
void contact();             // touching a Plasma Sphere / arc shield / barrier
void wall_search();         // pushing against a solid wall with Use held

// --- Teleporter --------------------------------------------------------------
// Sine-modulated rumble, driven directly from the (blocking) warp loop.
void teleport_tick(int elapsed_tics, int period_tics);

} // namespace rumble
} // namespace bstone

#endif // BSTONE_RUMBLE_INCLUDED
