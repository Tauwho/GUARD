# Linux check adapter

`tools/check.py` runs a scope report which probes `make package-check`. The P-owned
Makefile depends on `check` for that target, causing recursive checks on Linux.
Only while running Stream A CI, prepend this directory to PATH. The package probe
returns pending (exit 1); every actual format, policy, compatibility, ownership,
build and test stage still executes unchanged. No package success is fabricated.
Other make commands pass through to `/usr/bin/make`. Fresh-clone tests use the
normal PATH in a separate process session, with child exit status preserved.
