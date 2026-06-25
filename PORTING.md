# Porting bstone (Blake Stone) to Apple tvOS

**bstone** is a single-executable C++14 source port of *Blake Stone* built on SDL2 as its sole OS abstraction. This document is an engineering plan for getting it running on **Apple TV** with **MFi/standard game-controller** support. The work is favourable because the vendored SDL 2.32.10 already contains a complete, first-class tvOS layer — most of the remaining work is bstone-side integration, input, and a build/packaging shell, not writing OS backends.

**Scope and constraints (read this first).** This is a **local sideload experiment**, not an App Store release.

- **Distribution:** sideload to the author's own Apple TVs via a **paid Apple Developer account**; devices are already paired in Xcode on this Mac. No App Store, no TestFlight requirement, no App Review.
- **Deployment target:** **tvOS 26**.
- **Hardware:** Apple TV 4K **gen 1 (A10X)** and **gen 2 (A12)**, both arm64 — **one device build covers both**. Performance is a non-issue for a 320x200 raycaster.
- **Input:** an **MFi/standard gamepad is required for gameplay**; the **Siri Remote only needs to drive menus**.
- **Renderer:** **software-renderer-first** (`vid_renderer=software`; SDL_Renderer is Metal-backed on tvOS). Vulkan-via-MoltenVK is an **optional later path**. The GL/GLES hardware paths are **dead/deprecated on tvOS** and out of scope.
- **Build:** CMake builds SDL2 + the engine; a thin hand-authored **Xcode tvOS app shell** owns `Info.plist`, signing, icon, and deploy.
- **Storage:** redirect `get_profile_dir()` to the app's **Documents container**; accept tvOS purge risk and **skip iCloud durability work**.

Because this is a personal sideload, App-Store-only concerns are explicitly **dropped**: IP/licensing (5.2.1), IAP-vs-license-key (3.1.1), download-size disclosure (4.2.3), TestFlight, mandatory layered icon / Top Shelf assets, and the "must be playable with the Siri Remote" review rule do not apply.

---

## Engine architecture

A modern `bstone::` C++ rewrite (~516 `bstone_*` files) wraps the legacy id-software layer (`id_*`), the 3D game logic (`3d_*`), and the Jim Mellander modules (`jm_*`).

**Entry point and main loop.** The real cross-platform entry is `int main(int argc, char* argv[])` at `src/bstone/src/3d_main.cpp:9931`. Critically, the game disables SDL's main hijacking with `#define SDL_MAIN_HANDLED` at `src/bstone/src/3d_main.cpp:8`, so it owns its own `main()` on every non-Windows platform and **never uses `SDL_main`**. Windows uses a separate entry (`src/bstone/src/bstone_entry_point.cpp` defines `main()`/`WinMain()`; `src/bstone/src/bstone_entry_point.h` aliases the game's `main` to `bstone_entry_point` — a no-op elsewhere). `main()` boots the logger, the cvar/ccmd managers, an MT task manager, and the SDL system manager via `bstone::sys::make_system_mgr()`, then runs `freed_main()` (`src/bstone/src/jm_free.cpp:1431`) and finally enters `DemoLoop()` (`src/bstone/src/3d_main.cpp:9706`).

`freed_main()` does the heavy boot: cvar/ccmd registration, `InitDestPath()` + `find_contents()` (path/asset resolution), `ReadConfig()`, then `InitGame()` (`jm_free.cpp:1289`) which brings up subsystems in order: `CA_Startup` (assets), `sd_startup` (audio), `IN_Startup` (input/events), `VW_Startup` (video), `US_Startup` (UI).

The loop is **three nested blocking levels**:

- `DemoLoop()` (`3d_main.cpp:9706`) — outermost attract sequence + menu (`US_ControlPanel`).
- `GameLoop()` (`src/bstone/src/3d_game.cpp:3401`) — per-level: load level, draw, call `PlayLoop()`, handle transitions/death/restart.
- `PlayLoop()` (`src/bstone/src/3d_play.cpp:2006`) — the real-time per-frame loop: `PollControls()` → actor AI → `UpdatePaletteShifts()` → `ThreeDRefresh()` (render + present) → `CheckKeys()`.

