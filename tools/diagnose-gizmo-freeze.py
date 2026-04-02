#!/usr/bin/env python3
"""Diagnose gizmo and undo/redo freeze in the level editor.

Connects to the running editor via the debug harness Unix socket and:
1. Captures baseline state
2. For each operation type (translate drag, scale drag, undo, redo):
   - Captures state before
   - Executes the operation
   - Waits for frames to process
   - Captures state after
   - Checks for stuck states
3. Outputs a diagnostic report

Usage:
    # Start level editor, select an object, then run:
    python3 tools/diagnose-gizmo-freeze.py
    python3 tools/diagnose-gizmo-freeze.py --socket /tmp/pixel-roguelike-editor-1234.sock
    python3 tools/diagnose-gizmo-freeze.py --verbose
    python3 tools/diagnose-gizmo-freeze.py --json
"""

import argparse
import datetime
import glob
import json
import socket
import sys
import time


# ---------------------------------------------------------------------------
# Socket helpers
# ---------------------------------------------------------------------------

def find_socket() -> str:
    """Auto-discover the editor Unix socket via glob."""
    matches = glob.glob("/tmp/pixel-roguelike-editor-*.sock")
    if not matches:
        return ""
    return sorted(matches)[-1]


def send(sock_path: str, cmd: str, args: dict = None, timeout: float = 5.0) -> dict:
    """Send a command to the editor debug harness and return the parsed response."""
    if args is None:
        args = {}

    request = {"id": 1, "cmd": cmd, "args": args}
    request_str = json.dumps(request) + "\n"

    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(sock_path)
        sock.sendall(request_str.encode("utf-8"))

        response_bytes = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response_bytes += chunk
            if b"\n" in response_bytes:
                break

        sock.close()
    except FileNotFoundError:
        return {"ok": False, "error": f"Socket not found: {sock_path}"}
    except ConnectionRefusedError:
        return {"ok": False, "error": f"Connection refused: {sock_path}"}
    except socket.timeout:
        return {"ok": False, "error": f"Timed out waiting for response (timeout={timeout}s)"}
    except OSError as e:
        return {"ok": False, "error": str(e)}

    line = response_bytes.split(b"\n")[0].decode("utf-8").strip()
    if not line:
        return {"ok": False, "error": "Empty response from editor"}

    try:
        return json.loads(line)
    except json.JSONDecodeError as e:
        return {"ok": False, "error": f"Invalid JSON response: {e}", "raw": line}


# ---------------------------------------------------------------------------
# Diagnostic helpers
# ---------------------------------------------------------------------------

def capture_state(sock_path: str) -> dict:
    """Capture combined gizmo, ImGui, and ImGuizmo state in one call."""
    gizmo_detail = send(sock_path, "inspect.gizmo_detailed")
    imgui_cap = send(sock_path, "inspect.imgui_capture")
    imguizmo = send(sock_path, "inspect.imguizmo_state")

    combined = {}

    if gizmo_detail.get("ok") and "data" in gizmo_detail:
        combined.update(gizmo_detail["data"])
    else:
        combined["_gizmo_detail_error"] = gizmo_detail.get("error", "unknown error")

    if imgui_cap.get("ok") and "data" in imgui_cap:
        combined.update(imgui_cap["data"])
    else:
        combined["_imgui_cap_error"] = imgui_cap.get("error", "unknown error")

    if imguizmo.get("ok") and "data" in imguizmo:
        # Merge ImGuizmo state with "imguizmo_" prefix to avoid collisions
        for k, v in imguizmo["data"].items():
            if k not in combined:
                combined[k] = v
    else:
        combined["_imguizmo_error"] = imguizmo.get("error", "unknown error")

    return combined


def wait_frames(sock_path: str, n: int, delay: float = 0.05) -> None:
    """Poll inspect.frame_stats n times with a small delay to let frames process."""
    for _ in range(n):
        send(sock_path, "inspect.frame_stats")
        time.sleep(delay)


def check_stuck(state: dict, label: str, verbose: bool = False) -> list:
    """Return a list of stuck-state diagnostic strings (empty = no stuck state)."""
    issues = []

    is_using = state.get("imguizmo_is_using", state.get("is_using", False))
    mouse_left = state.get("mouse_left_down", False)
    want_capture = state.get("want_capture_mouse", False)
    is_over = state.get("imguizmo_is_over", state.get("is_over", False))

    if is_using and not mouse_left:
        issues.append(
            f"ImGuizmo::IsUsing=true but mouse is not down (stuck in drag mode)"
        )

    if want_capture and not is_using and not is_over:
        issues.append(
            "WantCaptureMouse=true but IsUsing=false and IsOver=false "
            "(ImGui thinks it owns the mouse but nothing is active)"
        )

    if verbose and issues:
        print(f"  [stuck-check:{label}]")
        for issue in issues:
            print(f"    ISSUE: {issue}")

    return issues


