---
quick_id: 260406-pe8
description: "Fix sky texture loading in packaged builds: resolve paths via resolveProjectPath"
status: complete
commit: 41c3d65
---

# Summary: Fix sky texture loading in packaged builds

## Problem
`SkyTextureLibrary` passed raw relative paths to `stbi_load()` via `Texture2D::createRGBA8FromFile()` and `TextureCube::createRGBA8FromFiles()`. When the packaged game was launched with a CWD different from the project root (e.g. double-clicking on macOS), sky cubemap faces and cloud textures failed to load.

## Fix
Added `resolveProjectPath()` calls in `SkyTextureLibrary::resolve()` and `SkyTextureLibrary::resolveCube()` to match the pattern used by all other asset loaders (MeshLibrary, Shader, ContentRegistry).

## Files Changed
- `src/engine/rendering/post/SkyTextureLibrary.cpp` — added `#include "engine/core/PathUtils.h"`, resolve paths before `createRGBA8FromFile`/`createRGBA8FromFiles`