Frame pacing is a fixed **70 Hz tick** (`TickBase` in `id_heads.h:34`, the original VGA timer rate) via `CalcTics()` (`src/bstone/src/3d_draw.cpp:1472`), which accumulates elapsed time with `std::chrono::steady_clock` and paces each iteration with `std::this_thread::sleep_for`. This is portable (no SDL timers, no timer thread) but is a classic **"engine owns the loop"** model that fights UIKit's run-loop-driven lifecycle. Exit throws `QuitException` from `Quit()` (`3d_main.cpp:9699`), caught in `main`, which runs `pre_quit()` → `ShutdownId()` (`3d_main.cpp:9576`) and tears down the sys managers in reverse, triggering `SDL_Quit`. There is already a `__vita__` special-case in `main()` (clock setup, injected `--ps` argv, 192 MB heap) — **a concrete precedent for per-device bootstrap**.

**The `bstone::sys` SDL2 abstraction.** A clean C++14 NVI layer defines pure-virtual interfaces (`src/bstone/src/bstone_sys_system_mgr.h`, etc.) with exactly one real backend (SDL2) plus null fallbacks. `make_system_mgr()` in `src/bstone/src/bstone_sys_system_mgr_sdl2.cpp` hardcodes `Sdl2SystemMgr`; its ctor calls `SDL_Init(0)` and subsystems init lazily per-manager via a `Sdl2Subsystem` RAII wrapper. Events pump **only** through `Sdl2EventMgr::do_poll_event` (`src/bstone/src/bstone_sys_event_mgr_sdl2.cpp`), wrapping `SDL_PollEvent`. Global singletons live in `src/bstone/src/bstone_globals.h` (`sys_system_mgr`, `sys_event_mgr`, `sys_video_mgr`, `sys_mouse_mgr`, `sys_window_mgr`). Platform branching is **binary WIN32 vs POSIX** (`src/bstone/src/bstone_platform.h`); Apple currently falls through to `BSTONE_POSIX` with no iOS/tvOS distinction.

**Rendering (two tiers).** The top tier is `bstone::Video` (`src/bstone/src/bstone_video.h`) with two implementations: `SwVideo` (CPU rasteriser, blits via SDL_Renderer + streaming b8g8r8a8 textures — `src/bstone/src/bstone_sw_video.cpp`) and `HwVideo` (`src/bstone/src/bstone_hw_video.cpp`). `VL_Startup()` in `src/bstone/src/id_vl.cpp` (~line 1350) reads the `vid_renderer` cvar; anything but `software` tries HW and **silently falls back to SW on any exception**. `VL_RefreshScreen()` (`id_vl.cpp:1748`) presents each frame. HW delegates 3D to the R3R abstraction (`bstone_r3r_*`), whose backend factory (`src/bstone/src/bstone_r3r_mgr.cpp`) dispatches to GL/GLES (`bstone_gl_r3r*`), Vulkan (`bstone_vk_r3r*`), or a debug-only Null backend. All windowing/context/surface goes through SDL2 (`SDL_CreateWindow` with `SDL_WINDOW_OPENGL`/`SDL_WINDOW_VULKAN`/`ALLOW_HIGHDPI` in `src/bstone/src/bstone_sys_window_sdl2.cpp`; `SDL_RenderPresent` for the SW path in `src/bstone/src/bstone_sys_renderer_sdl2.cpp:163`). The GL/GLES/Vulkan tiers are not relevant to the software-first plan.

**Input.** A two-layer keyboard+mouse model. SDL events normalise into a small `Event` union (`src/bstone/src/bstone_sys_event.h`) — keyboard/mouse/window/quit only, **no joystick/controller types**. The legacy `src/bstone/src/id_in.cpp` translates keys to DOS `ScanCode`s into a 128-entry `Keyboard[]` bitset (`NumCodes=128`), accumulates relative mouse deltas, and maps actions via `BindingId` → up to two `ScanCode`s in the `in_bindings` table; `in_is_binding_pressed()` (`id_in.cpp:1270`) just ORs the `Keyboard[]` state of the bound scancodes. Mouse buttons/wheel are already injected as **synthetic scancodes** `sc_mouse_left=0x64 … sc_mouse_wheel_up=0x6A` (`id_in.h`). `PollControls()` (`3d_play.cpp:562`) reads bindings into the globals `controlx` (turn) / `controly` (forward-back) / `strafe_value` / `buttonstate[]`, consumed by `ControlMovement()` (`src/bstone/src/3d_agent.cpp:508`). The single event funnel is `in_handle_events()` (`id_in.cpp:806`).

