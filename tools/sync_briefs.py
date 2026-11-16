#!/usr/bin/env python3
"""Rewrite the Due and Tier cells of the brief milestone tables from SCOPE.yaml.

Work and Acceptance prose is never touched. A missing or extra row is an error:
add the row by hand, then run this again (audit 4).
"""

import argparse
import sys

import brief_sync


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="do not write; fail on drift")
    args = parser.parse_args()
    if args.check:
        errors = brief_sync.check_all()
    else:
        print("sync: rewriting Due and Tier from SCOPE.yaml")
        errors = brief_sync.sync_all()
    if errors:
        print("brief-sync: FAIL")
        for error in errors:
            print(f"  {error}")
        return 1
    print("brief-sync: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
