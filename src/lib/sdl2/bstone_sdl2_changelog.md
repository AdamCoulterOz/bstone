# BStone: Unofficial source port of Blake Stone: Aliens of Gold and Blake Stone: Planet Strike
# SDL2 changelog

### Removed
- Directory *test*.

### Changed
- Disable dynamic API in *src/dynapi/SDL_dynapi.h*.
- Minimum CMake version to v3.5.0.
- tvOS: deliver Apple TV remote / Menu / arrow / Select / Play-Pause `UIPress`
  events even when a hardware keyboard (`GCKeyboard`) is attached, in
  *src/video/uikit/SDL_uikitview.m*.
- tvOS: bind a newly created main-display `UIWindow` to the active `UIWindowScene`
  in *src/video/uikit/SDL_uikitwindow.m* (`UIKit_CreateWindow`), so the window
  displays under the `UIScene` life cycle that tvOS 26+ requires and tvOS 27
  hard-enforces. Pairs with `BStoneSceneDelegate`
  (*src/bstone/src/bstone_tvos_scene_delegate.mm*).
