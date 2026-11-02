# Agent handoff

Planned boards and acceptance stages are maintained in [Roadmap](roadmap.md).
A roadmap entry does not imply a board is available through the build menu or machine status.

## Start of a work session

Read `README.md`, this file, `docs/status.md` and `docs/architecture.md`, then run:

```sh
make -f Makefile.agent handoff
make test
```

The handoff command is read-only. `target.boards` lists declared board identities with `name`, `architecture`, `platform`, `kernel_backend`, `boot_backend` and `available`; availability describes implementation, not hardware validation. It checks manifest/submodule definitions, reads source revisions and patch order, and reports next work. It does not compile or contact the device. A matching pin is not evidence of a successful build or boot; consult the recorded validation in `docs/status.md`.

## JSON interfaces

Invoke machine interfaces explicitly through `Makefile.agent`. Each emits a single JSON object on stdout. They do not edit the upstream sources or access the uConsole.

### handoff

`make -f Makefile.agent handoff` runs `tools/agent/handoff.py`.

Schema version 1 contains `schema_version`, `project`, `revision` (branch, commit, clean), `target`, `upstreams`, `kernel` (ordered `patch_series` and `patch_count`), `handoff_documents`, `next_actions` and `unknowns`. Each upstream entry contains `name`, `path`, `repository`, `ref`, `expected_commit`, `actual_commit`, `initialized` and `matches_pin`. An unborn parent repository is represented by revision commit `unborn`. Errors contain `schema_version`, `project` and `error`.

Script exits: 0 = both upstreams initialized at their pins; 1 = pin mismatch or Git failure; 2 = invalid project metadata; 3 = missing/uninitialized upstream. A dirty parent worktree is reported and does not itself fail the command. Submodule definitions and indexed Git links must match the manifest.

### kernel-artifact

`make -f Makefile.agent kernel-artifact` runs `tools/kernel_artifact.py` and validates the default CM5 report `kernel/build/current.json`. Pass `BOARD=uconsole-cm4` (script: `--board uconsole-cm4`) to validate `kernel/build/current-uconsole-cm4.json`. Board identity, the board-selected patch subset, configuration and artifact checksums must match.

Schema version 1 contains `schema_version`, `project` and `kernel`. The kernel object contains:

| Field | Meaning |
| --- | --- |
| `schema_version` | Kernel report format, currently 1 |
| `board_sha256` | Board declaration SHA-256; absent in original reports |
| `board` | Board target; absent only in the original CM5 report, interpreted as uconsole-cm5 |
| `kernel_release` | Built kernel release string |
| `linux_commit` | Fixed upstream commit |
| `patch_series` | Ordered patch filenames |
| `patch_sha256` | Filename → SHA-256 mapping |
| `config_sha256` | Merged kernel configuration SHA-256 |
| `config_fragment_sha256` | Project configuration fragment SHA-256 |
| `image_sha256` | SHA-256 of the deployed firmware kernel file; new Raspberry Pi builds use gzip, older artifacts may be raw Image |
| `artifact_directory` | Artifact directory relative to project root |
| `package_path` | Debian package path relative to project root |
| `package_sha256` | Debian package SHA-256 |

The Raspberry Pi backend builds Kbuild's `Image.gz` and stages those bytes
under the board's firmware filename (`kernel8.img` for CM4 or
`kernel_2712.img` for CM5). The firmware decompresses it before starting the
same kernel. `image_sha256` covers the compressed deployed file, matching the
package and image inspectors; its schema and validator exit behavior do not
change. Historical raw artifacts and reports retain their original bytes.

The package and artifact paths must remain under `kernel/build`. The validator checks the pin, board-selected ordered series, patch hashes, board configuration fragment hash, package, saved configuration and the board-specific staged Image. Errors contain `schema_version`, `project` and `error` instead of `kernel`. Script exits: 0 = valid current artifact; 2 = malformed/stale metadata, checksum mismatch or unreadable file; 3 = missing report or incomplete artifact. The configuration is also installed as `/boot/config-<release>`; `/boot/vmlinuz-<release>` links to the private firmware Image so standard initramfs enumeration finds the project kernel. A package-owned initramfs post-update hook synchronizes the firmware-prefix copy. Each board report uses the kernel object schema; it is published only after package creation succeeds. The package's embedded report omits package path/hash, since a package cannot include its own hash. The original CM5 external report was enriched with the matching board identity/hash when the board schema was introduced; its original package and Image bytes are unchanged.