def format_state_summary(state: dict) -> str:
    """Return a compact one-line summary of the key state fields."""
    is_using = state.get("imguizmo_is_using", state.get("is_using", "?"))
    is_over  = state.get("imguizmo_is_over",  state.get("is_over",  "?"))
    want_cap = state.get("want_capture_mouse", "?")
    mouse_lf = state.get("mouse_left_down", "?")
    sel_cnt  = state.get("selected_count", "?")
    tool     = state.get("tool", "?")

    def fmt(val) -> str:
        if isinstance(val, bool):
            return "TRUE" if val else "false"
        return str(val)

    return (
        f"IsUsing={fmt(is_using)}  "
        f"IsOver={fmt(is_over)}  "
        f"WantCapture={fmt(want_cap)}  "
        f"MouseDown={fmt(mouse_lf)}  "
        f"sel={sel_cnt}  tool={tool}"
    )


# ---------------------------------------------------------------------------
# Test runners
# ---------------------------------------------------------------------------

class TestResult:
    def __init__(self, name: str):
        self.name = name
        self.passed = True
        self.issues: list = []
        self.snapshots: dict = {}
        self.notes: list = []

    def fail(self, issue: str) -> None:
        self.passed = False
        self.issues.append(issue)


def run_drag_test(
    sock_path: str,
    test_name: str,
    gizmo_mode: str,
    start_x: float,
    start_y: float,
    end_x: float,
    end_y: float,
    verbose: bool,
) -> TestResult:
    result = TestResult(test_name)

    # Set gizmo mode
    resp = send(sock_path, "command.set_gizmo", {"mode": gizmo_mode})
    if not resp.get("ok"):
        result.fail(f"Failed to set gizmo mode {gizmo_mode}: {resp.get('error', '?')}")
        return result

    wait_frames(sock_path, 2)

    # Capture PRE state
    pre = capture_state(sock_path)
    result.snapshots["PRE"] = pre

    if verbose:
        print(f"  PRE:  {format_state_summary(pre)}")

    # Inject drag with hold=true so button stays pressed
    resp = send(sock_path, "command.drag", {
        "start_x": start_x,
        "start_y": start_y,
        "end_x": end_x,
        "end_y": end_y,
        "steps": 10,
        "hold": True,
        "button": 0,
    })
    if not resp.get("ok"):
        result.fail(f"Drag command failed: {resp.get('error', '?')}")

    wait_frames(sock_path, 5)

    # Capture MID state (button should still be held)
    mid = capture_state(sock_path)
    result.snapshots["MID"] = mid

    if verbose:
        print(f"  MID:  {format_state_summary(mid)}  <-- drag in progress")

    # Release mouse button
    resp = send(sock_path, "command.mouse_release", {"button": 0})
    if not resp.get("ok"):
        result.fail(f"Mouse release failed: {resp.get('error', '?')}")
        result.notes.append("Could not release mouse button — subsequent tests may be affected")

    wait_frames(sock_path, 5)

    # Capture POST state
    post = capture_state(sock_path)
    result.snapshots["POST"] = post

    if verbose:
        print(f"  POST: {format_state_summary(post)}")

    # Check for stuck states
    stuck = check_stuck(post, "POST", verbose)
    for issue in stuck:
        result.fail(issue)

    return result


def run_undo_test(sock_path: str, verbose: bool) -> TestResult:
    result = TestResult("Undo after gizmo")

    pre = capture_state(sock_path)
    result.snapshots["PRE"] = pre
    pre_can_undo = pre.get("can_undo", False)
    pre_sel_count = pre.get("selected_count", 0)

    if verbose:
        print(f"  PRE:  {format_state_summary(pre)}")

    resp = send(sock_path, "command.undo", {})
    if not resp.get("ok"):
        result.fail(f"Undo failed: {resp.get('error', '?')}")

    wait_frames(sock_path, 3)

    post = capture_state(sock_path)
    result.snapshots["POST"] = post

    if verbose:
        print(f"  POST: {format_state_summary(post)}")

    stuck = check_stuck(post, "POST-UNDO", verbose)
    for issue in stuck:
        result.fail(issue)

    # Check that selection wasn't lost unexpectedly
    post_sel_count = post.get("selected_count", 0)
    if pre_can_undo and pre_sel_count > 0 and post_sel_count == 0:
        result.fail(
            f"Selection lost after undo: had {pre_sel_count} selected, now 0 "
            "(possible selection pruning issue)"
        )

    return result


