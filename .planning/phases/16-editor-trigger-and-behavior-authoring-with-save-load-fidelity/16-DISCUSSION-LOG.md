# Phase 16: Editor Trigger and Behavior Authoring with Save-Load Fidelity - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-04
**Phase:** 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity
**Areas discussed:** Trigger authoring, Behavior editing UI, Interactable editing, Save-load fidelity

---

## Trigger authoring

### How should triggers be created in the editor?

| Option | Description | Selected |
|--------|-------------|----------|
| Add menu entry | Add Trigger option in existing Add Mesh picker dropdown — consistent with how meshes, lights, colliders are added | |
| Context menu on entity | Right-click an entity -> 'Add Trigger Zone' as a child. Trigger positioned relative to parent entity | ✓ |
| Both menu + context | Top-level 'Add Trigger' AND right-click 'Add Trigger Zone'. More discoverable but more UI work | |

**User's choice:** Context menu on entity
**Notes:** None

### How should trigger volumes be visualized and edited in the viewport?

| Option | Description | Selected |
|--------|-------------|----------|
| Wireframe + gizmo handles | Semi-transparent wireframe with drag handles on corners/edges for resize. Same gizmo style as colliders | ✓ |
| Solid semi-transparent | Filled semi-transparent volume. Resize via inspector fields only | |
| Wireframe only, inspector resize | Wireframe outline, all editing in inspector panel | |

**User's choice:** Wireframe + gizmo handles
**Notes:** None

### Should triggers support both Box and Sphere shapes?

| Option | Description | Selected |
|--------|-------------|----------|
| Both shapes | Full support for Box and Sphere from the start. Shape selector dropdown in inspector | ✓ |
| Box only first | Start with Box only. Sphere deferred | |

**User's choice:** Both shapes
**Notes:** None

### Should triggers appear as standalone items in the outliner?

| Option | Description | Selected |
|--------|-------------|----------|
| Standalone outliner items | Top-level entries like meshes and lights. Can exist independently or be parented | ✓ |
| Children only | Only as children of another entity | |
| Both modes | Can be standalone OR children | |

**User's choice:** Standalone outliner items
**Notes:** None

---

## Behavior editing UI

### How should behaviors be presented in the inspector?

| Option | Description | Selected |
|--------|-------------|----------|
| Collapsible event sections | Expandable sections for each event type. '+ Add Action' button per section | ✓ |
| Flat action list with event tags | Single scrollable list of all actions with event-type dropdown per action | |
| Tab-based events | Tabs for each event type | |

**User's choice:** Collapsible event sections
**Notes:** None

### How should users pick the action type?

| Option | Description | Selected |
|--------|-------------|----------|
| Dropdown selector | Standard combo listing all 14 action types | |
| Categorized dropdown | Dropdown with categories: Door, Audio, Lighting, Entity, Player, etc. | ✓ |
| You decide | Let Claude pick | |

**User's choice:** Categorized dropdown
**Notes:** None

### How should target node ID be selected?

| Option | Description | Selected |
|--------|-------------|----------|
| Entity picker from outliner | Click 'Pick Target' button, then click entity in outliner/viewport | |
| Dropdown of named nodes | Combo dropdown listing all entities with nodeId. Type-to-filter. 'self' first | ✓ |
| Raw text input | Free-text field for nodeId string | |

**User's choice:** Dropdown of named nodes
**Notes:** None

### Should all 14 action types be editable?

| Option | Description | Selected |
|--------|-------------|----------|
| All 14 action types | Full editor support for every action type | |
| Core subset first | Start with most-used types: door ops, SetLight, PlaySound, Delay, Enable/Disable | ✓ |
| You decide | Let Claude determine based on scene usage | |

**User's choice:** Core subset first
**Notes:** None

---

## Interactable editing

### How should interactable declarations be authored on mesh entities?

| Option | Description | Selected |
|--------|-------------|----------|
| Checkbox + inline fields | 'Make Interactable' checkbox. When checked, shows prompt text, distance, dot threshold inline | ✓ |
| Separate section | Dedicated 'Interactable' collapsible section in inspector | |
| You decide | Let Claude pick | |

**User's choice:** Checkbox + inline fields
**Notes:** None

### Should interaction distance have visual feedback in the viewport?

| Option | Description | Selected |
|--------|-------------|----------|
| Distance ring only | Wireframe ring/sphere showing interaction distance. No dot threshold visual | ✓ |
| Distance + cone | Ring for distance AND cone/arc for dot threshold | |
| No viewport feedback | Inspector only | |
| You decide | Let Claude determine | |

**User's choice:** Distance ring only
**Notes:** None

---

## Save-load fidelity

### What's the priority for save-load fidelity?

| Option | Description | Selected |
|--------|-------------|----------|
| Both equally | Triggers become full objects and survive save/load. Behaviors persist through edits. No data loss | ✓ |
| Triggers first | Focus on trigger save/load. Behavior preservation secondary | |
| Behaviors first | Focus on behavior data preservation. Trigger save/load secondary | |

**User's choice:** Both equally
**Notes:** None

### Should there be an automated round-trip test?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, extend existing test | Extend test_level_roundtrip.cpp to cover triggers, behaviors, interactables | |
| Yes, new dedicated test | New test file specifically for behavior/trigger round-trip fidelity | ✓ |
| Manual verification only | Verify manually in editor | |

**User's choice:** Yes, new dedicated test
**Notes:** None

---

## Claude's Discretion

- Exact wireframe colors and transparency for trigger volumes
- ImGui layout details for behavior/interactable inspector sections
- Which action parameter fields need special widgets vs simple float/string inputs
- Internal ordering of the categorized action type dropdown

## Deferred Ideas

- Full support for remaining 8 action types (FlickerLight, ShowMessage, EmitEvent, LockPlayer, UnlockPlayer, TeleportPlayer)
- Dot threshold viewport visualization
- Behavior copy/paste between entities
- Behavior templates/presets
