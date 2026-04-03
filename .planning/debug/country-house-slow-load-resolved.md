---
status: investigating
trigger: "Country House scene loads pretty slow compared to other scenes. Find out why and how to improve loading time."
created: 2026-03-31T00:00:00Z
updated: 2026-03-31T00:00:00Z
---

## Current Focus

hypothesis: THREE compounding causes confirmed: (1) mesh cache broken for multi-mesh FBX, (2) ~206 MB of PNG decoded from disk every load, (3) FBX parsed twice per rebuild
test: code inspection + cache directory verification
expecting: confirmed — all three causes verified
next_action: report findings to user

## Symptoms

expected: Country House scene should load as fast as other scenes (e.g., warden_office, cathedral)
actual: Country House scene loads noticeably slower than other scenes
errors: None — just slow loading
reproduction: Load the Country House scene in the game or editor
started: Since multi-submesh FBX loading was added for country_house.fbx (5 per-material groups)

## Eliminated

(none)

## Evidence

- timestamp: 2026-03-31
  checked: country_house.scene vs warden_office.scene entity count
  found: country_house has 8 mesh entries; warden_office has 43 mesh entries
  implication: mesh count is not the cause; country house is simpler in that regard

- timestamp: 2026-03-31
  checked: assets/textures/country_house/ file sizes
  found: 25 textures totaling ~206 MB on disk; 13 of them are 4096x4096 or 4096x3072 PNGs
  implication: enormous texture data compared to other scenes which use procedural textures

- timestamp: 2026-03-31
  checked: ch_*.material files (all 6 used in the scene)
  found: ALL use file-based albedo/normal/ao maps from assets/textures/country_house/; none are procedural
  implication: MaterialTextureLibrary.cpp line 313 comment confirms "File-based textures: not cached per design doc"; zero disk caching for these textures

- timestamp: 2026-03-31
  checked: uncompressed RGBA8 GPU upload size
  found: ~724 MB of pixel data must be decompressed from PNG and uploaded to GPU every load
    - Seven 4096x4096 textures = 64 MB each
    - Two 4096x3072 textures = 48 MB each
    - Total: ~724 MB uncompressed RGBA8
  implication: stb_image must decompress and glTexImage2D must upload all of this on every launch

- timestamp: 2026-03-31
  checked: AssetCache::findMeshCache() with cacheKey = resolvedPath + "#" + entry.name
  found: hashFileContents() tries to open "country_house.fbx#OldHouseMapWood02" as a file path — that file does not exist, so it returns hash=0, which triggers early return nullopt
  implication: mesh cache is COMPLETELY BROKEN for multi-mesh FBX; the 5 country_house submeshes are re-parsed from FBX every single load

- timestamp: 2026-03-31
  checked: .cache/meshes/ directory
  found: only country_house_door and country_house_doors .mesh.bin files exist; ZERO entries for country_house#... submeshes
  implication: confirms mesh cache is ineffective; Assimp must re-parse country_house.fbx every run

- timestamp: 2026-03-31
  checked: writeMeshCache() with same broken cacheKey
  found: sourceHash=0 causes early return on line 165 — so writes also silently fail; cache is never populated
  implication: even after first run, cache is not built; this broken behavior is permanent

- timestamp: 2026-03-31
  checked: RuntimeGameSession::rebuild() calls bootstrapRuntimeMeshLibrary()
  found: bootstrapRuntimeMeshLibrary is called both in the constructor (line 82) and in rebuild() (line 128); loadFromFileMulti() has no short-circuit if meshes are already registered; it always calls ModelLoader::loadRawMulti() upfront before checking per-submesh caches
  implication: country_house.fbx is Assimp-parsed TWICE per scene load (constructor + rebuild); both passes also fail their cache checks

- timestamp: 2026-03-31
  checked: kAssimpImportFlags in AssimpLoader.cpp
  found: aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ImproveCacheLocality | aiProcess_TransformUVCoords — these are expensive post-processing operations
  implication: Assimp post-processing on the FBX adds CPU time on top of pure parse time; runs twice per scene load

## Resolution

root_cause: Three compounding issues:
  1. AssetCache::findMeshCache/writeMeshCache use the compound key "country_house.fbx#SubmeshName" as a file path for hashing — that file doesn't exist, hash returns 0, cache always misses and never writes. Country_house.fbx is therefore Assimp-parsed on every launch with no caching benefit.
  2. loadFromFileMulti() unconditionally calls ModelLoader::loadRawMulti() before checking per-submesh caches, AND bootstrapRuntimeMeshLibrary is called twice per scene reload (constructor + rebuild), so the expensive FBX Assimp parse with all post-processing flags runs twice each time.
  3. All 6 country house materials use file-based textures (4096x4096 and 4096x3072 PNGs, ~206 MB on disk, ~724 MB uncompressed RGBA8). The texture cache explicitly skips file-based textures. stb_image decompresses all ~206 MB and glTexImage2D uploads all ~724 MB of pixel data from scratch on every launch, with no caching.

fix: (recommendations only — not applied)
verification:
files_changed: []