def run_redo_test(sock_path: str, verbose: bool) -> TestResult:
    result = TestResult("Redo after undo")

    pre = capture_state(sock_path)
    result.snapshots["PRE"] = pre

    if verbose:
        print(f"  PRE:  {format_state_summary(pre)}")

    resp = send(sock_path, "command.redo", {})
    if not resp.get("ok"):
        result.fail(f"Redo failed: {resp.get('error', '?')}")

    wait_frames(sock_path, 3)

    post = capture_state(sock_path)
    result.snapshots["POST"] = post

    if verbose:
        print(f"  POST: {format_state_summary(post)}")

    stuck = check_stuck(post, "POST-REDO", verbose)
    for issue in stuck:
        result.fail(issue)

    return result


def run_rapid_undo_redo_test(sock_path: str, cycles: int, verbose: bool) -> TestResult:
    result = TestResult(f"Rapid undo/redo cycle ({cycles}x)")

    for i in range(cycles):
        resp = send(sock_path, "command.undo", {})
        if not resp.get("ok"):
            result.notes.append(f"Undo {i+1}/{cycles} failed: {resp.get('error', '?')}")
        wait_frames(sock_path, 2)

    for i in range(cycles):
        resp = send(sock_path, "command.redo", {})
        if not resp.get("ok"):
            result.notes.append(f"Redo {i+1}/{cycles} failed: {resp.get('error', '?')}")
        wait_frames(sock_path, 2)

    final = capture_state(sock_path)
    result.snapshots["FINAL"] = final

    if verbose:
        print(f"  FINAL: {format_state_summary(final)}")

    stuck = check_stuck(final, "FINAL", verbose)
    for issue in stuck:
        result.fail(issue)

    return result


def run_viewport_escape_drag_test(sock_path: str, verbose: bool) -> TestResult:
    """Test drag that escapes the viewport (simulate mouse leaving the window)."""
    result = TestResult("Viewport-escape drag")

    pre = capture_state(sock_path)
    result.snapshots["PRE"] = pre

    if verbose:
        print(f"  PRE:  {format_state_summary(pre)}")

    # Drag from a reasonable viewport position toward off-screen (x=0 or negative)
    resp = send(sock_path, "command.drag", {
        "start_x": 500.0,
        "start_y": 400.0,
        "end_x": -50.0,  # Off the left edge of the window
        "end_y": 400.0,
        "steps": 15,
        "hold": False,
        "button": 0,
    })
    if not resp.get("ok"):
        result.fail(f"Viewport-escape drag failed: {resp.get('error', '?')}")

    wait_frames(sock_path, 8)

    post = capture_state(sock_path)
    result.snapshots["POST"] = post

    if verbose:
        print(f"  POST: {format_state_summary(post)}")

    stuck = check_stuck(post, "POST-ESCAPE", verbose)
    for issue in stuck:
        result.fail(issue)

    return result


# ---------------------------------------------------------------------------
# Main diagnostic runner
# ---------------------------------------------------------------------------