All joystick/controller/touch SDL events are explicitly **`SDL_DISABLE`d** in `configure_event_types()` (`bstone_sys_system_mgr_sdl2.cpp:~179-231`), as are all `SDL_APP_*` lifecycle events and `SDL_TEXTINPUT`. The only controller code (vita `Translate*`, `control2x`) is dead — gated behind `#if 0`/`__vita__`, and the `Translate*` functions are declared but never defined. There is no vertical look (`PollMouseMove` forces `delta_y=0`). Menus use a separate `IN_ReadControl`/`ControlInfo` (a `Direction dir` + `button0..3`) path; binding capture in `src/bstone/src/3d_menu.cpp` (`binds_draw_menu`, line 1382) reads `LastScan`.

**Audio.** Two backends behind `AudioMixer`, chosen by the `snd_driver` cvar (auto/system/openal). The **system** backend (`src/bstone/src/bstone_system_audio_mixer.cpp`) opens `SDL_OpenAudioDevice` with `AUDIO_F32SYS`, 2 channels, and mixes/decodes synchronously inside the SDL audio callback (`src/bstone/src/bstone_sys_polling_audio_device_sdl2.cpp`). The **openal** backend `dlopen`s `libopenal.so` at runtime (`src/bstone/src/bstone_oal_audio_mixer.cpp` — no Apple name handling, forbidden on tvOS). Auto-detect tries OpenAL first and cleanly falls back to system (`src/bstone/src/id_sd.cpp`). All synthesis (OPL3 via DBOPL/Nuked, AdLib music/SFX, PC-speaker, 7000 Hz PCM upsampling) is pure CPU and OS-independent.

**Filesystem/assets.** Two concerns. Read-only original game data is discovered by `find_contents()` (`jm_free.cpp:626`) probing an ordered path list (custom dir, **working dir = `getcwd()` by default**, profile dir, then Windows-registry-only GOG/Steam) and SHA1-validating files against a hard-coded manifest in the `Assets` class. Resources open via `ca_open_resource` (`src/bstone/src/id_ca.cpp`) joining `data_dir_`/`mod_dir_` + name, with a lowercase fallback for case-sensitive filesystems. Writable state (config, saves, high scores, `bstone_log.txt`, screenshots) all routes through `get_profile_dir()` (`3d_main.cpp:10774`) = `SDL_GetPrefPath("bibendovsky","bstone")` (the only writable-dir source, `src/bstone/src/bstone_sys_special_path_sdl2.cpp`), using a temp-file + `rename_with_overwrite` + POSIX advisory-lock pattern (`src/bstone/src/bstone_file_posix.cpp`). Every path is overridable via CLI (`--data_dir`/`--mod_dir`/`--profile_dir`) — but **tvOS has no command line**, so these must be injected programmatically (as `__vita__` already injects `--ps`). The engine **never** consults `SDL_GetBasePath` (the bundle) for data.

**Build system.** Multi-level CMake (root `CMakeLists.txt`, project `bstone_solution`, min 3.5.0). The single `add_executable(bstone "")` target (`src/bstone/CMakeLists.txt:120`) attaches ~1000 sources, requires C++14, and special-cases **only WIN32**. SDL2 is selected by `BSTONE_INTERNAL_SDL2` (default OFF): external via `find_package(SDL2W 2.0.4)`, or **bundled SDL 2.32.10** (static-only, `SDL2_DISABLE_SDL2MAIN` forced ON) linking `SDL2::SDL2-static`. There is no `MACOSX_BUNDLE`, no Info.plist, and no Apple/iOS/tvOS branch. Host codegen tools are pre-generated/committed — only `bin2c` runs at build time, and only under `BSTONE_VULKAN_COMPILE_SHADERS` (OFF by default), so a default cross-build runs **no host executables**. The `unsupported/psvita` tree (custom toolchain file, app metadata, `__vita__` ifdefs) is the existing template for a device target.

---

## Feasibility verdict

**Feasible, and meaningfully de-risked by the dependency tree — but it is a real porting project, not a recompile.** For this sideload the effort concentrates in (1) a new build/packaging path, (2) the entry-point reconciliation, (3) controller input, and (4) storage/lifecycle correctness. Renderer choice is already settled (software).

**Single biggest enabler: the bundled SDL 2.32.10 already ships a complete, first-class tvOS layer.** `src/lib/sdl2/src/REVISION.txt` reports `release-2.32.10-0-g5d2495703` (the commit message / changelog text saying "2.32.2" is stale). The vendored tree at `src/lib/sdl2/src/src/` contains:

- **UIKit video driver** — `src/lib/sdl2/src/src/video/uikit/` (13 `.m` files: window, modes, view, view controller, Metal view, OpenGLES/Vulkan views, app delegate, message box).
- **CoreAudio** — `src/lib/sdl2/src/src/audio/coreaudio/SDL_coreaudio.m`.
- **MFi / GameController joystick driver** — `src/lib/sdl2/src/src/joystick/iphoneos/SDL_mfijoystick.m`, with explicit `TARGET_OS_TV` branches, a `GCMicroGamepad` Siri-Remote path, and `CheckControllerSiriRemote()`.
- **UIKit app entry shim** — `src/lib/sdl2/src/src/main/uikit/SDL_uikit_main.c` (defines `main()` → `SDL_UIKitRunApp(argc,argv,SDL_main)` under `__IPHONEOS__ || __TVOS__`, only when `SDL_MAIN_HANDLED` is **not** set), plus `SDL_uikitappdelegate.m`'s `SDL_UIKitRunApp` → `UIApplicationMain`.
- **tvOS-aware config** — `src/lib/sdl2/src/include/SDL_config_iphoneos.h` with `__TVOS__`/`TARGET_OS_TV` branches (CoreAudio, MFi joystick, UIKit video, Metal enabled).

Notably, SDL also maps the Siri Remote's buttons to **keyboard scancodes** in `src/lib/sdl2/src/src/video/uikit/SDL_uikitview.m` (`scancodeFromPress`: Up/Down/Left/Right → arrows, Select → RETURN, Menu → ESCAPE, Play/Pause → PAUSE), so bstone's existing keyboard menu handling can be navigated by the remote with **zero controller code** — which is exactly the menu-only role the remote needs in this plan. bstone does not need to write a UIKit or GameController backend.

**Biggest risks, in priority order:**

1. **Entry-point conflict (architectural, unprototyped).** bstone sets `SDL_MAIN_HANDLED` (`3d_main.cpp:8`) and supplies its own `main()`, which compiles out SDL's UIKit main shim. On tvOS this bypasses `UIApplicationMain`/`SDLUIKitDelegate` entirely — no UIKit run loop, no app delegate. The port **must** add a tvOS branch that either drops `SDL_MAIN_HANDLED` and renames `main`→`SDL_main`, or explicitly calls `SDL_UIKitRunApp(argc, argv, &bstone_main)`. This also requires re-enabling SDL2main, which `BSTONE_INTERNAL_SDL2` currently force-disables.
2. **Blocking loop vs the UIKit run loop.** The three-level `while(true)` loop with `std::this_thread::sleep_for` pacing must coexist with the UIKit/CADisplayLink run loop `SDL_UIKitRunApp` establishes. SDL's Apple backend tolerates a blocking loop, but suspend/resume and OS watchdog behaviour are fragile and unverified.
3. **App lifecycle is absent.** All `SDL_APP_WILLENTERBACKGROUND`/`DIDENTERFOREGROUND`/`TERMINATING`/`LOWMEMORY` events are disabled in `configure_event_types()`. tvOS will background/terminate the app; these must be re-enabled and handled (pause audio, save, yield) or the app will misbehave. New code required.
4. **Input is keyboard/mouse-only.** No controller path is active; mouse-grab logic is entangled with input (`in_handle_mouse_buttons` auto-grabs on first click; focus toggles grab — `id_in.cpp:771-802`). A controller-only build must neutralise these so no action depends on `MousePresent`/grab state. Text entry (save-game naming via `IN_WaitForASCII`) also has no on-screen-keyboard path and `SDL_TEXTINPUT` is disabled.
5. **Storage persistence.** `SDL_GetPrefPath` on tvOS returns `NSCachesDirectory` — **purgeable**; SDL itself logs a critical warning (`src/lib/sdl2/src/src/filesystem/cocoa/SDL_sysfilesystem.m`). Default `data_dir = getcwd()` ≈ `/` in a sandbox, so data discovery fails unless redirected to the bundle. (We accept purge risk per the constraints; we still redirect writes to Documents.)
6. **Build/packaging is green-field.** No Apple CMake/toolchain, Info.plist, or bundle target exists — but for a sideload this is a thin Xcode shell, not full App Store packaging.

---

## Key decisions

