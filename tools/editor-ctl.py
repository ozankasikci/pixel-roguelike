#!/usr/bin/env python3
"""editor-ctl.py - CLI tool for communicating with the running level editor via Unix socket.

Usage:
    python3 tools/editor-ctl.py <command> [args_json]
    python3 tools/editor-ctl.py --socket /tmp/pixel-roguelike-editor-1234.sock <command> [args_json]

Examples:
    python3 tools/editor-ctl.py inspect.selection
    python3 tools/editor-ctl.py command.set_gizmo '{"mode": "Scale"}'
    python3 tools/editor-ctl.py command.toggle_panel '{"panel": "outliner"}'
    python3 tools/editor-ctl.py record.start
    python3 tools/editor-ctl.py record.stop
    python3 tools/editor-ctl.py inspect.panels
"""

import argparse
import glob
import json
import socket
import sys


def find_socket() -> str:
    """Auto-discover the editor Unix socket via glob."""
    matches = glob.glob("/tmp/pixel-roguelike-editor-*.sock")
    if not matches:
        return ""
    # Return the most recently modified socket if multiple exist
    return sorted(matches)[-1]


def send_command(sock_path: str, cmd: str, args: dict, timeout: float) -> dict:
    """Connect to the Unix socket, send command, and return the parsed JSON response."""
    request = {"id": 1, "cmd": cmd, "args": args}
    request_str = json.dumps(request) + "\n"

    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(sock_path)
        sock.sendall(request_str.encode("utf-8"))

        # Read until newline
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
        print(f"Error: socket not found at {sock_path}", file=sys.stderr)
        sys.exit(1)
    except ConnectionRefusedError:
        print(f"Error: connection refused at {sock_path}", file=sys.stderr)
        sys.exit(1)
    except socket.timeout:
        print(f"Error: timed out waiting for response (--wait {timeout}s)", file=sys.stderr)
        sys.exit(1)
    except OSError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # Parse first complete line as JSON
    line = response_bytes.split(b"\n")[0].decode("utf-8").strip()
    if not line:
        print("Error: empty response from editor", file=sys.stderr)
        sys.exit(1)

    try:
        return json.loads(line)
    except json.JSONDecodeError as e:
        print(f"Error: invalid JSON response: {e}", file=sys.stderr)
        print(f"Raw response: {line}", file=sys.stderr)
        sys.exit(1)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send commands to the running level editor via Unix socket",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("Usage:")[1] if "Usage:" in __doc__ else "",
    )
    parser.add_argument(
        "command",
        help="Command to send (e.g. inspect.selection, command.set_gizmo)",
    )
    parser.add_argument(
        "args_json",
        nargs="?",
        default="{}",
        help="Optional JSON object of arguments (e.g. '{\"mode\": \"Scale\"}')",
    )
    parser.add_argument(
        "--socket",
        metavar="PATH",
        default="",
        help="Path to Unix socket (default: auto-discover /tmp/pixel-roguelike-editor-*.sock)",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Print raw JSON without pretty-printing",
    )
    parser.add_argument(
        "--wait",
        type=float,
        default=5.0,
        metavar="SECONDS",
        help="Socket read timeout in seconds (default: 5)",
    )

    args = parser.parse_args()

    # Resolve socket path
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

    # Parse args JSON
    try:
        cmd_args = json.loads(args.args_json)
    except json.JSONDecodeError as e:
        print(f"Error: invalid JSON in args: {e}", file=sys.stderr)
        return 1

    if not isinstance(cmd_args, dict):
        print("Error: args must be a JSON object ({})", file=sys.stderr)
        return 1

    response = send_command(sock_path, args.command, cmd_args, args.wait)

    if args.raw:
        print(json.dumps(response))
    else:
        print(json.dumps(response, indent=2))

    return 0 if response.get("ok", False) else 1


if __name__ == "__main__":
    sys.exit(main())
