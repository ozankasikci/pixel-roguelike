#!/usr/bin/env python3
"""Scan the viewport to find where the gizmo is by checking ImGuizmo::IsOver()."""

import socket
import json
import sys
import glob
import time

def send_cmd(sock_path, cmd, args=None):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock_path)
    msg = {"id": 1, "cmd": cmd}
    if args:
        msg["args"] = args
    s.sendall((json.dumps(msg) + "\n").encode())
    data = b""
    while b"\n" not in data:
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data.decode().strip())

def main():
    sock = sys.argv[1] if len(sys.argv) > 1 else glob.glob("/tmp/pixel-roguelike-editor-*.sock")[0]

    # Move cursor to position, wait for processing, then check is_over
    # We need to move + wait 1 frame, then query

    # Get window info first
    info = send_cmd(sock, "inspect.window_info")
    w = info["data"]["window_w"]
    h = info["data"]["window_h"]
    print(f"Window: {w}x{h}")

    # Scan a grid across the viewport
    step = 50
    found_positions = []

    for y in range(100, int(h), step):
        for x in range(200, int(w), step):
            # Move cursor (single event, not a drag)
            send_cmd(sock, "command.drag", {
                "start_x": x, "start_y": y,
                "end_x": x, "end_y": y,
                "steps": 1, "hold": False
            })
            # Wait for events to process
            time.sleep(0.05)
            # Wait for pending events to clear
            for _ in range(10):
                r = send_cmd(sock, "inspect.pending_events")
                if r["data"]["pending"] == 0:
                    break
                time.sleep(0.02)

            # Check if gizmo is hovered
            state = send_cmd(sock, "inspect.imguizmo_state")
            if state["data"]["is_over"]:
                print(f"  GIZMO HIT at ({x}, {y})!")
                found_positions.append((x, y))
                # Don't need to scan the rest of this row
                break

    if found_positions:
        avg_x = sum(p[0] for p in found_positions) / len(found_positions)
        avg_y = sum(p[1] for p in found_positions) / len(found_positions)
        print(f"\nGizmo found at approximately ({avg_x:.0f}, {avg_y:.0f})")
        print(f"Total hit positions: {len(found_positions)}")
    else:
        print("\nGizmo NOT found in scan area!")

if __name__ == "__main__":
    main()