All decisions are **resolved** per the project constraints; the table records the chosen option and rationale.

| Decision | Chosen option | Rationale |
|---|---|---|
| **Renderer backend** | **Software** (`vid_renderer=software`) first; MoltenVK optional later | SW needs zero shader/GPU plumbing and SDL maps SDL_Renderer to **Metal** on tvOS — fastest to "it runs". The game renders at 320x200 internal VGA res and upscales, so CPU cost is trivial even at 4K. GL/GLES are dead on tvOS; MoltenVK is future-proof but needs code fixes and is deferred. |
| **Data bundling** | **Bundle the data** as Copy-Bundle-Resources (blue folder reference) | Blake Stone data is single-digit MB — far under any bundle cap, so ODR is needless complexity. Point `data_dir_` at `SDL_GetBasePath` / `[NSBundle].resourcePath` before `find_contents()` runs. Sideload = no IP/licensing review concern. |
| **Controller scope** | **Gamepad required for gameplay; Siri Remote drives menus only** | A twin-stick FPS is poor on the remote alone. The remote's menu role is half-free via SDL's scancode mapping; an `GCExtendedGamepad` MFi pad provides full movement + fire. No App Review "remote must play" rule applies to a sideload. |
| **Build approach** | **Hybrid: CMake builds SDL2 + engine; thin hand-authored Xcode tvOS app shell** owns Info.plist/signing/icon/deploy | CMake is already native to bstone and has no host-tool blocker in the default config. Signing, Info.plist, capabilities, icon, and deploy are far cleaner in a small hand-authored Xcode shell linking the CMake-built static lib. |
| **Save/config storage** | **Redirect `get_profile_dir()` to the Documents container**; accept purge risk, no iCloud | tvOS guarantees no durable local storage, but for a personal sideload the Documents container is good enough; iCloud KVS/CloudKit durability work is explicitly skipped. |

---

## Staged plan

Checkboxes double as a tracking checklist.

### Phase 0 — Build + entry-point reconciliation → blank Metal window

**Goal:** A signed `.app` launches on the paired Apple TV via SDL's UIKit delegate and clears the screen.

**Status: build pipeline complete, and validated by an actual run in the tvOS Simulator (Apple TV 4K, tvOS 26.5).** The whole engine + SDL2 compiles and links into a tvOS arm64 `.app` (`platform 3`, `minos 26.0`); on launch it boots via the UIKit delegate, brings up the SDL audio (**CoreAudio** driver present), video (1920×1080 window + GL context), event/mouse/window managers, reaches content discovery, and shows a **working native tvOS message box** ("Content not found") — graceful, since no data is bundled yet. This resolves two recon unknowns positively (CoreAudio is built into SDL; the native message box works on tvOS). Remaining for on-device: sign + Run on a physical Apple TV (needs the Apple Developer Team).

- [x] tvOS CMake toolchain ([`cmake/tvos.toolchain.cmake`](cmake/tvos.toolchain.cmake)): `CMAKE_SYSTEM_NAME=tvOS`, arm64, deployment target 26.0, forces `BSTONE_INTERNAL_SDL2=ON`. Driver script [`build-tvos.sh`](build-tvos.sh).
- [x] Entry-point reconciliation — done by mirroring the existing `bstone_entry_point` Windows mechanism for tvOS ([`bstone_entry_point.h`](src/bstone/src/bstone_entry_point.h)/[`.cpp`](src/bstone/src/bstone_entry_point.cpp)): the engine's `main()` is renamed to `bstone_entry_point`, and a real `main()` calls `SDL_UIKitRunApp(argc, argv, …)`. `3d_main.cpp` is untouched. Compile-verified against the tvOS SDK.
- [x] ~~Re-enable SDL2main~~ — **not needed.** `SDL_UIKitRunApp` lives in the SDL2 library itself, so our own `main()` calls it directly; `SDL2_DISABLE_SDL2MAIN` stays as-is.
- [x] Apple/tvOS platform tiering added to [`bstone_platform.h`](src/bstone/src/bstone_platform.h) (`BSTONE_APPLE`/`BSTONE_TVOS`); tvOS app-bundle target (Info.plist, bundle id, signing-team attr, `TARGETED_DEVICE_FAMILY=3`) added to [`src/bstone/CMakeLists.txt`](src/bstone/CMakeLists.txt).
- [x] Bundle produced by CMake's Xcode generator + a configured [`Info.plist.in`](src/bstone/src/resources/tvos/Info.plist.in) (no separate hand-authored Xcode project needed). Host codegen `tools` skipped on tvOS ([`src/CMakeLists.txt`](src/CMakeLists.txt)).
- [x] Vendored SDL 2.32.10 builds clean for tvOS (UIKit/CoreAudio/MFi/Metal) — the `project(SDL2 C)` language decl was a non-issue; SDL enables Objective-C itself. Frameworks (GameController, Metal, UIKit, CoreAudio, AVFoundation, CoreHaptics) link automatically.
- [x] `BSTONE_VULKAN_COMPILE_SHADERS` left OFF — default build runs no host `bin2c`.
- [x] tvOS-unavailable subprocess spawner (`fork`/`execve`/`execle`) in [`bstone_process_posix.cpp`](src/bstone/src/bstone_process_posix.cpp) stubbed out on tvOS (was the only compile failure). *(Pulled forward from Phase 2.)*

