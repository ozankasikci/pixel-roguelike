Original prompt: While trying to fix the colliders for the door, specifically for the door A in the initial scene, it looks like we messed up with the colliders. They don't work accurately, it doesn't seem to consistent. What I want is the colliders should work well within their borders boundaries. I sometimes can go through colliders and sometimes they don't feel accurate. So analyze why this happens and make web research about the best practices with the jolt colliders in game engines.

- Investigated `initial_scene.scene` DoorA collider and traced the runtime path through `LevelLoader`, `KinematicColliderSystem`, `DoorAnimationSystem`, and `PhysicsSystem`.
- Confirmed the scene collider size is roughly correct for the doorway once the leaf's `0.22` scale is applied, so the inconsistency is more likely runtime sync than raw collider dimensions.
- Found two likely runtime issues:
- The kinematic collider sync was only rotating a stored offset instead of preserving the collider's full local transform relative to the animated mesh.
- Moving door bodies had been switched from Jolt `MoveKinematic` to `SetPositionAndRotation`, which makes animated bodies behave more like teleports than velocity-driven kinematics.
- Patched kinematic links to store a full local model transform and updated the sync system to compose that transform with the animated door mesh's `modelOverride`.
- Patched physics kinematic motion to use frame `deltaTime` for `MoveKinematic`, with a zero-delta snap fallback for setup/reset cases.
- Patched kinematic solid collider creation to use the moving object layer in Jolt.
- Build verified with `cmake --build build -j4 --target pixel-roguelike`.
- Investigated packaged runtime launch failure. Root cause: startup used raw `std::filesystem::current_path()` in path resolution and mesh discovery, which throws when the inherited CWD no longer exists.
- Hardened packaged startup by guarding `current_path()` in path/cache lookup and by using the resolved project root instead of direct cwd reads for runtime mesh discovery.
