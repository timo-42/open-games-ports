#!/usr/bin/env python3
"""Compare a live loopback server with the browser replies in a probe.py capture.

Start the candidate with the map, cvars and data from the original run, then:
    python3 compare_browser.py /path/to/captures/dm_toujane/run1 --port 28960

Use one invocation per map. Only ordinary getinfo and getstatus requests are sent.
Missing, extra and unequal datagrams all fail. No wire fields are normalized.
"""

import argparse
import hashlib
import json
import socket
import sys
import time
from pathlib import Path

OOB = b"\xff" * 4
QUERIES = (
    ("01-getinfo", b"getinfo abc123"),
    ("02-getinfo_repeat", b"getinfo abc123"),
    ("03-getstatus", b"getstatus abc123"),
)


def reference_replies(directory):
    """Check both the request and the response checksum against the run index."""
    index = json.loads((directory / "index.json").read_text(encoding="utf-8"))
    steps = {step["tag"]: step for step in index["steps"]}
    replies = []
    for tag, query in QUERIES:
        step = steps[tag]
        if step["request"].encode("ascii") != query or len(step["packets"]) != 1:
            raise ValueError(f"{tag}: unexpected reference request or packet count")
        packet = (directory / f"{tag}.resp0.bin").read_bytes()
        signature = {"len": len(packet), "sha256": hashlib.sha256(packet).hexdigest()}
        if step["packets"][0] != signature:
            raise ValueError(f"{tag}: reference checksum mismatch")
        replies.append(packet)
    return replies


def wait_for_server(address, timeout):
    # Use a separate socket so delayed readiness replies cannot enter the comparison.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(0.75)
        sock.connect(address)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                sock.send(OOB + b"getinfo ready")
                if sock.recv(65535).startswith(OOB + b"infoResponse\n"):
                    return
            except (TimeoutError, ConnectionRefusedError, ConnectionResetError):
                pass
            time.sleep(0.5)
    raise TimeoutError(f"No infoResponse from {address} within {timeout}s")


def compare(reference, port, timeout):
    expected = reference_replies(reference)
    address = ("127.0.0.1", port)
    wait_for_server(address, timeout)
    mismatches = 0
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect(address)
        for (tag, query), packet in zip(QUERIES, expected, strict=True):
            time.sleep(1.1)  # the same flood-protection gap as probe.py
            sock.send(OOB + query)
            sock.settimeout(2)
            try:
                response = sock.recv(65535)
            except TimeoutError:
                response = None
            if response != packet:
                print(f"{tag}: expected {packet!r}, received {response!r}")
                mismatches += 1
            else:
                print(f"{tag}: matched {len(packet)} bytes")
            sock.settimeout(0.6)  # the original probe's inter-packet idle window
            try:
                extra = sock.recv(65535)
            except TimeoutError:
                continue
            print(f"{tag}: unexpected extra datagram {extra!r}")
            mismatches += 1
    return mismatches == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="original probe.py run directory")
    parser.add_argument("--port", type=int, default=28960)
    parser.add_argument("--ready-timeout", type=float, default=300)
    args = parser.parse_args()
    try:
        return 0 if compare(args.reference, args.port, args.ready_timeout) else 1
    except (OSError, ValueError, KeyError) as error:
        print(f"Browser comparison failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
