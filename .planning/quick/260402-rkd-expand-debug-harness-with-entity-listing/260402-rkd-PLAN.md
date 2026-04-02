---
phase: quick
plan: 260402-rkd
type: execute
wave: 1
depends_on: []
files_modified:
  - src/editor/debug/EditorInspector.h
  - src/editor/debug/EditorInspector.cpp
  - src/editor/debug/EditorCommander.h
  - src/editor/debug/EditorCommander.cpp
  - src/editor/debug/DebugHarness.h
  - src/editor/debug/DebugHarness.cpp
  - apps/level_editor/main.cpp
autonomous: true
---

<objective>
Expand the editor debug harness with entity listing, camera focus, gizmo-aware drag, and wait-for-idle. Fix the coordinate system so all harness APIs return and accept GLFW cursor coordinates (logical pixels, window-relative).

Purpose: Enable reliable automated testing of gizmo drag operations by giving the harness the ability to discover entities, focus the camera on them, compute the gizmo screen position correctly, and perform gizmo drags in the correct coordinate space.

Output: Extended EditorInspector with `entities` and `world_to_screen` commands; extended EditorCommander with `focus_entity`, `gizmo_drag`, and `wait_events`; all coordinate APIs normalized to GLFW logical pixel space.
</objective>

<context>
@.planning/STATE.md
@src/editor/debug/EditorCommander.h
@src/editor/debug/EditorCommander.cpp
@src/editor/debug/EditorInspector.h
@src/editor/debug/EditorInspector.cpp
@src/editor/debug/DebugHarness.h
@src/editor/debug/DebugHarness.cpp
@src/editor/viewport/EditorViewportController.h
@src/editor/viewport/EditorViewportController.cpp
@src/editor/scene/EditorSceneDocument.h
@external/ImGuizmo/ImGuizmo.h
@apps/level_editor/main.cpp

<interfaces>
<!-- Key types and contracts the executor needs -->

From src/editor/scene/EditorSceneDocument.h:
```cpp
const std::vector<EditorSceneObject>& objects() const;
EditorSceneObject* findObject(std::uint64_t id);
glm::mat4 worldTransformMatrix(std::uint64_t id) const;
```
```cpp
std::string editorSceneObjectLabel(const EditorSceneObject& object);
glm::vec3 editorSceneObjectAnchor(const EditorSceneObject& object);
const char* editorSceneObjectKindName(EditorSceneObjectKind kind);
```

From src/editor/scene/EditorPreviewWorld.h:
```cpp
const EditorObjectBounds* findObjectBounds(std::uint64_t objectId) const;
struct EditorObjectBounds {
    glm::vec3 min{0.0f}, max{0.0f};
    bool valid = false;
    glm::vec3 center() const;
    void expand(const glm::vec3& point);
    void expand(const EditorObjectBounds& other);
};
```

From src/editor/viewport/EditorViewportController.h:
```cpp
struct EditorCamera { glm::vec3 position; float yawDegrees, pitchDegrees, fovDegrees, orbitDistance; glm::vec3 orbitPivot; bool orbitPivotValid; /* etc */ };
struct EditorCameraAnimation { bool active; EditorCamera target; float progress, duration; };
struct EditorViewportState { bool hovered, focused; ImVec2 origin, size; };
void beginFocusAnimation(EditorCamera& camera, EditorCameraAnimation& anim, const glm::vec3& boundsMin, const glm::vec3& boundsMax);
glm::mat4 editorCameraView(const EditorCamera& camera);
glm::mat4 editorCameraProjection(const EditorCamera& camera, float aspect);
```

From external/ImGuizmo/ImGuizmo.h:
```cpp
ImVec2 GetScreenCenter(); // returns mScreenSquareCenter, set after Manipulate()
```

Coordinate system insight: ImGuizmo::SetRect() receives viewport.origin (from ImGui::GetCursorScreenPos()) and viewport.size. With ViewportsEnable OFF, ImGui screen coords == GLFW logical cursor coords. So GetScreenCenter() returns values in GLFW cursor space directly. The y=1862 issue from earlier was likely stale data (gizmo not yet rendered that frame).
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add camera/viewport refs to harness and implement entity listing + world-to-screen</name>
  <files>
    src/editor/debug/EditorInspector.h
    src/editor/debug/EditorInspector.cpp
    src/editor/debug/DebugHarness.h
    src/editor/debug/DebugHarness.cpp
    apps/level_editor/main.cpp
  </files>
  <action>
**1. Expand DebugHarness constructor** to accept additional references needed for camera focus and screen projection. In `DebugHarness.h`, add these members alongside the existing ones:

```cpp
EditorCamera& camera_;
EditorCameraAnimation& cameraAnim_;
const EditorViewportState& viewport_;
const EditorPreviewWorld& previewWorld_;
```

Update the constructor signature in both `.h` and `.cpp` to accept `EditorCamera& camera, EditorCameraAnimation& cameraAnim, const EditorViewportState& viewport, const EditorPreviewWorld& previewWorld`. Include `editor/scene/EditorPreviewWorld.h` and `editor/viewport/EditorViewportController.h` headers.

Also forward these to `EditorInspector` and `EditorCommander` constructors.

**2. Update main.cpp** to pass the new arguments. The DebugHarness is constructed at line ~414 currently as:
```cpp
DebugHarness debugHarness(document, selectedIds, ui, commandStack, window.handle());
```
Change to also pass `editCamera, cameraAnim, viewportState, previewWorld`. Note: `viewportState` is declared at line ~1154, AFTER the harness. Move the `EditorViewportState viewportState;` declaration to before the harness construction (around line 410, near the other state variables). This is safe because viewportState is just a POD struct with default-initialized members and gets populated each frame in the render loop.

**3. Expand EditorInspector** to accept `const EditorSceneDocument& doc, const EditorCamera& camera, const EditorViewportState& viewport, const EditorPreviewWorld& previewWorld` (the doc ref already exists). Add camera_, viewport_, and previewWorld_ member references.

**4. Add `inspect.entities` command** to EditorInspector. This returns a JSON array of all scene objects with:
- `id` (uint64)
- `label` (from `editorSceneObjectLabel()`)
- `kind` (from `editorSceneObjectKindName()`)
- `world_position` (xyz from `editorSceneObjectAnchor()`)

Implementation in EditorInspector::entities():
```cpp
nlohmann::json EditorInspector::entities() const {
    auto arr = nlohmann::json::array();
    for (const auto& obj : doc_.objects()) {
        glm::vec3 pos = editorSceneObjectAnchor(obj);
        arr.push_back({
            {"id", obj.id},
            {"label", editorSceneObjectLabel(obj)},
            {"kind", editorSceneObjectKindName(obj.kind)},
            {"world_position", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}}
        });
    }
    return {{"ok", true}, {"data", arr}};
}
```

Register as `inspect.entities` in DebugHarness::init().

**5. Add `inspect.world_to_screen` command** to EditorInspector. Given a world-space xyz, projects it to screen coordinates using the current camera view/projection and viewport rect. Returns the screen position in GLFW cursor coordinates (logical pixels), plus a `visible` flag (true if the point is in front of the camera and within the viewport).

Implementation:
```cpp
nlohmann::json EditorInspector::worldToScreen(const nlohmann::json& args) const {
    float wx = args.value("x", 0.0f);
    float wy = args.value("y", 0.0f);
    float wz = args.value("z", 0.0f);
    
    float aspect = viewport_.size.x > 0 ? viewport_.size.x / viewport_.size.y : 1.0f;
    glm::mat4 view = editorCameraView(camera_);
    glm::mat4 proj = editorCameraProjection(camera_, aspect);
    glm::vec4 clip = proj * view * glm::vec4(wx, wy, wz, 1.0f);
    
    bool visible = clip.w > 0.001f;
    if (visible) {
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        // NDC to viewport pixel coordinates
        float sx = viewport_.origin.x + (ndc.x * 0.5f + 0.5f) * viewport_.size.x;
        float sy = viewport_.origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_.size.y;
        visible = sx >= viewport_.origin.x && sx <= viewport_.origin.x + viewport_.size.x
               && sy >= viewport_.origin.y && sy <= viewport_.origin.y + viewport_.size.y;
        return {{"ok", true}, {"data", {{"x", sx}, {"y", sy}, {"visible", visible}}}};
    }
    return {{"ok", true}, {"data", {{"x", 0}, {"y", 0}, {"visible", false}}}};
}
```

Register as `inspect.world_to_screen` in DebugHarness::init().

**6. Fix `inspect.gizmo_screen_pos`** in DebugHarness::init(). The current implementation subtracts the GLFW window position from the gizmo center, which is wrong. Since ImGuizmo coordinates with ViewportsEnable OFF are already in ImGui screen coords = GLFW window-relative logical pixels, no subtraction is needed. Simplify to:
```cpp
registry_.registerCommand("inspect.gizmo_screen_pos", [this](const nlohmann::json& /*args*/) {
    ImVec2 center = ImGuizmo::GetScreenCenter();
    return nlohmann::json{
        {"ok", true},
        {"data", {
            {"x", center.x},
            {"y", center.y},
            {"note", "Coordinates are in GLFW cursor space (logical pixels, window-relative). "
                     "Only valid after ImGuizmo::Manipulate() has been called this frame."}
        }}
    };
});
```
Remove the window position subtraction and the `window_relative_x/y` fields.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>
    EditorInspector and DebugHarness accept camera/viewport/previewWorld references. `inspect.entities` returns all scene objects with label, kind, and world position. `inspect.world_to_screen` projects world coordinates to screen. `inspect.gizmo_screen_pos` returns raw ImGuizmo coordinates without incorrect window position subtraction. Code compiles cleanly.
  </done>