GNU make normally returns 2 when a recipe fails, regardless of the script's exit status. Call the documented Python script directly when automation needs to distinguish these exit codes.

## Human tools and build exits

`./build.sh lunch` interactively selects board, rootfs provider, system profile and desktop, saving the choice in ignored `.build-config.json`. `./build.sh` prepares the selected backend, kernel and image in sequence; Ubuntu Base and Arch Base skip the pi-gen image container; `kernel`/`image` select a partial build, `show` displays the saved selection and `check` runs `make check`. The wrapper delegates to root Makefile targets. Its local configuration is internal state, not a machine status API. It does not deploy images.

The optional board schema-1 lists `image_profiles` and `image_desktops` restrict
both menu and direct profile selection. Omission preserves component-based
compatibility; supplied lists must contain unique nonempty identifiers.
`cm5-io-board` permits only `ubuntu-noble-base` / `none`; its kernel uses an empty
ordered patch subset. Use `BOARD=cm5-io-board` with the existing kernel artifact
interface to validate `kernel/build/current-cm5-io-board.json`. Handoff JSON
fields and all existing exit semantics are unchanged. Saved menu state is 0600.
It is written to a temporary file and atomically replaced; save/publication
failure preserves the previous valid selection.

`make knowledge-check` invokes `tools/knowledge_audit.py`: 0 = structural/local
link checks pass, 1 = document errors, 2 = invalid CLI/environment/read failure.
It requires TL;DR within 20 lines, fewer than 300 lines, unique level-2/3
headings, index coverage and existing local link targets. It does not establish
factual correctness, anchor validity or sanitization. `make check` includes it.
`make image-compress IMAGE=...` separately compresses an explicit regular image
with default zstd settings; Python CLI options also provide Zip64, dry-run and
optional checksum. `tools/serial_capture.py` captures only RX from an explicitly
selected UART when invoked. Both return 0 for operation success, 1 for tool/input
failure and 2 for CLI errors; capture also returns 130 on interruption. Full
dependencies, outputs and limitations are in tools/README.md（工程源码：`tools/README.md`）.
These human CLI tools add no stable JSON interface; GNU make retains its normal
recipe-failure exit behavior.

Use root `Makefile` for status, upstream initialization, patch checks, compilation, images and tests. `make kernel-container-module BOARD=uconsole-cm4 MODULE=drivers/gpu/drm/panel/panel-cwu50.o` configures the selected board as needed, then invokes Kbuild for only that source-relative object target; it does not build a kernel image, all modules, or a package. The target may be given as a `.c` source or `.o` object; absolute paths, parent traversal and other suffixes are rejected. `make kernel-container-build` remains the full image/modules/DTB and package build. `scripts/kernel_build.py` and `scripts/image_build.py` return 0 on success, 1 on build/tool failure, 2 on invalid project inputs and 3 on missing build environment/artifacts. `tools/kernel_patches.py` uses 0 for applicable series, 1 for pin/application failure, 2 for invalid metadata and 3 for missing source. `tools/image_inspect.py` returns 0 when artifact checks pass, 1 for inspection/tool failure and 3 for missing kernel artifacts or native inspection tools; its make wrapper has the usual recipe-failure behavior. These human tools print progress; their output is not a JSON API.

Builds write under ignored output/work directories. pi-gen preparation replaces only its managed snapshot; inspect an incomplete kernel worktree before removing it. Repeated builds can reuse compiler objects, while staging is recreated to avoid retaining obsolete modules. See `image/README.md` for host requirements, `docs/image-design.md` for OS/desktop profiles and `docs/board-design.md` for the generic board schema and non-Pi platform extension contracts.

`tools/kernel_candidate.py` is a human-facing offline diagnostic staging tool,
separate from ordinary builds. It defaults to preview, refuses host `/`, and
requires explicit `--apply` to create new candidate files while preserving
usrmerge and normal boot inputs. Its text output is not a machine API; allowed
archive contents, exit codes and recovery limits are documented in
[the staging contract](kernel-candidate.md). It does not activate or boot a kernel.

