# Phase 18: Maintainability Refactoring for Editor UI, Serialization, and Round-Trip Fidelity - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-05
**Phase:** 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity
**Areas discussed:** Parser consolidation, Legacy format cleanup, Inspector decomposition, Switch proliferation

---

## Parser Consolidation

### Q1: How should the 8+ duplicate metadata parsing loops be consolidated?

| Option | Description | Selected |
|--------|-------------|----------|
| Extract helpers | Create parseNodeMetadata() and parseShapeTokens() helpers. Minimal structural change, big duplication win. | ✓ |
| Table-driven parser | Declare formats as data, loop dispatches. More work upfront, self-documenting. | |
| You decide | Claude picks the approach that best fits existing parser structure. | |

**User's choice:** Extract helpers
**Notes:** None

### Q2: How aggressive should the action parsing refactor be?

| Option | Description | Selected |
|--------|-------------|----------|
| Extract per-action parsers | Each action type gets a dedicated parseXxxParams() function. Type-safe and testable. | ✓ |
| Minimal cleanup | Keep current structure, add type assertions, extract shared parameter parsing. Less disruption. | |
| You decide | Claude picks based on action type count and complexity. | |

**User's choice:** Extract per-action parsers
**Notes:** None

### Q3: Should the parser add a shared PlacementBase struct?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, common base | Create PlacementBase with position, rotation, nodeId, parentNodeId. All placement types embed it. | ✓ |
| No, keep flat | Keep each placement struct independent. Helpers take individual fields by reference. | |
| You decide | Claude evaluates whether PlacementBase is worth the refactor cost. | |

**User's choice:** Yes, common base
**Notes:** None

---

## Legacy Format Cleanup

### Q4: What should happen to ~460 lines of legacy format parsing?

| Option | Description | Selected |
|--------|-------------|----------|
| Delete entirely | Remove all legacy parsing code. Old .scene files already migrated from Phase 17. | ✓ |
| Keep but isolate | Move legacy parsing into separate parseLegacyFormat() function. Quarantine dead code. | |
| Migration tool | Write one-time migration script, then delete legacy parsing. Belt and suspenders. | |

**User's choice:** Delete entirely
**Notes:** None

### Q5: Should light keywords be unified?

| Option | Description | Selected |
|--------|-------------|----------|
| Unify keywords | Single 'light' keyword with type=point/spot/directional token. Requires scene file migration. | ✓ |
| Keep separate | Three keywords is fine — different enough that separate parsing is clearer. | |
| You decide | Claude decides based on duplication impact. | |

**User's choice:** Unify keywords
**Notes:** None

### Q6: Should light migration happen in this phase?

| Option | Description | Selected |
|--------|-------------|----------|
| Same phase | Migrate scene files as part of parser refactor. Parser only handles new format. | ✓ |
| Two-step | First: parser handles both. Second: migrate files and delete old parsing. Safer rollback. | |

**User's choice:** Same phase
**Notes:** None

---

## Inspector Decomposition

### Q7: How should EditorInspectorPanel.cpp be split?

| Option | Description | Selected |
|--------|-------------|----------|
| Per-type inspector classes | Create MeshInspector, LightInspector, etc. Each separate .cpp with drawInspector() method. | ✓ |
| Per-type free functions | Keep one file, extract drawMeshInspector() etc. Less files, still organized. | |
| You decide | Claude picks based on file sizes and coupling. | |

**User's choice:** Per-type inspector classes
**Notes:** None

### Q8: Should transform editing be extracted?

| Option | Description | Selected |
|--------|-------------|----------|
| Shared drawTransform() | Single drawTransformSection(PlacementBase&) all inspectors call. | ✓ |
| Keep per-type | Each inspector handles own transforms. Different ranges/step sizes possible. | |
| You decide | Claude evaluates whether duplication is harmful or intentional. | |

**User's choice:** Shared drawTransform()
**Notes:** None

### Q9: Should asset inspectors also be extracted?

| Option | Description | Selected |
|--------|-------------|----------|
| All inspectors | Extract both scene object AND asset inspectors into separate classes. Full decomposition. | ✓ |
| Scene objects only | Only split scene object inspectors. Asset inspectors stay in main panel. | |
| You decide | Claude decides based on asset inspector complexity. | |

**User's choice:** All inspectors
**Notes:** None

---

## Switch Proliferation

### Q10: How should the 7 switch statements in EditorSceneDocument.cpp be addressed?

| Option | Description | Selected |
|--------|-------------|----------|
| Visitor pattern | Create SceneObjectVisitor interface. Each function becomes visitor implementation. | ✓ |
| Common interface on payload | Add virtual methods or free function overloads on placement structs. Dispatch via std::visit. | |
| Accept current structure | 7 switches is manageable. PlacementBase will reduce per-case boilerplate. | |
| You decide | Claude picks based on how often new object kinds get added. | |

**User's choice:** Visitor pattern
**Notes:** None

### Q11: Should the variant be kept or replaced?

| Option | Description | Selected |
|--------|-------------|----------|
| Keep variant | std::variant with PlacementBase reducing duplication. Compile-time safety. | |
| Replace with base class | PlacementBase becomes polymorphic. Virtual dispatch instead of variant visiting. | |
| You decide | Claude evaluates variant vs. inheritance for this use case. | ✓ |

**User's choice:** You decide
**Notes:** Claude has discretion on variant vs. inheritance approach

---

## Claude's Discretion

- Variant vs. inheritance decision for EditorSceneObjectPayload
- File organization for per-type inspectors (subdirectory vs flat)
- PlacementBase composition vs. inheritance
- Which switch statements are best served by visitor pattern vs. std::visit lambdas

## Deferred Ideas

- Declarative format schema — more ambitious than this refactor
- Extended test coverage for environment and group hierarchy serialization
- EditorOutlinerPanel switch cleanup
- Node ID type safety improvements (string → typed handles)