</task>

<task type="auto">
  <name>Task 2: Implement focus_entity, gizmo_drag, and wait_events commands</name>
  <files>
    src/editor/debug/EditorCommander.h
    src/editor/debug/EditorCommander.cpp
    src/editor/debug/DebugHarness.cpp
  </files>
  <action>
**1. Expand EditorCommander constructor** to accept `EditorCamera& camera, EditorCameraAnimation& cameraAnim, const EditorViewportState& viewport, const EditorPreviewWorld& previewWorld`. Store as members. Include the necessary headers: `editor/scene/EditorPreviewWorld.h`, `editor/viewport/EditorViewportController.h`.

**2. Add `command.focus_entity` command** to EditorCommander. Takes `{"name": "entity_label"}`. Implementation:

```cpp
nlohmann::json EditorCommander::focusEntity(const nlohmann::json& args) {
    std::string name = args.value("name", "");
    if (name.empty()) {
        return {{"ok", false}, {"error", "Missing required arg: name"}};
    }
    
    const EditorSceneObject* targetObj = nullptr;
    for (const auto& obj : doc_.objects()) {
        if (editorSceneObjectLabel(obj) == name) {
            targetObj = &obj;
            break;
        }
    }
    if (!targetObj) {
        return {{"ok", false}, {"error", "Entity not found: " + name}};
    }
    
    // Compute bounds for the focus animation, same logic as main.cpp focusPressed
    const EditorObjectBounds* bounds = previewWorld_.findObjectBounds(targetObj->id);
    if (bounds && bounds->valid) {
        beginFocusAnimation(camera_, cameraAnim_, bounds->min, bounds->max);
    } else {
        glm::vec3 anchor = editorSceneObjectAnchor(*targetObj);
        glm::vec3 halfUnit(0.5f);
        beginFocusAnimation(camera_, cameraAnim_, anchor - halfUnit, anchor + halfUnit);
    }
    
    return {{"ok", true}, {"note", "Camera focus animation started. Wait ~0.3s (18 frames at 60fps) for completion."}};
}
```

Register as `command.focus_entity` in DebugHarness::init().

**3. Add `command.gizmo_drag` command** to EditorCommander. This is a high-level command that reads the current gizmo screen center, then queues a drag sequence from the gizmo center in a specified direction. Takes:
- `direction`: "right", "left", "up", "down", or {"dx": float, "dy": float}
- `distance`: pixels to drag (default 50)
- `steps`: interpolation steps (default 10)

Implementation:
```cpp
nlohmann::json EditorCommander::gizmoDrag(const nlohmann::json& args) {
    ImVec2 gizmoCenter = ImGuizmo::GetScreenCenter();
    
    // Validate the gizmo center is within reasonable window bounds
    int ww = 0, wh = 0;
    if (window_) glfwGetWindowSize(window_, &ww, &wh);
    if (gizmoCenter.x < 0 || gizmoCenter.y < 0 || gizmoCenter.x > ww || gizmoCenter.y > wh) {
        return {{"ok", false}, {"error", "Gizmo screen center is out of window bounds"},
                {"gizmo_x", gizmoCenter.x}, {"gizmo_y", gizmoCenter.y},
                {"window_w", ww}, {"window_h", wh},
                {"hint", "Focus camera on entity first, then wait for gizmo render"}};
    }
    
    float dx = 0, dy = 0;
    if (args.contains("direction")) {
        if (args["direction"].is_string()) {
            std::string dir = args["direction"].get<std::string>();
            if (dir == "right")     { dx = 1; dy = 0; }
            else if (dir == "left") { dx = -1; dy = 0; }
            else if (dir == "up")   { dx = 0; dy = -1; }
            else if (dir == "down") { dx = 0; dy = 1; }
            else return {{"ok", false}, {"error", "Unknown direction: " + dir}};
        } else if (args["direction"].is_object()) {
            dx = args["direction"].value("dx", 0.0f);
            dy = args["direction"].value("dy", 0.0f);
        }
    } else {
        dx = 1; dy = 0; // default: drag right
    }
    
    float distance = args.value("distance", 50.0f);
    int steps = args.value("steps", 10);
    if (steps < 1) steps = 1;
    
    // Normalize direction
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.001f) { dx /= len; dy /= len; }
    
    float startX = gizmoCenter.x;
    float startY = gizmoCenter.y;
    float endX = startX + dx * distance;
    float endY = startY + dy * distance;
    
    // Build drag args and delegate to the existing drag() method
    nlohmann::json dragArgs = {
        {"start_x", startX}, {"start_y", startY},
        {"end_x", endX}, {"end_y", endY},
        {"button", 0}, {"steps", steps}
    };
    
    nlohmann::json result = drag(dragArgs);
    result["gizmo_center"] = {{"x", startX}, {"y", startY}};
    result["drag_end"] = {{"x", endX}, {"y", endY}};
    return result;
}
```

