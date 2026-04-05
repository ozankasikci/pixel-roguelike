# Fix Asset Discovery for Distribution - Research

**Researched:** 2026-04-05
**Domain:** C++ cross-platform asset path resolution (macOS / Windows / Linux)
**Confidence:** HIGH

## Summary

The current `findProjectRoot()` in `PathUtils.cpp` already handles the distributed case correctly for flat directory layouts -- it walks up from the exe directory and checks each level for `assets/`. When exe and `assets/` are siblings (the CI staging layout), it finds `assets/` on the first probe. The real gaps are: (1) no support for macOS `.app` bundles where the exe is buried inside `Contents/MacOS/`, (2) no environment variable override for custom install paths, and (3) no clear diagnostic logging when resolution fails.

**Primary recommendation:** Add three search strategies in priority order: (1) `PIXEL_ROGUELIKE_ROOT` env var override, (2) macOS `.app` bundle `Contents/Resources/` detection, (3) the existing walk-up-from-exe logic (which already works for flat distribution). Add `spdlog` trace logging to `projectRoot()` so failures are diagnosable.

## Current State Analysis

### What Works

| Scenario | CWD | Exe Path | Result |
|----------|-----|----------|--------|
| Dev: run from project root | `gsd-3d-roguelike/` | `build/apps/runtime/pixel-roguelike` | CWD probe finds `assets/` immediately |
| Dev: run from build dir | `build/apps/runtime/` | same | CWD fails, exe walk-up finds `assets/` 3 levels up |
| CI flat staging | anywhere | `staging/pixel-roguelike` | Exe parent = `staging/`, probe finds `staging/assets/` on first check |
| User downloads and double-clicks | `/` or `~` | `~/Downloads/.../pixel-roguelike` | CWD fails, exe walk-up finds sibling `assets/` |

### What Fails

| Scenario | Why It Fails |
|----------|-------------|
| macOS `.app` bundle | `_NSGetExecutablePath` returns `Foo.app/Contents/MacOS/pixel-roguelike`. Parent is `Contents/MacOS/`. Walk-up checks `Contents/MacOS/assets`, `Contents/assets`, `Foo.app/assets` -- never finds `Contents/Resources/assets/` which is the standard bundle location |
| Custom install path (e.g., `/opt/games/pixel-roguelike/bin/game`) where assets are in `/opt/games/pixel-roguelike/data/assets/` | Walk-up finds `/opt/games/pixel-roguelike/` but looks for `assets/` not `data/assets/` -- this is fine if you keep the flat layout, but an env var override would handle any custom layout |
| Debugging failures | No logging -- when `projectRoot()` falls back to CWD, there is zero indication of what was tried and why it failed |

## Architecture Patterns

### How Other Engines/Games Handle This

**SDL approach (`SDL_GetBasePath`):** Returns the exe's directory. Assets are expected as siblings or in a known subdirectory. Simple and works for 90% of games. This is essentially what the current code already does.

**Godot approach:** Uses `res://` virtual paths. At startup, resolves `res://` to either the project directory (editor mode) or the exe's directory (exported game). Supports `--path` CLI override.

**Wipeout Rewrite:** Compile-time `PATH_ASSETS` define for packager overrides, runtime exe-relative fallback.

**Commercial games (Steam/itch.io):** Almost universally use exe-relative paths. The launcher sets CWD to the game directory, or the game resolves paths from `GetModuleFileName`/`_NSGetExecutablePath`. Steam guarantees CWD = game install dir.

### Recommended Search Order

```
1. PIXEL_ROGUELIKE_ROOT environment variable (if set)
   -> Use directly, no probing

2. macOS .app bundle detection (macOS only)
   -> If exe path contains ".app/Contents/MacOS/"
   -> Check <bundle>/Contents/Resources/assets/
   -> This is the standard macOS bundle resource location

3. Exe-relative walk-up (existing logic, all platforms)
   -> Start from exe's parent directory
   -> Walk up checking for assets/ at each level
   -> Depth limit of 6 is fine

4. CWD walk-up (existing logic)
   -> Handles dev-time "run from project root" case

5. Fallback: CWD as-is (existing)
   -> Last resort, likely to fail but at least consistent
```

