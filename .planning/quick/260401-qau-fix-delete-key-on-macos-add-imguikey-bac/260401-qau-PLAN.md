---
phase: quick
plan: 260401-qau
type: execute
wave: 1
depends_on: []
files_modified: [apps/level_editor/main.cpp]
autonomous: true
requirements: [QUICK-macos-delete-key]

must_haves:
  truths:
    - "Pressing the macOS delete key (backspace) triggers object deletion in the level editor"
    - "Pressing forward-delete (fn+backspace) still triggers object deletion"
    - "Delete key does not fire when a text input field is focused"
  artifacts:
    - path: "apps/level_editor/main.cpp"
      provides: "Keyboard shortcut handler for object deletion"
      contains: "ImGuiKey_Backspace"
  key_links:
    - from: "apps/level_editor/main.cpp:434"
      to: "deletePressed"
      via: "ImGuiKey_Backspace OR ImGuiKey_Delete"
      pattern: "ImGuiKey_Backspace.*ImGuiKey_Delete|ImGuiKey_Delete.*ImGuiKey_Backspace"
---

<objective>
Fix Delete key on macOS: the key labeled "delete" on Mac keyboards sends ImGuiKey_Backspace, not ImGuiKey_Delete. Add ImGuiKey_Backspace as an alternative trigger for object deletion in the level editor.

Purpose: Mac users expect pressing the delete key to delete selected objects. Currently nothing happens because only ImGuiKey_Delete (forward-delete, fn+backspace) is checked.
Output: Updated keyboard shortcut handler that accepts both keys.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@apps/level_editor/main.cpp
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add ImGuiKey_Backspace as alternative delete trigger</name>
  <files>apps/level_editor/main.cpp</files>
  <action>
At line 434 in apps/level_editor/main.cpp, change:

```cpp
if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;
```

to:

```cpp
if (!io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) deletePressed = true;
```

Also update the Edit menu item shortcut label at line 557 from `"Delete"` to `"Delete/Backspace"` so users see both keys are accepted:

```cpp
if (ImGui::MenuItem("Delete", "Delete/Backspace", false, !selectedIds.empty())) {
```

Do NOT change any other key bindings. The `!io.WantTextInput` guard already prevents firing during text input, so Backspace in text fields is unaffected.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor 2>&1 | tail -5</automated>
  </verify>
  <done>Level editor compiles. The delete shortcut handler accepts both ImGuiKey_Delete and ImGuiKey_Backspace. Menu item label reflects both keys.</done>
</task>

</tasks>

<verification>
- Build succeeds with no warnings on the modified file
- Grep confirms both ImGuiKey_Delete and ImGuiKey_Backspace appear in the shortcut handler line
</verification>

<success_criteria>
- macOS delete key (backspace) triggers object deletion
- Forward-delete (fn+backspace) still works
- Text input fields unaffected (guarded by !io.WantTextInput)
- Edit menu shows updated shortcut label
</success_criteria>

<output>
After completion, create `.planning/quick/260401-qau-fix-delete-key-on-macos-add-imguikey-bac/260401-qau-SUMMARY.md`
</output>
