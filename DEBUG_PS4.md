# PS4 Debugging Guide

## The Problem

PS4 doesn't have a console or easy way to view logs. The borealis logger doesn't reliably write to files, and crashes kill the app before any output is visible.

## Solution: Direct File Logging

Use direct `fprintf`/`fflush` to write logs, bypassing the borealis logger entirely.

### Basic Pattern

```cpp
// Create log file
std::string logPath = fmt::format("{}/debug.log", AppConfig::instance().configDir());
FILE* dbgLog = fopen(logPath.c_str(), "w");

// Helper lambda
auto LOG = [&](const char* msg) {
    if (dbgLog) { fprintf(dbgLog, "%s\n", msg); fflush(dbgLog); }
};

// Use it
LOG("Step 1: Starting something");
LOG("Step 2: About to call risky function");
riskyFunction();
LOG("Step 3: Survived risky function");

// Close when done
if (dbgLog) fclose(dbgLog);
```

### Key Points

1. **Always `fflush()` after every write** - Otherwise the crash kills the app before the buffer is written to disk

2. **Log file location**: `/data/Switchfin/` (same as `AppConfig::instance().configDir()`)

3. **Access via FTP** - Connect to PS4 via FTP and navigate to `/data/Switchfin/` to retrieve log files

4. **For threaded code** - Open the file in append mode (`"a"`) in the thread:
   ```cpp
   ThreadPool::instance().submit([logPath](HTTP& s) {
       FILE* dbgLog = fopen(logPath.c_str(), "a");
       auto LOG = [&](const char* msg) {
           if (dbgLog) { fprintf(dbgLog, "%s\n", msg); fflush(dbgLog); }
       };
       // ... rest of code
   });
   ```

5. **Log variables** - Use fprintf directly for non-string values:
   ```cpp
   fprintf(dbgLog, "Size: %ld bytes\n", size); fflush(dbgLog);
   fprintf(dbgLog, "Error code: 0x%08X\n", ret); fflush(dbgLog);
   ```

## What Doesn't Work

- `brls::Logger` - Doesn't write to file reliably on PS4
- `brls::Logger::setLogOutput()` - File is created but stays empty
- `fflush(nullptr)` after logger calls - Still doesn't work
- `/user/data/` path - Use `/data/Switchfin/` instead (app's config dir)

## Example: Debugging a Crash

If something crashes, add LOG statements before and after each suspicious call:

```cpp
LOG("About to call functionA");
functionA();
LOG("functionA OK");

LOG("About to call functionB");
functionB();  // If log stops here, this is the crash
LOG("functionB OK");
```

The last line in the log file tells you exactly where it crashed.

## Debug Build Workflow

For iterative debugging:

### 1. Add logging to your code

```cpp
#ifdef __PS4__
static FILE* s_debug_log = nullptr;
static bool s_debug_log_init = false;
#define DEBUG_LOG(fmt, ...) \
    if (s_debug_log) { fprintf(s_debug_log, fmt "\n", ##__VA_ARGS__); fflush(s_debug_log); }
#endif

void SomeFunction() {
#ifdef __PS4__
    if (!s_debug_log_init) {
        s_debug_log = fopen("/data/Switchfin/my_debug.log", "w");
        s_debug_log_init = true;
    }
    DEBUG_LOG("SomeFunction called with value=%d", someValue);
#endif
    // ... rest of function
}
```

### 2. Build debug PKG

```bash
# Touch modified files to force recompile
touch app/src/view/my_file.cpp

# Build
docker run --rm --entrypoint "" \
  -v /home/doug/switchfin-source:/src \
  -w /src \
  xfangfang/pacbrew:250221 \
  /bin/bash -c "export OPENORBIS=/opt/pacbrew/ps4/openorbis && make -C build_ps4 -j4"
```

### 3. Upload as debug build

```bash
curl -u ps4:ps4 -T build_ps4/IV0001-SFIN00000_00-SFIN000000008000.pkg \
  ftp://192.168.1.123:2121/data/pkg/Switchfin_debug.pkg
```

**Important**: Name it `Switchfin_debug.pkg` to distinguish from release builds.

### 4. Install and reproduce

- Install via GoldHEN Package Installer (source: HDD)
- Reproduce the bug
- Exit the app or let it run

### 5. Fetch the log

```bash
curl -u ps4:ps4 ftp://192.168.1.123:2121/data/Switchfin/my_debug.log
```

### 6. Analyze and iterate

Review the log, add more logging if needed, repeat from step 1.