pi-gen image preparation requires a preset account password (`FIRST_USER_PASS`
in ignored `image/config.local`) and `DISABLE_FIRST_BOOT_USER_RENAME=1`. Missing
credentials fail with metadata exit 2 before a managed snapshot is replaced.
The default username is `pi`; SSH is enabled by default. No password is part of
the machine status interfaces.

In the current pi-gen backend, `ENABLE_CLOUD_INIT=0` also installs
`/etc/cloud/cloud-init.disabled` in the project stage. Upstream pi-gen only
skips seed-file installation for that flag. This uses the package's documented
runtime opt-out and preserves its installed files and units. With `1` or an
unset flag, the stage leaves the marker untouched. Account and SSH preparation
remain independent image-build steps; other rootfs backends own their policy.

`make image-check IMAGE_USER=pi` adds preset-account/SSH/wizard checks.
`IMAGE_VARIANT=cm4-carrier BOARD=uconsole-cm4` checks the separate generic CM4
carrier fixture and requires uConsole GPIO audio files to be absent. Both use
the existing image inspector exit behavior; neither changes or flashes images.

## Arch Base inputs and export

`make image-build PROFILE=arch-base BOARD=cm5-io-board DESKTOP=none` selects
Arch Linux ARM rolling/aarch64 with the project kernel and the stock CM5 IO
Board boot payload. The provider's optional `boards` list in
`image/components.toml` must be a nonempty list of unique board identifiers;
selection outside that list is invalid project input (exit 2). Arch Base
initially permits only CM5 IO Board and no desktop.

Its own `image/arch-base/config.toml` and ignored `config.local.toml` set the
account, hostname, locale, timezone and pacman mirror. Preparation hashes the
password and snapshots inputs into a private work directory. Its native builder
uses sudo, private mount namespaces and ARM64 chroot/QEMU, then exports a regular
DOS/FAT32/ext4 image through the shared exporter. It never flashes a device.
The project kernel's Debian archive is extracted as files; Debian maintainer
scripts and the dpkg package database are not used. Arch's mkinitcpio and an ALPM
hook own the initramfs. The exported image omits the build keyring; an enabled
first-boot service initializes unique local pacman keys from signed repository
keyring files. The archive's CA bundle link is retained without host trust input.

The existing image-build exit behavior applies: 0 success, 1 build/tool failure,
2 invalid project metadata and 3 missing environment/kernel artifacts; GNU make
retains normal recipe-failure behavior. The existing image inspector dispatches
on the selected provider and checks Arch packages, services, account and boot
references using read-only mounts. Neither build nor inspection establishes a
hardware boot. Input trust, internal artifact schemas and exact commands are in
the Arch Base instructions（工程源码：`image/arch-base/README.md`）.

## Ubuntu Base inputs and export

For the official CM5 IO Board, run `make image-build BOARD=cm5-io-board
PROFILE=ubuntu-noble-base` after building its matching kernel. This isolated
rootfs/IO development target reads `image/board/cm5-io-board/config.toml` plus
that directory's ignored `config.local.toml`; it does not consume the uConsole
Ubuntu local file. The public development defaults are `pi` / `1`, SSH enabled,
and GPIO14/15 UART0 console/login at 115200 8N1. Raw settings TOML is never copied
into prepared inputs; only the password hash enters the private specification.
Its image inspector rejects uConsole hardware payloads and verifies the UART
console/getty. The optional USB payload ships with its boot-time dwc2 Overlay
commented out and ADB marker absent. CM5IO inspection also checks the gadget,
DHCP, Netplan and ordinary-user adbd integration; it does not test enumeration
or establish host-OS compatibility. Activation and authentication limits are in
the [USB debug note](../knowledge/usb/cm5-io-board-usb-debug.md). Use the existing
`image-check` with `BOARD=cm5-io-board`,
`PROFILE=ubuntu-noble-base` and `IMAGE_USER=pi`. This does not establish a boot.

`make image-build PROFILE=ubuntu-noble-base BOARD=uconsole-cm4` selects the
native Ubuntu Base 24.04.5 ARM64 backend without a desktop. The profile defaults
to CM4; CM5 is selectable, with verification tracked independently. Ubuntu
Desktop/XFCE/KDE remain unavailable. Its settings use
`image/ubuntu-base/config.toml` plus ignored `config.local.toml`, not pi-gen
shell configuration. `password` is required; `username` defaults to `pi`.
Optional Wi-Fi, APT mirror and proxy settings stay in that local file.