### macOS `.app` Bundle Structure

```
MyGame.app/
  Contents/
    MacOS/
      pixel-roguelike        <-- exe lives here
    Resources/
      assets/                <-- assets go here
        shaders/
        meshes/
        scenes/
    Info.plist
```

**C++ code to detect and resolve bundle resources (no Objective-C required):**

```cpp
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>

std::string getBundleResourcesPath() {
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (!mainBundle) return {};

    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    if (!resourcesURL) return {};

    char path[4096];
    bool ok = CFURLGetFileSystemRepresentation(resourcesURL, true,
                                                reinterpret_cast<UInt8*>(path),
                                                sizeof(path));
    CFRelease(resourcesURL);

    if (ok) return std::string(path);
    return {};
}
#endif
```

**Alternative (no CoreFoundation dependency):** Parse the exe path string. If it contains `.app/Contents/MacOS/`, strip those three components and append `Contents/Resources/`:

```cpp
std::string detectAppBundleResources(const std::filesystem::path& exePath) {
    namespace fs = std::filesystem;
    std::string pathStr = exePath.string();
    auto pos = pathStr.find(".app/Contents/MacOS");
    if (pos == std::string::npos) return {};

    // Extract up to and including .app/
    fs::path bundlePath = pathStr.substr(0, pos + 4); // includes ".app"
    fs::path resources = bundlePath / "Contents" / "Resources";
    if (fs::exists(resources / "assets")) {
        return resources.lexically_normal().string();
    }
    return {};
}
```

The string-parsing approach avoids linking CoreFoundation and works in the existing `#if defined(__APPLE__)` block. It is the simpler option for this project since there is no other CoreFoundation usage.

### Windows Considerations

Windows games universally use `GetModuleFileName` + relative path. The current code already does this correctly. No changes needed for Windows distribution -- the flat layout (exe + assets/ as siblings) is the Windows standard.

