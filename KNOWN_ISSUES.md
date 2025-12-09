# Known Issues - Switchfin PS4

## Video Player Hangs on Loading Screen

**Status**: UNRESOLVED

**Symptom**: When selecting a video to play, the player shows a gray/loading screen and doesn't start playback. Pressing X multiple times eventually kicks it into playing. Sometimes the first video works, sometimes it doesn't - behavior is inconsistent.

**Observations**:
- First video sometimes works, sometimes doesn't
- Subsequent videos usually hang
- Pressing X multiple times eventually starts playback
- Audio sometimes plays behind the gray screen
- MPV events (FILE_LOADED, PLAYBACK_RESTART) don't appear to fire on PS4
- Issue exists in original upstream Switchfin, not just this fork

**Fixes Attempted (none worked)**:
1. MPV pause state (`pause=no` option, unpause commands)
2. Loading screen visibility (hiding it, skipping it entirely)
3. MPV event polling in `draw()`
4. VideoView focus (making it focusable)
5. `MPVCore::instance().reset()` before `playMedia()` in constructor
6. `MPVCore::instance().reset()` in destructor
7. `brls::sync([](){})` after `mpv.setUrl()` to force sync
8. Transparent background (`brls/clear` set to `nvgRGBA(0,0,0,0)` in `willAppear()`)
9. `usleep(10000)` delay after `setUrl()`
10. `brls::Application::giveFocus(this->view)` after `setUrl()`
11. Simulated button presses (`onControllerButtonPressed`) after `setUrl()`
12. Removing `#ifdef ANDROID` conditional on transparency code
13. `brls::delay(100, ...)` before calling `playMedia()`
14. Setting `video_stopped = false` immediately in `setUrl()` instead of waiting for MPV event

**Root Cause Theory**:
The `draw()` function in `mpv_core.cpp` only renders video when `alpha >= 1 && !this->video_stopped`. The `video_stopped` flag is normally set to `false` by the `MPV_EVENT_PLAYBACK_RESTART` event, but this event doesn't seem to fire reliably on PS4. Button presses may trigger a UI event loop iteration that somehow resolves the rendering block.

**Files Involved**:
- `app/src/view/mpv_core.cpp` - MPV core, `draw()`, `setUrl()`, event handling
- `app/src/activity/player_view.cpp` - Player activity, `playMedia()`
- `app/src/view/video_view.cpp` - Video view UI
- `app/include/activity/player_view.hpp` - `willAppear()`/`willDisappear()` transparency

---

## Sidebar Focus Navigation Issues

**Status**: PARTIALLY RESOLVED

**Symptom**: When navigating the sidebar with D-pad, pressing down past the last item or up past the first item causes the focus highlight to jump to unexpected positions or select the entire container box.

**Fix Applied**:
Added boundary checking in `AutoTabFrame::getNextFocus()` in `app/src/view/auto_tab_frame.cpp`:
- When at last sidebar item and pressing DOWN, stay on last item
- When at first sidebar item and pressing UP, stay on first item
- When navigating back to sidebar from content, return the active tab item instead of default focus

**Files Modified**:
- `app/src/view/auto_tab_frame.cpp`

---

## TV Series Not Showing Seasons

**Status**: NEEDS INVESTIGATION

**Symptom**: When selecting a TV series from the home menu, it doesn't show the seasons - just shows a button that says "intro".

**Notes**: May be related to the sidebar/navigation focus issues or the dynamic library loading.

---

## Build Issues

See `BUILD_PS4.md` for troubleshooting:
- PKG too small (22MB instead of 58MB) - don't use clean builds
- Changes not being compiled - use `touch` to force recompile