The default selected kernel is still checked against current source/configuration
by `load_kernel_artifact()`. For an explicit rootfs trial, local `kernel_report`
and `kernel_report_sha256` select a named historical report together. The
retained validator checks that report's exact bytes, current board/upstream/series
identity and saved package/configuration/Image hashes. It accepts the report's
historical patch/configuration hashes; it does not assert that those inputs
match current source, and it does not alter the strict kernel-artifact command.
The rootfs builder installs the current release-matched initramfs hook before
configuring the unchanged kernel package and records the hook hash separately.

The backend runs rootfs assembly with `sudo -n unshare --mount --propagation
private` and ARM64 chroot execution. x86 hosts need binfmt/QEMU with the F
flag. The exporter formats and checks regular FAT/ext4 files, assembles a
DOS-partitioned `.img` without loop/device access and publishes it atomically
with mode 0600. Work directories and local settings can contain credentials;
status commands must not print them. Ubuntu always retains initramfs and has
its own first-boot root expansion and SSH host-key services.

The adjacent `.json` receipt has schema version 1 and records `profile`, `base`,
`kernel`, `retained_kernel`, `input_sha256`, `builder_sha256`, `exporter_sha256`,
`initramfs_hook_sha256`, `layout` and `image_sha256`. A sibling `.packages.txt`
records actual package versions. These are internal build artifacts rather
than a new status interface. Field meanings, configuration and output names
are documented in the Ubuntu Base instructions（工程源码：`image/ubuntu-base/README.md`）.
The existing image-build exit behavior applies: 0 success, 1 tool/build failure,
2 invalid configuration/metadata and 3 missing environment or kernel input;
GNU make retains its normal recipe-failure behavior.

Inspect with `make image-check PROFILE=ubuntu-noble-base BOARD=uconsole-cm4
IMAGE_USER=pi IMAGE=path/to/image.img`. When the image uses retained input,
pass `IMAGE_KERNEL_REPORT` and `IMAGE_KERNEL_REPORT_SHA256` together so the
inspector checks the same selection. Native inspection uses read-only loop
and filesystem mounts; ext4 is mounted without journal replay. That inspection
is separate from the file-only exporter and does not establish a hardware boot.

## CM4 direct-root policy

The pi-gen CM4 image starts with the managed initramfs include enabled. Its
`uconsole-direct-root.timer` runs a conservative layout/kernel checker 60 s
after boot. First-boot/custom initialization parameters and unsupported layouts
retain initramfs. A later eligible normal boot selects direct MMC/ext4 root
for subsequent boots without rebooting or removing the recovery initrd. CM5
and other backends do not install this policy. Image inspection requires the
initial initramfs policy, exact script/unit/timer payloads, and the executable
kernel initramfs post-update hook generated for the installed release. This
rejects cached kernel packages whose older hook cannot restore initramfs loading
before replacing the recovery initrd.

The runtime checker is internal image behavior, not an agent JSON interface.
It logs the selection/reason and exits 0 for direct selection, a conservative
skip or restoration of the default; policy/lock file I/O failure exits 1.
The initramfs post-update hook uses the same lock, restores the managed default
and creates a current-boot update marker, preventing same-release updates from
being approved again before a successful reboot. The
[boot-time note](../knowledge/boot/cm4-boot-time.md) records source and device evidence.

## Desktop battery package inputs

Raspberry Pi Desktop preparation consumes the internal cached artifact
`image/work/wfplug-batt/packages/wfplug-batt_1.11+uconsole1_arm64.deb` and its
sibling `.json` receipt. Lite preparation does not require it. The human
`make image-battery-package` command builds the package natively on ARM64
from two pinned official Raspberry Pi 1.11 inputs; it does not install the
package, fetch dependencies or contact a device. See
`image/packages/wfplug-batt/README.md` for its environment and input hashes.

Receipt schema version 1 contains `schema_version`, `version`, `architecture`,
`source_sha256`, `upstream_package_sha256`, `patch_sha256`, `builder_sha256`,
`backend_check_sha256` and `package_sha256`. All checksum values are lowercase
SHA-256 hex strings. Preparation checks the identity and pinned inputs, the
current project patch/build/checker bytes and the resulting package bytes
before replacing its managed work snapshot. The `.deb` is copied into the
project-owned export step after pi-gen's final distribution upgrade; the
export hook and package participate in the image work identity.

