# Switchfin PS4 Build Guide

## Prerequisites

- Docker installed and running
- `xfangfang/pacbrew:250221` Docker image

Pull the image if needed:
```bash
docker pull xfangfang/pacbrew:250221
```

## Build Commands

### Clean Build (recommended for first build or after major changes)

```bash
docker run --rm --entrypoint "" \
  -v /home/doug/switchfin-source:/src \
  -w /src \
  xfangfang/pacbrew:250221 \
  /bin/bash -c "export OPENORBIS=/opt/pacbrew/ps4/openorbis && rm -rf build_ps4 && cmake -B build_ps4 -DPLATFORM_PS4=ON && make -C build_ps4 -j4"
```

### Incremental Build (after code changes)

```bash
docker run --rm --entrypoint "" \
  -v /home/doug/switchfin-source:/src \
  -w /src \
  xfangfang/pacbrew:250221 \
  /bin/bash -c "export OPENORBIS=/opt/pacbrew/ps4/openorbis && make -C build_ps4 -j4"
```

### Configure Only (without building)

```bash
docker run --rm --entrypoint "" \
  -v /home/doug/switchfin-source:/src \
  -w /src \
  xfangfang/pacbrew:250221 \
  /bin/bash -c "export OPENORBIS=/opt/pacbrew/ps4/openorbis && cmake -B build_ps4 -DPLATFORM_PS4=ON"
```

## Output

After successful build:
- PKG file: `build_ps4/IV0001-SFIN00000_00-SFIN000000008000.pkg`

## Key Environment Variables

| Variable | Value | Purpose |
|----------|-------|---------|
| OPENORBIS | /opt/pacbrew/ps4/openorbis | Path to OpenOrbis SDK inside container |

## Docker Image Details

- **Image**: `xfangfang/pacbrew:250221`
- **Contains**: OpenOrbis SDK, clang compiler, all PS4 libraries
- **Entrypoint**: Must be overridden with `--entrypoint ""`

## Toolchain

- **Compiler**: Clang 12.0.1
- **Target**: x86_64-pc-freebsd12-elf
- **SDK Path**: `/opt/pacbrew/ps4/openorbis`
- **Toolchain file**: `/opt/pacbrew/ps4/openorbis/cmake/ps4.cmake`

## Troubleshooting

### "undefined symbol: primary_dns / secondary_dns"

These globals are defined in `library/borealis/library/lib/platforms/ps4/ps4_platform.cpp`. If missing, add:

```cpp
#include <netinet/in.h>

// DNS globals required by patched musl
in_addr_t primary_dns = 0;
in_addr_t secondary_dns = 0;
```

### Docker entrypoint issues

Always use `--entrypoint ""` to bypass the default entrypoint script.

### Git safe directory warnings

Ignore the "dubious ownership" warnings - they don't affect the build.

### PKG is too small (~22MB instead of ~58MB)

This happens when `rm -rf build_ps4` deletes the CMake cache. The clean build reconfigures CMake but may miss some settings.

**Solution**: Don't use clean builds unless absolutely necessary. Use incremental builds instead:

```bash
docker run --rm --entrypoint "" \
  -v /home/doug/switchfin-source:/src \
  -w /src \
  xfangfang/pacbrew:250221 \
  /bin/bash -c "export OPENORBIS=/opt/pacbrew/ps4/openorbis && make -C build_ps4 -j4"
```

### Changes not being compiled

Make may not detect file changes due to Docker timestamp issues. Force recompile specific files with `touch`:

```bash
touch /home/doug/switchfin-source/app/src/view/mpv_core.cpp
# Then run incremental build
```

Or touch all changed files before building:

```bash
touch app/src/activity/player_view.cpp app/src/view/mpv_core.cpp
```

## Desktop Build (for testing)

For quick testing without PS4:

```bash
cmake -B build_desktop -DPLATFORM_DESKTOP=ON
make -C build_desktop -j4
./build_desktop/Switchfin
```

Requires: libmpv, libcurl, libwebp, SDL2/GLFW

## Creating a Release (for in-app updates)

The app checks `crucifix86/switchfin` GitHub releases for updates. To publish an update:

### 1. Update the version number

Edit `CMakeLists.txt` and increment the version:

```cmake
set(VERSION_MAJOR "0")
set(VERSION_MINOR "8")
set(VERSION_ALTER "3")  # Increment this for each release
```

**Important:** Only change `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_ALTER`. Do NOT change `VITA_VERSION` - that controls the PKG filename.

### 2. Build the PKG

```bash
# Touch changed files to ensure they recompile
touch app/src/utils/version.cpp CMakeLists.txt

# Build
docker run --rm --entrypoint "" \
  -v /home/doug/switchfin-source:/src \
  -w /src \
  xfangfang/pacbrew:250221 \
  /bin/bash -c "export OPENORBIS=/opt/pacbrew/ps4/openorbis && make -C build_ps4 -j4"
```

### 3. Create GitHub release

The release tag MUST match the app version:

```bash
gh release create v0.8.3 \
  build_ps4/IV0001-SFIN00000_00-SFIN000000008000.pkg \
  --repo crucifix86/switchfin \
  --title "Switchfin PS4 v0.8.3" \
  --notes "Release notes here"
```

### How the update works

1. App compares GitHub release tag against its version
2. If release tag > app version, prompts user to update
3. User clicks update, PKG downloads to `/user/data/pkg/`
4. App shows instructions and exits
5. User deletes old app from home screen
6. User goes to GoldHEN Package Installer, sets source to HDD, installs

### Important notes

- Release tag (e.g., `v0.8.3`) MUST match app version in CMakeLists.txt
- PKG filename must be exactly `IV0001-SFIN00000_00-SFIN000000008000.pkg` (never change VITA_VERSION)
- After update, app version = release tag, so no more update prompt until next release
