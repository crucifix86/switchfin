# PS4 Video Display Issue - Debug Findings

## Status: FIXED

## Problem
Video shows loading screen with audio playing, but video doesn't display until user presses controller buttons multiple times.

## Root Cause Found
**File:** `/home/doug/switchfin-source/library/borealis/library/lib/platforms/sdl/sdl_platform.cpp` (lines 253-259)

```cpp
if (!hasEvent && !Application::hasActiveEvent())
{
    if (SDL_WaitEventTimeout(&event, (int)(brls::Application::getDeactivatedFrameTime() * 1000))
        && !processEvent(&event))
    {
        return false;
    }
}
```

The main loop **BLOCKS** with `SDL_WaitEventTimeout()` when:
1. No SDL events are polled
2. `hasActiveEvent()` returns false

On PS4, controller buttons are **polled** via `SDL_GameControllerGetButton()`, NOT via SDL events. So MPV video frame updates don't generate SDL events, causing the loop to block and skip frame rendering.

## Fix Applied
**File:** `/home/doug/switchfin-source/library/borealis/library/lib/core/application.cpp` (line 652)

```cpp
bool Application::hasActiveEvent()
{
#if defined(__SWITCH__) || defined(__PS4__)
    // Switch and PS4 do not support waiting for events - always render
    return true;
#else
    // ... original logic
#endif
}
```

## Key Architecture Findings

### Main Loop Flow
1. `Application::mainLoop()` -> `platform->runLoop(internalMainLoop)`
2. `internalMainLoop()`:
   - `platform->mainLoopIteration()` - **CAN BLOCK HERE**
   - `processInput()` - polls controller state
   - `Ticking::updateTickings()` - animations
   - `Application::frame()` - renders everything
   - `Threading::performSyncTasks()`

### Frame Rendering Pipeline
1. `videoContext->beginFrame()` - (empty for OpenGL)
2. `videoContext->clear()` - `glClear()`
3. `nvgBeginFrame()` - nanovg setup
4. Draw all views (including VideoView -> MPVCore::draw())
5. `nvgEndFrame()` - flush nanovg commands
6. `videoContext->endFrame()` - `SDL_GL_SwapWindow()`

### PS4 Specifics
- Uses GLES2 via Orbis Pigletv2VSH
- Precompiled binary shaders (not GLSL source)
- Controller input: `SDL_GameControllerGetButton()` (polled, not events)

### Input Processing
- **File:** `library/borealis/library/lib/platforms/sdl/sdl_input.cpp`
- `updateControllerState()` polls buttons via `SDL_GameControllerGetButton()`
- Does NOT call `setActiveEvent(true)` when buttons are pressed
- Button events don't go through SDL event queue

### Deactivation Settings (defaults)
- `deactivatedFPS = 5` (5 FPS when inactive)
- `deactivatedTime = 5000000` (5 seconds)
- `deactivatedBehavior = false` (disabled by default)

## Files Modified During Debug

### App Layer (no effect on issue)
- `app/src/view/video_view.cpp` - MPV event handlers, input guards
- `app/src/view/mpv_core.cpp` - GL state resets, alpha checks, logging

### Borealis Layer (THE FIX)
- `library/borealis/library/lib/core/application.cpp` - `hasActiveEvent()` fix

## Other Attempted Fixes (did not work)
1. `ignoreInput` flag for spurious BUTTON_B - worked for that issue
2. `hideLoading()` on MPV_RESUME - no effect
3. `glFinish()` / `glFlush()` after MPV render - no effect
4. Alpha threshold change (>0.9 vs >=1) - no effect
5. SDL_PushEvent button simulation - doesn't work (input is polled)
6. MPV context restart - not tested, removed

## PS4 FTP Upload
```bash
curl -u ps4:ps4 -T build_ps4/IV0001-SFIN00000_00-SFIN000000008000.pkg ftp://192.168.1.123:2121/data/pkg/Switchfin_debug.pkg
```

## Log File Location
`/data/Switchfin/switchfin.log` on PS4 (access via FTP)
