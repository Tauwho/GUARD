# GUARD limits

What GUARD is not. This list is the single source: the README "Limits" block is generated
from it by `tools/build_readme.py`, and `build_readme.py --check` fails if they differ.

## Limits
<!-- claims-ok: negated -->
- No certified functional safety. GUARD claims no compliance with ISO 13849, IEC 61508, ISO 3691-4 or IEC 62304, and no SIL or PL rating.
- No real actuators, braking or vehicles. This is a simulation-only prototype.
- No hard real-time guarantee on hardware. QEMU timing is not hardware timing.
- No formal verification. Assurance comes from unit, scenario and property tests only.
- No defence against a compromised kernel, supervisor or gate. Only the planner is treated as untrusted.
- No QNX performance claims and no QNX-vs-Linux comparisons without BlackBerry's written approval.

## Related
- [parameters.md](parameters.md): every default value, with its source or marked as an assumption.
- [architecture.md, REF-CLAIMS](architecture.md#ref-claims): banned phrases and preferred wording.