This receipt is build input metadata, not a public status command. The native
builder prints progress and its final package path; it exits 0 on success and
1 on build, tool or input-check failure. Missing desktop package artifacts use
the existing image tool environment exit 3; stale/malformed receipts and hash
mismatches use metadata exit 2. The make wrappers retain normal GNU make
recipe-failure behavior.

## Experimental Omarchy core package

`make image-omarchy-package` delegates to
`image/packages/omarchy/build.py INPUT_DIR OUTPUT_DIR --work-dir WORK_DIR`.
`OMARCHY_INPUTS`, `OMARCHY_PACKAGE_DIR` and `OMARCHY_WORK_DIR` select local paths.
This offline data repack neither downloads inputs nor compiles native code,
installs packages, contacts a device or selects an image desktop. Its CLI exits
0 on success, 1 on input/tool/build failure and 2 on argument errors; GNU make
retains its own recipe-failure exit behavior.

The internal receipt schema 1 records `build_method` (`data-repack`),
`package` (name/version/architecture), `unsigned`, `inputs_sha256`, `signer_fingerprint`, `dependencies`,
`recipe_sha256`, `builder_sha256`, `validator_sha256`, `payload_sha256`,
`package_sha256`, `manifest` and `source_manifest`. `payload_sha256` covers the
project-owned public keys, detached signatures, license and compatibility patch.
The manifest records each archive path/type/mode/root owner, file size/hash or
symlink target. The source manifest also records the original package name and
the version helper's original digest. A sibling `.sha256` binds the output name
and bytes. `tools.omarchy_package.load_omarchy_package(root, directory=None)`
returns a `Path` only after current tool/recipe and archive validation; absent
artifacts raise `EnvironmentProblem`, stale or invalid metadata raises
`ProjectMetadataError`. These are local build receipts, not independently signed
attestations authenticating a third party's repack. Deployment authenticates the
trusted local output separately while retaining required pacman signatures;
see the package instructions（工程源码：`image/packages/omarchy/README.md`）.

## Manual Omarchy session

`image/desktops/omarchy/prepare.py OUTPUT --username pi --uid 1000 --home /home/pi`
generates eight session files and a schema-1 `manifest.json` with `settings`,
`template_sha256` and `files` (path/mode/SHA-256). The apply tool recomputes that
manifest against current templates and requires an explicit `--target-root`.
Preview is the default; `--apply` requires root. Live `/` additionally requires
`--live-board` on the CM5; offline roots must identify Arch Linux ARM. Both CLIs
exit 0 on success, 1 on validation/file failure and 2 on usage errors. They do
not connect to a device, enable/start units or reboot. Runtime packages, seeded
user settings and generated theme are checked prerequisites; user configuration
and service reload/start remain explicit steps in the
manual-session instructions（工程源码：`image/desktops/omarchy/README.md`）.

These internal file schemas are not new stable project-status APIs.
`arch-base` continues to select `desktop=none`; ordinary image builds do not
consume these experimental outputs.

## Experimental Ubuntu desktop inputs

`make image-mesa-noble-package` builds an optional libraries-only Mesa package
on native Ubuntu 24.04 ARM64 from three fixed offline Ubuntu source archives.
It neither installs the output nor changes image/session selection. Its CLI
exits 0 on success, 1 on build/input/tool failure and 2 on argument errors.
`MESA_NOBLE_INPUTS`, `MESA_NOBLE_PACKAGE_DIR`, `MESA_NOBLE_WORK_DIR` and
`MESA_JOBS` configure the human make wrapper.

Its receipt schema 1 records `package` (name/version/architecture),
`source_version`, `sources_sha256`, `series_sha256`, `patch_sha256`,
`native_options`, `recipe_sha256`, `builder_sha256`, `validator_sha256`,
`environment_helper_sha256`, `source_tree_sha256`, `build_environment`,
`runtime_packages`, `depends`,
`library_closure` and `package_sha256`. The build environment records the
Noble OS/architecture, compiler identities, tools, native package inventory and
pkg-config dependencies. These fields describe build provenance, not a public
status interface or proof of display output. The validator rejects missing
inputs as an environment problem and stale/malformed metadata as a project
metadata problem, using the same exception types as other native packages.

