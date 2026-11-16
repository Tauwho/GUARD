# GUARD developer setup

Goal: every member builds with `qcc`, boots a QNX 8.0 x86_64 target on QEMU, and commits a setup evidence bundle (issue #30, spike S1). Steps marked **(verify)** have not yet been run end to end by the team. Whoever runs them first corrects this file in the same PR as their evidence.

## 1. Licence and SDP (everyone, own licence)
1. Create a myQNX account at qnx.com and request the free **non-commercial** licence. Use your own; never share or commit the key (AGENTS.md §2).
2. Install **QNX Software Center** for your host OS and sign in.
3. Install **QNX SDP 8.0** (latest update). Note the version string for `manifest.json`.
4. Also install the **QNX SDP 8.0 Quick Start Target Image (QSTI) for QEMU** package. It supports Ubuntu 22.04 and 24.04 hosts.
5. Each licence may be used on at most 2 machines you own or control.

Never copy anything from the SDP install folder (`qnx800/`, `qnxsdp-env.*`, headers, libraries, images) into this repo. `.gitignore` and the `no-qnx-files` CI check catch the common cases.

## 2. Host options
| Host | Status | Notes |
|---|---|---|
| Ubuntu 22.04/24.04 with KVM | Primary | Fastest. Needs a bridge for ssh (§3) |
| Windows 11 + WSL2 (Ubuntu) | Supported | Follow the official QNX WSL2 codelab. Nested virtualisation gives `/dev/kvm` in WSL2 **(verify)** |
| VMware / VirtualBox Ubuntu VM | Fallback | Enable nested VT-x/AMD-V. Without it QEMU runs in TCG mode, which is slow but works |

## 3. Host packages and bridge (Ubuntu or WSL2)
```sh
sudo apt install qemu-system-x86 qemu-utils bridge-utils cmake ninja-build openssh-client python3
sudo usermod -aG kvm "$USER"            # log out and in again
```
The QEMU bridge helper needs a bridge it is allowed to use **(verify the bridge name your image expects; `br0` is assumed)**:
```sh
sudo ip link add br0 type bridge && sudo ip addr add 10.10.10.1/24 dev br0 && sudo ip link set br0 up
echo "allow br0" | sudo tee /etc/qemu/bridge.conf
sudo chmod u+s /usr/lib/qemu/qemu-bridge-helper
```
The bridge is lost on reboot. Re-run the `ip` lines, or use netplan if you want it permanent.

## 4. Check the host
```sh
source ~/qnx800/qnxsdp-env.sh            # path depends on your install
python3 tools/env-check.py               # all checks
python3 tools/env-check.py --build-only  # build host without QEMU
```
Every FAIL line says what to fix. WARN lines are allowed. The output prints no paths or licence data, so you may paste it into `transcript.txt`.

## 5. Boot the target
**Option A, QSTI for QEMU.** Start it with the launch script shipped in the QSTI package, as described in that package's README **(verify and record the exact command here)**.

**Option B, `mkqnximage`** (part of SDP 8.0):
```sh
mkdir -p ~/guard-target && cd ~/guard-target     # outside the repo
mkqnximage --type=qemu --arch=x86_64 --build --run
```
Use `mkqnximage --help` for options; record any you change. Image files stay outside the repo.

Find the target IP from the target console (`ifconfig`), then:
```sh
ssh qnxuser@<target-ip>          # default users depend on the image (verify)
scp ./hello qnxuser@<target-ip>:/tmp/
```

## 6. First program (S1)
```sh
printf '#include <stdio.h>\nint main(void){puts("hello qnx");return 0;}\n' > /tmp/hello.c
qcc -Vgcc_ntox86_64 -o /tmp/hello /tmp/hello.c
scp /tmp/hello qnxuser@<target-ip>:/tmp/ && ssh qnxuser@<target-ip> /tmp/hello
```

## 7. Evidence capture v0 (manual)
Protocol: `evidence/README.md`. Bundle path `evidence/<yyyymmdd>-<sha7>-<topic>/`, where `sha7` is the commit you built from and `topic` is `setup-po`, `setup-a` or `setup-b`.
```sh
B=evidence/$(date -u +%Y%m%d)-$(git rev-parse --short=7 HEAD)-setup-b && mkdir -p "$B"
script -q -c "ssh qnxuser@<target-ip>" "$B/transcript.txt"   # run: uname -a; pidin; /tmp/hello; exit
ssh qnxuser@<target-ip> uname -a   > "$B/uname.txt"
ssh qnxuser@<target-ip> pidin      > "$B/pidin.txt"
ssh qnxuser@<target-ip> slog2info  > "$B/slog2.txt"
{ file /tmp/hello; readelf -h /tmp/hello; sha256sum /tmp/hello; } > "$B/binaries.txt"
```
Then write `manifest.json` by hand. Fill the fields listed in `evidence/README.md`, plus a `files` list naming every other file in the bundle. The exact shape is `contracts/evidence-manifest.schema.json` (C13), still pending in #3. Until it merges, CI rejects any manifest, so keep the bundle on your branch.

Before committing:
- Read every file. Remove licence identifiers, serial numbers, your home path and any personal data.
- No binaries: `hello` itself is never committed. The bundle must stay under 1 MiB.
- Run `python3 tools/check_policy.py all`.

## 8. Troubleshooting
| Symptom | Fix |
|---|---|
| `qcc: command not found` | Source `qnxsdp-env.sh` in this shell |
| `qcc -V` lacks `gcc_ntox86_64` | Reinstall SDP 8.0 with the x86_64 target |
| QEMU very slow | No KVM. Check `/dev/kvm` and the `kvm` group |
| `failed to create tun device` / bridge helper error | Re-check §3: `bridge.conf`, setuid helper, bridge up |
| ssh times out | Target and bridge are on different subnets; check `ifconfig` on the target |
| Licence errors in Software Center | Sign in again; check the licence is activated on this machine |

Ask in issue #30 if a step fails, and add the fix to this table.