Register as `command.gizmo_drag` in DebugHarness::init().

**4. Add `command.wait_events` command** to EditorCommander. This is a synchronous check that returns the current event queue status. Since the harness processes one event per frame, the caller should poll this until `pending` is 0.

```cpp
nlohmann::json EditorCommander::waitEvents(const nlohmann::json& /*args*/) {
    int pending = pendingEventCount();
    bool cameraAnimating = cameraAnim_.active;
    return {
        {"ok", true},
        {"data", {
            {"pending_events", pending},
            {"camera_animating", cameraAnimating},
            {"idle", pending == 0 && !cameraAnimating}
        }}
    };
}
```

Register as `command.wait_events` in DebugHarness::init().

**5. Add `inspect.camera` command** to EditorInspector. Returns the current camera state for debugging coordinate issues:
```cpp
nlohmann::json EditorInspector::camera() const {
    return {{"ok", true}, {"data", {
        {"position", {{"x", camera_.position.x}, {"y", camera_.position.y}, {"z", camera_.position.z}}},
        {"yaw", camera_.yawDegrees},
        {"pitch", camera_.pitchDegrees},
        {"fov", camera_.fovDegrees},
        {"orbit_distance", camera_.orbitDistance},
        {"orbit_pivot", {{"x", camera_.orbitPivot.x}, {"y", camera_.orbitPivot.y}, {"z", camera_.orbitPivot.z}}},
        {"orbit_pivot_valid", camera_.orbitPivotValid}
    }}};
}
```

Register as `inspect.camera` in DebugHarness::init().

**6. Declare all new methods** in the respective header files:
- In `EditorInspector.h`: `nlohmann::json entities() const;`, `nlohmann::json worldToScreen(const nlohmann::json& args) const;`, `nlohmann::json camera() const;`
- In `EditorCommander.h`: `nlohmann::json focusEntity(const nlohmann::json& args);`, `nlohmann::json gizmoDrag(const nlohmann::json& args);`, `nlohmann::json waitEvents(const nlohmann::json& args);`

**7. Include necessary headers** in EditorCommander.cpp: `<cmath>` for std::sqrt.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>
    `command.focus_entity` starts camera focus animation on a named entity. `command.gizmo_drag` reads current gizmo screen center and queues a drag in the specified direction. `command.wait_events` reports event queue and camera animation status for polling. `inspect.camera` returns camera state. `inspect.entities` and `inspect.world_to_screen` from Task 1 are registered. All commands compile cleanly and are registered in the harness.
  </done>
</task>

</tasks>

<verification>
1. Build succeeds: `cmake --build build --target level-editor` — zero errors
2. Launch editor, connect to debug socket, verify new commands are available:
   - `inspect.entities` returns list of scene objects with labels and positions
   - `inspect.world_to_screen` with known object position returns in-viewport screen coordinates
   - `inspect.gizmo_screen_pos` returns coordinates within viewport bounds (no y=1862 on a 916-high window)
   - `inspect.camera` returns current camera state
   - `command.focus_entity` with a scene object name starts camera animation
   - `command.wait_events` returns idle=true after events drain
   - `command.gizmo_drag` with direction "right" drags from gizmo center rightward
</verification>

<success_criteria>
- All new harness commands compile and are registered
- `inspect.entities` lists all scene objects with id, label, kind, world_position
- `inspect.gizmo_screen_pos` returns coordinates in GLFW cursor space (no window position subtraction, values within window dimensions)
- `command.focus_entity` triggers camera animation targeting the named entity
- `command.gizmo_drag` reads gizmo center and queues a complete press-drag-release sequence
- `command.wait_events` reports idle state accurately (pending events + camera animation)
</success_criteria>

<output>
After completion, update `.planning/STATE.md` stopped_at field and add to Quick Tasks Completed table.
</output>
</task>