**Exit criteria:** App installs and runs to a paired Apple TV from Xcode; reaches `main`/`freed_main`; clears to a blank UIKit/Metal window without crashing. — **Build side done; on-device launch pending the user's sign+Run.**

### Phase 1 — Software renderer on screen

**Goal:** The game renders frames via the SW path on the TV.

**Status: DONE — validated in the tvOS Simulator (Apple TV 4K, tvOS 26).** The attract sequence (JAM Productions logo → title → in-game demo with full HUD) renders full-screen at the native display resolution via SDL's Metal renderer.

- [x] `VL_Startup()` (`src/bstone/src/id_vl.cpp`) forces `is_sw = true` on tvOS — always the software renderer, never GL/Vulkan.
- [x] The tvOS (software) window carries no `SDL_WINDOW_OPENGL`/`SDL_WINDOW_VULKAN`, so SDL_Renderer binds to Metal (confirmed: renderer name `"metal"`).
- [x] Neutralised desktop window/HiDPI sizing: dropped `SDL_WINDOW_ALLOW_HIGHDPI` on tvOS (`bstone_sys_window_sdl2.cpp` `map_flags`) **and** forced `vid_cfg_get_window_mode()` → `fake_fullscreen` so the layout sizes from the real display mode (1920×1080) instead of the 640×480 window config. The picture fills the screen with correct 4:3 pillarboxing.
- [x] The blocking loop runs fine under the UIKit run loop (attract demo animates live).

**Exit criteria:** Attract sequence / menu render correctly at native resolution; no GL/Vulkan init attempted. — **MET (simulator).**

### Phase 2 — Asset bundling + writable container

**Goal:** Real game data loads from the bundle; saves/config persist.

**Status: DONE — validated.** The bundled AOG v3.0 shareware data loads and the game runs (the "Content not found" error is gone).

- [x] `InitDestPath()` (`src/bstone/src/3d_main.cpp`) points `data_dir_` at the app bundle on tvOS via the new `bstone::sys::SpecialPath::get_base_path()` (wraps `SDL_GetBasePath`), set before `find_contents()` runs.
- [x] Game data bundled by CMake: `data/*.BS1|*.BS6|*.VSI` → app Resources, i.e. the bundle root where `SDL_GetBasePath` resolves (confirmed: the 10 `.BS1` files sit at `bstone.app/`). `./fetch-data.sh` pulls the free AOG v3.0 shareware set, which passes the engine's built-in SHA1 manifest.
- [~] Left `get_profile_dir()` on `SDL_GetPrefPath` (→ `Library/Caches`) — writable and working; it's purgeable, but durability (Documents/iCloud) isn't worth it for a sideload experiment. Revisit only if vanishing saves become annoying.
- [x] Stub the subprocess spawner (`src/bstone/src/bstone_process_posix.cpp`) on tvOS — `fork`/`exec*` are unavailable; `create_and_wait_for_exit`/`open_file_or_url` are now inert on tvOS. *(`--extract_*` options are moot without a CLI; revisit if ever needed.)*

**Exit criteria:** A level loads (driven by keyboard/remote scancodes for now); a save written in one session is readable after relaunch.

### Phase 3 — Controller + Siri Remote input

**Goal:** Full gameplay input from an MFi controller; Siri Remote navigates menus.

