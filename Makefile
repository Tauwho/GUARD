# Thin wrapper; the logic lives in tools/check.py so Windows (no make) runs the same thing.
PY ?= python3

.PHONY: check check-fast scope demo-smoke package-check

check:
	$(PY) tools/check.py

check-fast:
	$(PY) tools/check.py --fast

scope:
	$(PY) tools/scope_gate.py

demo-smoke:
	bash tools/fresh_clone_demo.sh main

# P's final gate. tools/check_readme_links.py and tools/check_package.py are P's (stream P, milestone P4).
package-check: check
	bash tools/fresh_clone_demo.sh $${REF:-v1.0-rc1}
	$(PY) tools/check_readme_links.py
	$(PY) tools/check_package.py
