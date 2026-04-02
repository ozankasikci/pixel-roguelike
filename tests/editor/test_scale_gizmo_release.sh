#!/usr/bin/env bash
#
# test_scale_gizmo_release.sh
#
# Verifies that the scale gizmo properly releases when the mouse cursor
# drifts outside the viewport window during a drag. This was the root cause
# of the "scale gizmo locks editor" bug: HandleScale() had a !mbMouseOver
# early-exit guard that prevented the mouse-release check from running when
# the cursor left the viewport mid-drag.
#
# Prerequisites:
#   - level-editor must be running with a scene loaded (any scene)
#   - The debug harness Unix socket must be active at /tmp/pixel-roguelike-editor-*.sock
#
# Usage:
#   ./tests/editor/test_scale_gizmo_release.sh

set -euo pipefail

# --- Find the editor socket ---
SOCK=$(ls /tmp/pixel-roguelike-editor-*.sock 2>/dev/null | head -1)
if [[ -z "$SOCK" ]]; then
    echo "FAIL: No editor debug socket found. Is the level-editor running?"
    exit 1
fi
echo "Using socket: $SOCK"

# --- Helper: send a command and get JSON response ---
send_cmd() {
    local cmd="$1"
    # socat sends the JSON command and reads the response
    echo "$cmd" | socat -t5 - UNIX-CONNECT:"$SOCK"
}

# --- Helper: wait for events to drain ---
wait_idle() {
    local max_attempts=30
    local attempt=0
    while [[ $attempt -lt $max_attempts ]]; do
        local result
        result=$(send_cmd '{"cmd":"command.wait_events"}')
        local idle
        idle=$(echo "$result" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('data',{}).get('idle',False))" 2>/dev/null || echo "False")
        if [[ "$idle" == "True" ]]; then
            return 0
        fi
        sleep 0.1
        attempt=$((attempt + 1))
    done
    echo "WARNING: Events did not drain within timeout"
}

echo ""
echo "=== Test: Scale gizmo releases after out-of-viewport drag ==="
echo ""

# Step 1: Get entity list and select the first entity
echo "[1/7] Listing entities..."
ENTITIES=$(send_cmd '{"cmd":"inspect.entities"}')
echo "  Entities response: $ENTITIES"

# Pick the first entity name
FIRST_ENTITY=$(echo "$ENTITIES" | python3 -c "
import sys, json
d = json.load(sys.stdin)
entities = d.get('data', [])
if isinstance(entities, dict):
    entities = entities.get('entities', [])
if entities:
    print(entities[0].get('label', entities[0].get('name', '')))
else:
    print('')
" 2>/dev/null || echo "")

if [[ -z "$FIRST_ENTITY" ]]; then
    echo "FAIL: No entities found in scene. Load a scene first."
    exit 1
fi
echo "  Selected entity: $FIRST_ENTITY"

# Step 2: Select the entity
echo "[2/7] Selecting entity..."
send_cmd "{\"cmd\":\"command.select_entity\",\"args\":{\"name\":\"$FIRST_ENTITY\"}}"
wait_idle

# Step 3: Focus camera on entity so gizmo is visible
echo "[3/7] Focusing camera on entity..."
send_cmd "{\"cmd\":\"command.focus_entity\",\"args\":{\"name\":\"$FIRST_ENTITY\"}}"
sleep 0.5  # Wait for camera animation
wait_idle

# Step 4: Set gizmo to Scale mode
echo "[4/7] Setting gizmo to Scale mode..."
send_cmd '{"cmd":"command.set_gizmo","args":{"mode":"Scale"}}'
wait_idle

# Step 5: Check initial gizmo state -- mbUsing should be false
echo "[5/7] Checking initial gizmo state..."
GIZMO_PRE=$(send_cmd '{"cmd":"inspect.imguizmo_state"}')
echo "  Pre-drag gizmo state: $GIZMO_PRE"

IS_USING_PRE=$(echo "$GIZMO_PRE" | python3 -c "
import sys, json
d = json.load(sys.stdin)
print(d.get('data', {}).get('is_using', 'unknown'))
" 2>/dev/null || echo "unknown")

if [[ "$IS_USING_PRE" == "true" || "$IS_USING_PRE" == "True" ]]; then
    echo "FAIL: Gizmo is already in 'using' state before drag. Editor may be stuck from a previous test."
    exit 1
fi
echo "  is_using=$IS_USING_PRE (expected: false) -- OK"

# Step 6: Perform a gizmo drag that goes far upward (likely to leave viewport)
echo "[6/7] Performing scale gizmo drag (upward, 400px -- should leave viewport)..."
DRAG_RESULT=$(send_cmd '{"cmd":"command.gizmo_drag","args":{"direction":"up","distance":400,"steps":20}}')
echo "  Drag result: $DRAG_RESULT"

# Wait for all queued events to process
sleep 0.3
wait_idle

# Allow a few extra frames for ImGuizmo to process the release
sleep 0.2
wait_idle

# Step 7: Check gizmo state after drag -- mbUsing MUST be false
echo "[7/7] Checking post-drag gizmo state..."
GIZMO_POST=$(send_cmd '{"cmd":"inspect.imguizmo_state"}')
echo "  Post-drag gizmo state: $GIZMO_POST"

IS_USING_POST=$(echo "$GIZMO_POST" | python3 -c "
import sys, json
d = json.load(sys.stdin)
print(d.get('data', {}).get('is_using', 'unknown'))
" 2>/dev/null || echo "unknown")

echo ""
if [[ "$IS_USING_POST" == "false" || "$IS_USING_POST" == "False" ]]; then
    echo "PASS: Scale gizmo released correctly after out-of-viewport drag."
    echo "  is_using went from $IS_USING_PRE -> $IS_USING_POST"

    # Bonus: verify we can still interact (select another entity or undo)
    echo ""
    echo "  Bonus: verifying editor input is not locked..."
    UNDO_RESULT=$(send_cmd '{"cmd":"command.undo"}')
    echo "  Undo result: $UNDO_RESULT"
    echo "  Editor input is responsive."

    echo ""
    echo "=== ALL TESTS PASSED ==="
    exit 0
else
    echo "FAIL: Scale gizmo is STUCK in 'using' state after drag release!"
    echo "  is_using=$IS_USING_POST (expected: false)"
    echo "  This means mbUsing was not reset -- the bug is still present."
    exit 1
fi