One minor improvement: use `GetModuleFileNameW` (wide) instead of `GetModuleFileNameA` to handle paths with non-ASCII characters (e.g., user's home directory in Japanese). This is a nice-to-have, not a blocker.

### Environment Variable Override

An env var override (`PIXEL_ROGUELIKE_ROOT`) provides an escape hatch for:
- Custom install layouts
- Development with multiple asset directories
- CI/testing with asset stubs
- Packagers who know their filesystem layout

```cpp
const char* envRoot = std::getenv("PIXEL_ROGUELIKE_ROOT");
if (envRoot && fs::exists(fs::path(envRoot) / "assets")) {
    g_projectRoot = fs::path(envRoot).lexically_normal().string();
    return g_projectRoot;
}
```

## Don't Hand-Roll

| Problem | Don't Build | Use Instead |
|---------|-------------|-------------|
| macOS bundle resource path | String manipulation guessing at bundle structure | `CFBundleCopyResourcesDirectoryURL` or the `.app/Contents/MacOS` string detection pattern (both proven approaches) |
| Exe path on macOS | Custom `/proc` parsing | `_NSGetExecutablePath` (already used) |
| Exe path on Windows | Registry queries | `GetModuleFileName` (already used) |

## Common Pitfalls

### Pitfall 1: Symlink Resolution
**What goes wrong:** `_NSGetExecutablePath` may return a symlinked path. `std::filesystem::path::parent_path()` follows the string, not the resolved target.
**How to avoid:** Use `std::filesystem::canonical()` or `fs::weakly_canonical()` on the exe path before extracting parent. `weakly_canonical` is safer because it doesn't throw if the path doesn't exist.

### Pitfall 2: CWD Priority Over Exe Path
**What goes wrong:** Checking CWD first means the game could pick up a stale or wrong `assets/` directory if the user happens to run from a directory that contains one (e.g., another game project).
**How to avoid:** Consider checking exe-relative first, CWD second. However, changing the priority could break the dev workflow where CWD = project root is the expected case. The current CWD-first order is fine for now.

### Pitfall 3: No Logging on Failure
**What goes wrong:** When `projectRoot()` falls through all probes, it silently falls back to CWD. The user sees a crash with no explanation of what was searched.
**How to avoid:** Add `spdlog::warn` or `spdlog::info` at each probe step so the log shows what was tried.

### Pitfall 4: CoreFoundation Linking
**What goes wrong:** Adding `#include <CoreFoundation/CoreFoundation.h>` without adding `-framework CoreFoundation` to CMake link flags causes build failure.
**How to avoid:** If using the CFBundle approach, add `target_link_libraries(... "-framework CoreFoundation")` on Apple. The string-parsing approach avoids this entirely.

## Code Examples

### Minimal Fix (String-Parsing, No New Dependencies)

```cpp
// In the macOS section of projectRoot(), before the existing exe walk-up:

#if defined(__APPLE__)
    char buf[4096];
    uint32_t bufSize = sizeof(buf);
    if (_NSGetExecutablePath(buf, &bufSize) == 0) {
        fs::path exePath = fs::weakly_canonical(fs::path(buf));

        // Check for .app bundle: exe is inside Foo.app/Contents/MacOS/
        std::string pathStr = exePath.string();
        auto appPos = pathStr.find(".app/Contents/MacOS");
        if (appPos != std::string::npos) {
            fs::path bundlePath = pathStr.substr(0, appPos + 4);
            fs::path resources = bundlePath / "Contents" / "Resources";
            if (fs::exists(resources / "assets")) {
                g_projectRoot = resources.lexically_normal().string();
                return g_projectRoot;
            }
        }

        // Existing: walk up from exe directory
        root = findProjectRoot(exePath.parent_path());
        if (!root.empty()) {
            g_projectRoot = root;
            return g_projectRoot;
        }
    }
#endif
```

### Full Recommended projectRoot() Structure

```
1. Env var override:     PIXEL_ROGUELIKE_ROOT
2. macOS bundle:         .app/Contents/Resources/assets/
3. Exe-relative walk-up: existing findProjectRoot from exe parent
4. CWD walk-up:          existing findProjectRoot from cwd
5. Fallback:             cwd as-is
```

## Open Questions

1. **Should the project support `.app` bundles now or later?**
   - The CI workflows currently produce flat directories, not `.app` bundles
   - Adding bundle support is low-cost (10 lines) and future-proofs distribution
   - Recommendation: add it now since it is trivial

2. **Should search priority change to exe-first, CWD-second?**
   - Exe-first is more correct for distribution
   - CWD-first works better for `cd project-root && ./build/apps/runtime/pixel-roguelike`
   - Both cases are covered by the current code; the question is which takes precedence if both find different `assets/` dirs
   - Recommendation: keep CWD-first for dev ergonomics; in practice there is no ambiguity

## Sources

### Primary (HIGH confidence)
- Apple Developer Docs: [CFBundleCopyResourcesDirectoryURL](https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/AccessingaBundlesContents/AccessingaBundlesContents.html) - bundle resource location API
- Apple Developer Docs: [Accessing Bundle Contents](https://developer.apple.com/documentation/corefoundation/cfbundlecopyresourcesdirectoryurl(_:)) - CoreFoundation bundle API
- SDL Wiki: [SDL_GetBasePath](https://wiki.libsdl.org/SDL3/SDL_GetBasePath) - reference implementation of exe-relative path discovery

### Secondary (MEDIUM confidence)
- [Wipeout Rewrite issue #53](https://github.com/phoboslab/wipeout-rewrite/issues/53) - real-world discussion of asset path configuration for distribution
- [MacRumors C++ bundle path thread](https://forums.macrumors.com/threads/c-bundle-path.563516/) - community patterns for _NSGetExecutablePath with bundles

### Tertiary (LOW confidence)
- None; all findings verified against primary sources

## Metadata

**Confidence breakdown:**
- Current code analysis: HIGH - read the source directly
- macOS bundle pattern: HIGH - verified against Apple docs
- Windows pattern: HIGH - standard Win32 API, already implemented
- Distribution layout: HIGH - verified against CI workflow files

**Research date:** 2026-04-05
**Valid until:** 2026-05-05 (stable domain, APIs unchanged for years)
