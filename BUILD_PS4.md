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

## Desktop Build (for testing)

For quick testing without PS4:

```bash
cmake -B build_desktop -DPLATFORM_DESKTOP=ON
make -C build_desktop -j4
./build_desktop/Switchfin
```

Requires: libmpv, libcurl, libwebp, SDL2/GLFW
