#!/usr/bin/env python3
"""Fail if a brief milestone table drifts from SCOPE.yaml (audit 4)."""

import argparse
import sys

import brief_sync


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    errors = brief_sync.check_all()
    if errors:
        print("brief-sync: FAIL")
        for error in errors:
            print(f"  {error}")
        print("Run: python tools/sync_briefs.py (then add any missing row by hand).")
        return 1
    print("brief-sync: PASS (A/B/P milestone ids, tiers and dates match SCOPE.yaml)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
