"""Spike S6 (#32) host side: accept one TCP stream on port 7700 and check it.

Run on the host before the target sends:  python3 spike/s6/s6_host.py [count]
Prints a yes/no answer and line counts only, no timing numbers.
"""

import json
import socket
import sys


def main() -> int:
    expected = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    with socket.create_server(("0.0.0.0", 7700)) as srv:
        srv.settimeout(300)
        print("s6_host: listening on 0.0.0.0:7700", flush=True)
        conn, addr = srv.accept()
        with conn, conn.makefile("r", encoding="utf-8") as stream:
            seqs = []
            bad = 0
            for line in stream:
                try:
                    seqs.append(json.loads(line)["seq"])
                except (ValueError, KeyError, TypeError):
                    bad += 1
    in_order = seqs == list(range(len(seqs)))
    ok = len(seqs) == expected and bad == 0 and in_order
    print(f"s6_host: peer={addr[0]} lines={len(seqs)} expected={expected} bad={bad}")
    print(f"s6_host: in order={'yes' if in_order else 'no'}")
    print(f"S6 host received the target stream intact: {'yes' if ok else 'no'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
