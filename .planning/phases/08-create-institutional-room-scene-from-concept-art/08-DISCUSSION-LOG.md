# Phase 8: Create Institutional Room Scene from Concept Art - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-01
**Phase:** 08-create-institutional-room-scene-from-concept-art
**Areas discussed:** Room geometry & doors, Materials & color palette, Lighting & atmosphere, Props & detail elements

---

## Room Geometry & Doors

### Room Size

| Option | Description | Selected |
|--------|-------------|----------|
| Small (6m x 5m) | Tight institutional feel, similar to warden_office but shorter depth | |
| Medium (8m x 6m) | Comfortable space matching concept art proportions | ✓ |
| Large (10m x 7m) | Spacious corridor, more liminal emptiness | |

**User's choice:** Medium (8m x 6m)
**Notes:** Recommended option selected. Matches warden office proportions.

### Door Layout

| Option | Description | Selected |
|--------|-------------|----------|
| All on far wall | All three doors on the same wall, player enters from behind | ✓ |
| L-shaped arrangement | Wooden door on left wall, metal and chained on far wall | |
| You decide | Claude picks arrangement | |

**User's choice:** All on far wall
**Notes:** Matches concept art directly.

### Wooden Door Behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Static open doorway | Just geometry with warm light behind it | ✓ |
| Interactable door | Uses DoorSystem, player can open/close | |
| You decide | Claude picks | |

**User's choice:** Static open doorway
**Notes:** Simpler, matches concept art as-is.

### Blocked Doors Behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Purely visual | Static geometry only, no interaction | |
| Interactable stubs | InteractableComponent with "locked" message | ✓ |
| You decide | Claude picks | |

**User's choice:** Interactable stubs
**Notes:** Adds player feedback when approaching locked doors.

---

## Materials & Color Palette

### Wall Material

| Option | Description | Selected |
|--------|-------------|----------|
| New procedural material | New inst_beige_wall with warm beige base_color | ✓ |
| Tint concrete_wall | Reuse existing material with tint | |
| You decide | Claude picks | |

**User's choice:** New procedural material
**Notes:** Dedicated material for institutional aesthetic.

### Floor Finish

| Option | Description | Selected |
|--------|-------------|----------|
| High gloss | Low roughness (~0.3), subtle reflections | ✓ |
| Moderate sheen | Medium roughness (~0.55) | |
| Matte | High roughness (~0.8) | |

**User's choice:** High gloss
**Notes:** Institutional linoleum feel with visible light reflections.

### Trim Material

| Option | Description | Selected |
|--------|-------------|----------|
| New dark trim material | Dedicated inst_dark_trim with dark brown | ✓ |
| Tint wood_default dark | Reuse existing wood material | |
| You decide | Claude picks | |

**User's choice:** New dark trim material
**Notes:** Distinct painted wood trim look.

---

## Lighting & Atmosphere

### Light Count

| Option | Description | Selected |
|--------|-------------|----------|
| 2 panels | Two fluorescent panels matching concept art | ✓ |
| 3 panels | Three panels for more even illumination | |
| You decide | Claude places lights for best mood | |

**User's choice:** 2 panels
**Notes:** Direct match to concept art.

### Door Light Source

| Option | Description | Selected |
|--------|-------------|----------|
| Point light behind doorway | Warm point light behind open door frame | ✓ |
| Area light in doorway | LTC area light across door opening | |
| You decide | Claude picks | |

**User's choice:** Point light behind doorway
**Notes:** Creates natural warm light spill.

### Environment Profile

| Option | Description | Selected |
|--------|-------------|----------|
| New 'institutional' profile | Tuned for warm indoor fluorescent lighting | ✓ |
| Reuse 'default' profile | Existing profile with tinted lights | |
| You decide | Claude picks | |

**User's choice:** New 'institutional' profile
**Notes:** Full control over interior lighting mood.

---

## Props & Detail Elements

### HVAC Vent

| Option | Description | Selected |
|--------|-------------|----------|
| Mesh only, no particle effect | Rectangular grille geometry, no steam | ✓ |
| Skip the vent entirely | Don't add HVAC vent | |
| Vent + particle steam | Full mesh plus particle system | |

**User's choice:** Mesh only, no particle effect
**Notes:** Geometry implies atmosphere without needing particle system.

### Smoke Detector

| Option | Description | Selected |
|--------|-------------|----------|
| Small mesh + red point light | Cylinder mesh + small red light for LED | ✓ |
| Emissive material only | Mesh with emissive red material, no point light | |
| Skip it | Don't add smoke detector | |

**User's choice:** Small mesh + red point light
**Notes:** Red LED adds subtle detail and color accent to ceiling.

### Chain & Padlock Detail

| Option | Description | Selected |
|--------|-------------|----------|
| Simple chain mesh | 3-4 torus chain links + box padlock | ✓ |
| Just a padlock | Single padlock, no chain | |
| You decide | Claude picks detail level | |

**User's choice:** Simple chain mesh
**Notes:** Recognizable at gameplay distance, dark iron tint.

---

## Claude's Discretion

- Exact UV scale values for new materials
- Crown molding mesh design
- Precise light positions and intensities
- Door frame geometry details
- Collider placement

## Deferred Ideas

None — discussion stayed within phase scope