`make image-wayland-package` and `make image-noctalia-package` build offline
native Ubuntu 24.04 ARM64 packages; neither command installs packages, starts
sessions or contacts a device. Builders exit 0 on success, 1 on build/input/tool
failure and 2 on CLI errors. GNU make retains its usual recipe-failure exit.

Wayland receipt schema 1 records `package`, `version`, `architecture`, `prefix`,
`sources_sha256`, `recipe_sha256`, `builder_sha256`, `validator_sha256`,
`package_sha256`, `build_environment` and `depends`. Native environment includes
machine, OS, C/C++ compiler commands/targets/versions and package inventory.
Noctalia receipt schema 1 records a `package` identity object, `inputs_sha256`,
`native_options`, `effective_native_options`, the three recipe/builder/validator hashes,
`patches_sha256`, `build_environment`, `validation`, `elapsed_seconds` and
`package_sha256`. `effective_native_options` comes from Meson's build-option
introspection after setup and before compilation. Both option objects must
match the current recipe; missing or different effective options invalidate the
cache. The release recipe uses optimization `3`. Its validation includes
version, ELF machine, `private_runpath` and resolved runtime packages. These are build metadata,
not public machine status commands or evidence of working desktop output.

The Waybar profile requires a validated package cache. The internal
`tools/desktop_packages.py` facade supplies profile availability, Ubuntu desktop
preparation and desktop image inspection. Fixed `.deb` inputs are tracked with
Git LFS under `prebuilt/ubuntu-noble/arm64/`; their sibling JSON receipts use
ordinary Git and keep the existing schemas. If the native cache is absent or
empty, the facade atomically populates it from the tracked pair before calling
the unchanged package validator. Any nonempty native cache takes precedence,
including stale or incomplete inputs; it is not overwritten on failure.
There is no implicit download or native build. Missing packages or receipts and
unexpanded LFS pointers use the image tools' environment exit 3; pointers include
a `git lfs pull` hint. Stale or malformed metadata and checksum mismatches use
exit 2. GNU make retains its normal recipe-failure behavior. This facade adds
no public machine-readable interface.
Noctalia is selectable through `ubuntu-noble-labwc-noctalia` or
`DESKTOP=labwc-noctalia` and requires both native package caches. Waybar remains
the default labwc profile. Ubuntu image receipt schema 1 additionally records
`desktop` (null for headless builds); desktop plans include source hashes,
package/receipt identity, runtime packages, username and output configuration:
`output_transform` is `"270"` and `output_scale` is the number `1.5` for current
uConsole Ubuntu desktop plans. The shared installer's standalone default scale
is `1.0`; it rejects nonfinite or nonpositive values before writing files.
`stack_package` records the compositor package; Noctalia plans also record
`noctalia_package` with the same path/hash/receipt fields. Preparation snapshots
`wayland-stack.deb` and, when selected, `noctalia.deb`; both hashes are checked
before installation. An unselected Noctalia input is rejected. Installation
passes scoped dpkg path-includes for each native package's `/usr/share/doc/`
directory, retaining bundled licenses despite Ubuntu Base's documentation
exclusion without changing the global dpkg policy.
`make image-check` accepts the same `DESKTOP` override as image building and
validates payload bytes, permissions, installed package identity and greetd.
The CM4 Noctalia session has completed real lock/unlock, graphical polkit,
FileChooser portal, bidirectional UTF-8 clipboard, launcher and session-exit
roundtrips; the user confirms normal audio and improved menu responsiveness.
Matched warm-session memory observations do not measure cold boot or frame rate,
and do not establish CM5 validation. See the [desktop comparison
note](knowledge/desktop/labwc-shell-comparison.md) for measurements, evidence and
remaining limits. Image preparation consumes validated package
caches and receipts without invoking native compilation. Source, patch, recipe,
builder or validation identity changes invalidate the corresponding package cache.
Package reuse and the separate board adaptation boundary are documented in the
[build knowledge note](../knowledge/build/userspace-package-reuse.md).

## Validation boundary

Tests cite a documented oracle. Source checks, compilation, device-tree application, artifact inspection and real board behavior are separate evidence levels. Record observations, conclusions and unknowns separately. Generating an image never authorizes deployment; do not flash, reboot or change device configuration as ordinary verification.