def run_diagnostic(sock_path: str, verbose: bool, output_json: bool) -> int:
    """Run the full diagnostic sequence and return 0 if all tests pass."""

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    if not output_json:
        print("=== Gizmo Freeze Diagnostic Report ===")
        print(f"Date: {timestamp}")
        print(f"Editor socket: {sock_path}")
        print()

    # --- Baseline ---
    baseline = capture_state(sock_path)

    if not output_json:
        print("--- Baseline ---")

    sel_count = baseline.get("selected_count", 0)
    tool = baseline.get("tool", "Unknown")
    can_undo = baseline.get("can_undo", False)
    can_redo = baseline.get("can_redo", False)
    undo_label = baseline.get("undo_label", "")
    redo_label = baseline.get("redo_label", "")

    if not output_json:
        print(f"Selected count: {sel_count}")
        print(f"Tool: {tool}")
        print(f"Undo available: {'yes' if can_undo else 'no'}" +
              (f" (label: \"{undo_label}\")" if can_undo else ""))
        print(f"Redo available: {'yes' if can_redo else 'no'}" +
              (f" (label: \"{redo_label}\")" if can_redo else ""))
        print()

    if sel_count == 0:
        msg = (
            "No object selected in the editor.\n"
            "Please select an object in the viewport and re-run this script."
        )
        if output_json:
            print(json.dumps({"ok": False, "error": msg}))
        else:
            print(f"ERROR: {msg}")
        return 1

    # --- Run tests ---
    results = []

    # Test 1: Translate drag
    if not output_json:
        print("--- Test: Translate Drag ---")
    r = run_drag_test(sock_path, "Translate drag",
                      "Translate", 500.0, 400.0, 600.0, 400.0, verbose)
    results.append(r)
    if not output_json:
        _print_test_result(r)

    # Test 2: Scale drag
    if not output_json:
        print("--- Test: Scale Drag ---")
    r = run_drag_test(sock_path, "Scale drag",
                      "Scale", 500.0, 400.0, 550.0, 400.0, verbose)
    results.append(r)
    if not output_json:
        _print_test_result(r)

    # Test 3: Undo after gizmo
    if not output_json:
        print("--- Test: Undo ---")
    r = run_undo_test(sock_path, verbose)
    results.append(r)
    if not output_json:
        _print_test_result(r)

    # Test 4: Redo after undo
    if not output_json:
        print("--- Test: Redo ---")
    r = run_redo_test(sock_path, verbose)
    results.append(r)
    if not output_json:
        _print_test_result(r)

    # Test 5: Rapid undo/redo cycle
    if not output_json:
        print("--- Test: Rapid Undo/Redo Cycle (3x) ---")
    r = run_rapid_undo_redo_test(sock_path, 3, verbose)
    results.append(r)
    if not output_json:
        _print_test_result(r)

    # Test 6: Viewport-escape drag
    if not output_json:
        print("--- Test: Viewport-Escape Drag ---")
    r = run_viewport_escape_drag_test(sock_path, verbose)
    results.append(r)
    if not output_json:
        _print_test_result(r)

    # --- Summary ---
    passed = [r for r in results if r.passed]
    failed = [r for r in results if not r.passed]

    if output_json:
        output = {
            "ok": len(failed) == 0,
            "timestamp": timestamp,
            "socket": sock_path,
            "baseline": baseline,
            "results": [
                {
                    "name": r.name,
                    "passed": r.passed,
                    "issues": r.issues,
                    "notes": r.notes,
                    "snapshots": r.snapshots,
                }
                for r in results
            ],
            "summary": {
                "total": len(results),
                "passed": len(passed),
                "failed": len(failed),
            },
        }
        print(json.dumps(output, indent=2))
    else:
        print()
        print("=== Summary ===")
        print(f"PASS: {len(passed)}/{len(results)}")
        if failed:
            names = ", ".join(r.name for r in failed)
            print(f"FAIL: {len(failed)}/{len(results)} ({names})")
            print()
            print("Diagnoses:")
            for r in failed:
                print(f"  [{r.name}]")
                for issue in r.issues:
                    print(f"    - {issue}")
                if r.name == "Scale drag" and any("IsUsing" in i for i in r.issues):
                    print("    DIAGNOSIS: ImGuizmo::mbUsing not cleared after mouse release.")
                    print("               Check HandleScale() early-return guard and")
                    print("               EditorPendingCommand / MultiGizmoState finalization logic.")
                if r.name in ("Undo after gizmo", "Redo after undo") and any("Selection lost" in i for i in r.issues):
                    print("    DIAGNOSIS: pruneSelection() may not be called after undo/redo.")
                    print("               Check commandStack_.undo/redo call sites in main.cpp.")
        else:
            print()
            print("All tests passed. No stuck states detected.")

    return 0 if not failed else 1


def _print_test_result(result: TestResult) -> None:
    snapshots = result.snapshots
    for label, state in snapshots.items():
        suffix = ""
        if label == "MID":
            suffix = "  <-- drag in progress"
        elif label == "POST" and not result.passed:
            is_using = state.get("imguizmo_is_using", state.get("is_using", False))
            want_cap = state.get("want_capture_mouse", False)
            extras = []
            if is_using:
                extras.append("STUCK!")
            if want_cap and not is_using:
                extras.append("orphaned capture")
            if extras:
                suffix = "  <-- " + ", ".join(extras)
        print(f"  {label:5s}: {format_state_summary(state)}{suffix}")

    for note in result.notes:
        print(f"  NOTE: {note}")

    if result.passed:
        print("  RESULT: PASS")
    else:
        print("  RESULT: FAIL")
        for issue in result.issues:
            print(f"    - {issue}")
    print()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Diagnose gizmo and undo/redo freeze in the level editor",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--socket",
        metavar="PATH",
        default="",
        help="Path to editor Unix socket (default: auto-discover /tmp/pixel-roguelike-editor-*.sock)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print full JSON state at each capture point",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        dest="output_json",
        help="Output all results as machine-readable JSON to stdout",
    )

    args = parser.parse_args()

    sock_path = args.socket
    if not sock_path:
        sock_path = find_socket()
    if not sock_path:
        print(
            "Could not connect to editor. Is it running?\n"
            "Start the level editor and try again, or specify --socket PATH.",
            file=sys.stderr,
        )
        return 1

    # Verify connection
    probe = send(sock_path, "inspect.frame_stats")
    if not probe.get("ok"):
        print(
            f"Error: could not communicate with editor at {sock_path}\n"
            f"  {probe.get('error', 'unknown error')}",
            file=sys.stderr,
        )
        return 1

    return run_diagnostic(sock_path, args.verbose, args.output_json)


if __name__ == "__main__":
    sys.exit(main())
