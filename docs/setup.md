# GUARD developer setup

Goal: every member builds with `qcc`, boots a QNX 8.0 x86_64 target on QEMU, and commits a setup evidence bundle (issue #30, spike S1). Steps marked **(verify)** have not yet been run end to end by the team. Whoever runs them first corrects this file in the same PR as their evidence.

Verified end to end on 2026-11-16 by B: Windows 11, WSL2 Ubuntu 24.04, SDP 8.0, QEMU + KVM, `mkqnximage` (Option B in §5). Other hosts still need a run.

## 1. Licence and SDP (everyone, own licence)
1. Create a myQNX account at qnx.com and request the free **non-commercial** licence. Use your own; never share or commit the key (AGENTS.md §2).
2. Install **QNX Software Center** for the OS that will run the SDP. On WSL2 that is the **Linux** installer (`qnx-setup-<ver>-linux.run`), run inside Ubuntu, not the Windows `.exe`:
   ```sh
   cp /mnt/c/Users/<you>/Downloads/qnx-setup-*-linux.run ~/ && cd ~ && chmod +x qnx-setup-*-linux.run && ./qnx-setup-*-linux.run
   ~/qnx/qnxsoftwarecenter/qnxsoftwarecenter &     # default install path ~/qnx
   ```
3. Sign in, click **Add Installation**, pick **QNX Software Development Platform 8.0**, install to `~/qnx800`. Keep the default packages. If the wizard is taller than the screen and Next is hidden, restart it with `GDK_DPI_SCALE=0.7`.
4. **(verify)** The **QSTI for QEMU** package is only needed for Option A in §5. Option B needs only SDP 8.0.
5. Each licence may be used on at most 2 machines you own or control.

Never copy anything from the SDP install folder (`qnx800/`, `qnxsdp-env.*`, headers, libraries, images) into this repo. `.gitignore` and the `no-qnx-files` CI check catch the common cases.

## 2. Host options
| Host | Status | Notes |
|---|---|---|
| Ubuntu 22.04/24.04 with KVM | Primary | Fastest. Needs a bridge for ssh (§3) **(verify on bare metal)** |
| Windows 11 + WSL2 (Ubuntu 24.04) | Verified | `/dev/kvm` is present in WSL2. Use the Linux Software Center inside WSL (§1) |
| VMware / VirtualBox Ubuntu VM | Fallback | Enable nested VT-x/AMD-V. Without it QEMU runs in TCG mode, which is slow but works |

## 3. Host packages and bridge (Ubuntu or WSL2)
```sh
sudo apt update && sudo add-apt-repository -y universe     # ninja-build is in universe
sudo apt install qemu-system-x86 qemu-utils bridge-utils cmake ninja-build g++ openssh-client python3
sudo usermod -aG kvm "$USER"            # then log out and in (WSL2: `wsl --shutdown` in PowerShell)
```
The QEMU bridge helper needs a bridge it is allowed to use:
```sh
sudo ip link add br0 type bridge && sudo ip addr add 10.10.10.1/24 dev br0 && sudo ip link set br0 up
sudo mkdir -p /etc/qemu && echo "allow br0" | sudo tee /etc/qemu/bridge.conf
sudo chmod u+s /usr/lib/qemu/qemu-bridge-helper
```
The bridge is lost on reboot (WSL2: on `wsl --shutdown`). Re-run the `ip` lines. `br0` shows `DOWN` until a target attaches; that is fine.

## 4. Check the host
```sh
source ~/qnx800/qnxsdp-env.sh            # path depends on your install
python3 tools/env-check.py               # all checks
python3 tools/env-check.py --build-only  # build host without QEMU
```
Every FAIL line says what to fix. WARN lines are allowed. The output prints no paths or licence data, so you may paste it into `transcript.txt`.

## 5. Boot the target
**Option A, QSTI for QEMU.** Start it with the launch script shipped in the QSTI package, as described in that package's README **(verify and record the exact command here)**.

**Option B, `mkqnximage`** (part of SDP 8.0, verified):
```sh
[ -f ~/.ssh/id_ed25519 ] || ssh-keygen -q -t ed25519 -N '' -f ~/.ssh/id_ed25519   # local key, enables root login
mkdir -p ~/qsti && cd ~/qsti     # outside the repo; must be empty the first time
OPT_NETWORK=bridge:br0 mkqnximage --type=qemu --arch=x86_64 --ip=10.10.10.2 \
    --ssh-ident="$HOME/.ssh/id_ed25519.pub" --build --run=-h
ssh root@10.10.10.2 uname -a     # wait ~20 s after boot
```
- `OPT_NETWORK=bridge:br0` and a static `--ip` avoid the default `nat` mode, which needs DHCP (`dnsmasq` or libvirt's `virbr0`) on the bridge.
- `--run=-h` runs headless in the background; `--run` alone attaches the console (quit with `Ctrl-a x`). Later boots: `mkqnximage --run=-h` in the same directory. Stop: `mkqnximage --stop`.
- WSL2: a headless target started from a one-shot `wsl -- ...` command dies when that command ends. Start it from an open Ubuntu terminal, or with `setsid nohup mkqnximage --run=-h ... &`.
- On the image, `/tmp` is the 64 MiB `/data` partition. `nc` and `traceprinter` are not installed.

**Option C, no sudo: QEMU user-mode networking** (verified by the PO on Windows 11 + WSL2 Ubuntu 24.04 with KVM, 16 Nov, #81). Option B needs sudo for the `br0` bridge (§3). Without sudo, build the image with `mkqnximage` and start QEMU yourself:
```sh
ssh-keygen -q -t ed25519 -N '' -f ~/.ssh/guard_target_ed25519     # key only for the target
mkdir -p ~/guard-target && cd ~/guard-target
mkqnximage --type=qemu --arch=x86_64 --hostname=guard-<you> --sshd-pregen=yes \
  --ssh-ident="$HOME/.ssh/guard_target_ed25519.pub" --build
setsid qemu-system-x86_64 -enable-kvm -cpu host -smp 2 -m 1G \
  -drive file=output/disk-qemu.vmdk,if=ide,id=drv0 \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 -device virtio-net-pci,netdev=net0 \
  -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-pci,rng=rng0 \
  -kernel output/ifs.bin -serial file:output/qemu.out -display none < /dev/null > /dev/null 2>&1 &
sleep 45 && tail -3 output/qemu.out                                  # expect "Startup complete"
ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1 uname -a
```
- Drop `-enable-kvm -cpu host` if `/dev/kvm` is missing (slower, TCG mode).
- `scp` fails on this image (no sftp server). Copy files with ssh instead: `ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1 'cat > /tmp/hello && chmod +x /tmp/hello' < /tmp/hello`.
- In §6-§8, replace `ssh qnxuser@<target-ip>` with `ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1`.

## 6. First program (S1)
```sh
printf '#include <stdio.h>\nint main(void){puts("hello qnx");return 0;}\n' > /tmp/hello.c
qcc -Vgcc_ntox86_64 -o /tmp/hello /tmp/hello.c
scp /tmp/hello root@10.10.10.2:/tmp/ && ssh root@10.10.10.2 /tmp/hello
```
Socket programs also need `-lsocket`.

## 7. Evidence capture v0 (manual)
Protocol: `evidence/README.md`. Bundle path `evidence/<yyyymmdd>-<sha7>-<topic>/`, where `sha7` is the commit you built from and `topic` is `setup-po`, `setup-a` or `setup-b`.
```sh
T=root@10.10.10.2
B=evidence/$(date -u +%Y%m%d)-$(git rev-parse --short=7 HEAD)-setup-b && mkdir -p "$B"
script -q -c "ssh $T 'uname -a; pidin; /tmp/hello'" "$B/transcript.txt"
ssh $T uname -a   > "$B/uname.txt"
ssh $T pidin      > "$B/pidin.txt"
ssh $T slog2info  > "$B/slog2.txt"
{ file /tmp/hello; readelf -h /tmp/hello; sha256sum /tmp/hello; } > "$B/binaries.txt"
```
Then write `manifest.json` per `contracts/evidence-manifest.schema.json` (C13): `version`, full 40-char `sha`, `date`, `topic`, `runner_role`, `host_os`, `sdp_version`, `qcc_version` (first line of `qcc -Vgcc_ntox86_64 --version`), `image_id` (≤ 120 characters) and a `files` list naming every other file in the bundle. The `sha` may be your PR branch commit; it stays valid after a squash merge (D21).

Before committing:
- Read every file. Remove licence identifiers, serial numbers, your home path, ssh key fingerprints and any personal data.
- No binaries: `hello` itself is never committed. The bundle must stay under 1 MiB.
- Run `python3 tools/check_policy.py all`.

## 8. Core tests on the target (B-2, spike S2)
The same doctest binary must pass on the host and on the target. Toolchain: `cmake/qnx-x86_64.cmake`, preset `qnx-x86_64`.
```sh
cmake --preset host-debug && cmake --build --preset host-debug && ctest --preset host-debug
source ~/qnx800/qnxsdp-env.sh
python3 tools/env-check.py --build-only
cmake --preset qnx-x86_64 && cmake --build --preset qnx-x86_64
scp build/qnx-x86_64/guard_core_tests root@10.10.10.2:/tmp/
ssh root@10.10.10.2 /tmp/guard_core_tests
```
The target run must end with doctest's `Status: SUCCESS!` line. Capture `evidence/<yyyymmdd>-<sha7>-core-on-target/` as in §7. Put `binaries.txt` for `build/qnx-x86_64/guard_core_tests`. Put the doctest summary counts (test cases, assertions, failures) for host and target into `results.json`. First run: `evidence/20261116-161b1e8-core-on-target/`.

## 8a. GUARD image (B-9)
`image/run-demo.sh` builds the GUARD image if needed (`image/build-image.sh`), boots it in the background and waits for ssh. No sudo with the default user-mode networking (Option C ports: ssh on `127.0.0.1:2222`, key `~/.ssh/guard_target_ed25519`):
```sh
source ~/qnx800/qnxsdp-env.sh
image/run-demo.sh                 # --net bridge for br0 (Option B), --test adds guard-hog, --stop
image/run-demo.sh --dashboard     # also starts dashboard/relay.py and runs F0: open http://127.0.0.1:8765
```
Build output stays in `image/out/` (git-ignored). Processes not in the repo yet are skipped; see `image/README.md`.

## 9. Troubleshooting
| Symptom | Fix |
|---|---|
| `qcc: command not found` | Source `qnxsdp-env.sh` in this shell |
| `qcc -V` lacks `gcc_ntox86_64` | Reinstall SDP 8.0 with the x86_64 target |
| `Package 'ninja-build' has no installation candidate` | `sudo apt update` and enable `universe` (§3) |
| `No CMAKE_CXX_COMPILER could be found` (host preset) | `sudo apt install g++` |
| QEMU very slow | No KVM. Check `/dev/kvm` and the `kvm` group |
| `The current directory is neither that of an existing mkqnximage virtual image nor ... empty` | Run `mkqnximage` in an empty directory (§5) |
| `dnsmasq not running on br0 for DHCP service` | Use `OPT_NETWORK=bridge:br0` and `--ip=10.10.10.2` (§5) |
| `failed to create tun device` / bridge helper error | Re-check §3: `bridge.conf`, setuid helper, bridge up |
| ssh: `No route to host` | The target is not running (WSL2: see §5) or not on `br0`. Check `pgrep -fa qemu-system` |
| scp: `write remote ... Failure` | `/tmp` (the 64 MiB `/data`) is full; delete old files on the target |
| Software Center wizard buttons off screen | Start it with `GDK_DPI_SCALE=0.7` (§1) |
| Licence errors in Software Center | Sign in again; check the licence is activated on this machine |

If a step fails, ask in issue #30 and add the fix to this table.