**Status: IMPLEMENTED — builds and boots clean; pending controller "feel" test.** The mapping translates controller input straight onto existing keyboard scancodes (no synthetic codes / rebinding needed), so it flows through the existing binding + menu systems. Default Xbox layout: **L-stick** move/strafe, **R-stick** turn, **A** use/confirm, **B** menu/back (= the real Esc), **RT** attack, **LT** strafe-mod, **D-pad ←/→** prev/next weapon, **D-pad ↑/↓** radar zoom, **LS-click** run, **View** stats, **Menu** pause. X/Y/LB/RB free.

- [x] Enabled controller events on tvOS in `configure_event_types()` + `SDL_INIT_GAMECONTROLLER` in the event mgr; controllers opened on startup and on hot-plug; tvOS hints set (`APPLE_TV_CONTROLLER_UI_EVENTS=0`).
- [x] Extended the engine event model: `GamepadButton`/`GamepadAxis` enums + `gamepad_button`/`gamepad_axis` events in `bstone_sys_event.h`; `SDL_CONTROLLER*` cases in `Sdl2EventMgr::handle_event`.
- [x] Buttons → scancodes + analog sticks → `controlx`/`controly`/`strafe_value` (`PollControllerMove` in `3d_play.cpp`); D-pad/left-stick drive menu nav via a controller block in `IN_ReadControl`. **Tunables:** deadzone 8000, `turn_scale` 2 (`id_in.cpp`/`3d_play.cpp`).
- [ ] Map controller **buttons** to new synthetic `ScanCode`s in the free `0x6B–0x7F` range of the 128-entry `Keyboard[]` bitset; set `Keyboard[code]` + `LastScan` in a new `in_handle_controller_button()` called from `in_handle_events` (`src/bstone/src/id_in.cpp:806`). This makes them work automatically with `in_is_binding_pressed`, the `in_bindings` tables, the text-config ccmd, and the binding-capture menu. Register names in `in_scan_code_name_to_id_map` (`id_in.cpp:~1644`).
- [ ] Map **analog sticks** by writing `controlx` (turn) / `controly` (forward-back) / `strafe_value` each frame in `PollControls` (`src/bstone/src/3d_play.cpp:562`), mirroring the `control2x` precedent already wired into `ControlMovement` (`src/bstone/src/3d_agent.cpp:527`). Apply a deadzone, scale by tics for frame-rate independence, and **clamp identically to the keyboard path** (the demo buffer stores `controlx`/`controly` as bytes — compatibility risk if ranges differ).
- [ ] Set SDL hints: `SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS=0` (so Menu/B feed input rather than backgrounding), `SDL_HINT_TV_REMOTE_AS_JOYSTICK`, `SDL_HINT_APPLE_TV_REMOTE_ALLOW_ROTATION` as desired.
- [ ] Neutralise the mouse-grab dependencies (`in_handle_mouse_buttons` auto-grab; focus-toggle grab in `id_in.cpp:771-802`) so no action depends on `MousePresent`.

**Exit criteria:** Full movement + fire + menu navigation with an MFi controller; Siri Remote at minimum navigates menus; controller add/remove handled gracefully.

### Phase 4 — Binding UI, menu/exit semantics, text entry

**Goal:** Remappable controls and correct remote/menu behaviour.

- [ ] Extend the binding-capture UI (`binds_draw_menu`, `src/bstone/src/3d_menu.cpp:1382`) to capture controller presses — works automatically once buttons set `LastScan` as synthetic scancodes; verify on device.
- [ ] Make the D-pad/left-stick produce `dir_North/South/East/West` and confirm → `button0` in `IN_ReadControl`/`ReadAnyControl` (`src/bstone/src/3d_menu.cpp:4230`); map a cancel/back button (and the remote's Menu→ESCAPE scancode) to `sc_escape` (menus check `Keyboard[sc_escape]` directly).
- [ ] Implement sensible Menu-button semantics: back one screen, pause overlay in gameplay (never an instant exit), as appropriate for the sideload.
- [ ] Solve text entry (save-game naming via `IN_WaitForASCII`/`in_keyboard_map_to_char`) — `SDL_TEXTINPUT` is disabled. Add an on-screen-keyboard or `SDL_StartTextInput` path, or replace with controller-driven slot naming.

**Exit criteria:** Controls are remappable; Menu button behaves sensibly; saves can be named without a physical keyboard.

### Phase 5 — Lifecycle + polish

**Goal:** A well-behaved sideload build that survives backgrounding.

- [ ] Re-enable and handle `SDL_APP_WILLENTERBACKGROUND`/`DIDENTERFOREGROUND`/`TERMINATING`/`LOWMEMORY` (disabled in `configure_event_types`): pause audio (`SDL_PauseAudioDevice`; the mixer already has `suspend_state`/`resume_state` in `src/bstone/src/bstone_system_audio_mixer.cpp:216-224`), save state, and yield. Verify the blocking loop cooperates with suspend; confirm `pre_quit()`/save runs on `SDL_APP_TERMINATING` (recon found `Sdl2EventMgr::handle_event` does **not** map `SDL_QUIT`→`EventType::quit`, so the clean-quit path needs verification).
- [ ] Force/default `snd_driver=system` on tvOS so the engine skips the forbidden OpenAL `dlopen('libopenal.so')` attempt; ideally compile `OalAudioMixer` out for Apple. Confirm SDL was built with `SDL_AUDIO_DRIVER_COREAUDIO` (else the null audio manager silently mutes the game); measure audio-callback headroom (OPL3 mixing runs inside the callback).
- [ ] Finalise the Info.plist controller capability so MFi pads enumerate (`GCSupportedGameControllers` = MicroGamepad + ExtendedGamepad, `GCSupportsControllerUserInteraction`), and tidy signing/deploy for repeatable sideloads.
- [ ] *(Optional, deferred)* MoltenVK path: add `VK_KHR_PORTABILITY_ENUMERATION` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` to instance creation (`src/bstone/src/bstone_vk_r3r.cpp:~1176-1234`) and enable `VK_KHR_portability_subset` device extension (`~1549-1553`); bundle MoltenVK and ensure `SDL_Vulkan_LoadLibrary` finds it.

**Exit criteria:** Survives background/foreground/terminate without data loss or crash; audio plays; MFi controller enumerates reliably; sideload to the author's Apple TVs is repeatable.

---

## Open questions / unknowns

**Code-side, to verify before or while building:**

- **Has this codebase ever built on macOS/Apple at all?** The "Apple = POSIX" tiering is inferred, not confirmed. The macOS SDL link fix (`project(SDL2 C CXX)`, commit `def07025`) appears reverted in vendored SDL 2.32.10 (`project(SDL2 C)`) — rebuild and confirm Apple linking; re-apply if needed.
- **Entry-point reconciliation is unprototyped.** Does dropping `SDL_MAIN_HANDLED` / calling `SDL_UIKitRunApp` cleanly coexist with bstone owning boot, and does the blocking three-level loop tolerate the UIKit/CADisplayLink run loop without stalls or watchdog kills?
- **Software-renderer performance at 4K** is unmeasured. It is almost certainly fine (320x200 internal res upscaled via SDL textures on A10X/A12 hardware), but it has not been profiled on device.
- **Was SDL built with the tvOS CoreAudio driver** for this configuration? The `Sdl2Subsystem` wrapper accepting `SDL_INIT_AUDIO` does not guarantee `SDL_AUDIO_DRIVER_COREAUDIO` is compiled in; if it is not, the null audio manager silently makes the game mute. Verify CoreAudio, UIKit, and MFi drivers are all in the tvOS SDL build.
- **Text entry has no on-screen keyboard.** `SDL_TEXTINPUT` is disabled and there is no `SDL_StartTextInput`/IME path — save-game naming needs a new on-screen-keyboard or controller-driven scheme.
- **Demo-format compatibility with analog injection.** The demo buffer stores `controlx`/`controly` as clamped bytes (`3d_play.cpp:571-601`); analog stick values must be clamped identically to the keyboard path or demo record/playback may break.
- **`project(SDL2 C)` vs `C CXX` language question.** The vendored SDL CMake's `project()` language list affects whether Apple Obj-C/C++ objects link; this was the subject of the reverted `def07025` fix and should be re-checked for the tvOS toolchain specifically.
- **CLI/argv injection grammar** (`src/bstone/src/bstone_cl.cpp/.h`) was not fully traced — confirm `--data_dir`/`--profile_dir` can be injected programmatically without a real `argv`, before relying on the `__vita__`-style hook.
- **Asset manifest enumeration** — the exact per-version file names/extensions/SHA1s live in the `Assets` class (`assets.get_*_resources()`, `get_base_path_name()` in `id_ca.cpp`); enumerate these before finalising bundling, and confirm the hi-res "mod" asset path (`ca_make_resource_path`, `id_ca.cpp:1900-1926`) never writes into the read-only bundle.
