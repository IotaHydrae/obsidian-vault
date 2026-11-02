# Architecture

## Source ownership

`upstream/linux` and `upstream/pi-gen` are pinned official submodules for the Raspberry Pi backend. The manifest, `.gitmodules` and staged Git links must agree. Never commit adaptations inside those submodules. pi-gen uses its `arm64` branch: its master branch builds armhf and is unsuitable for these CM4/CM5 images. Other platform backends own their source and firmware pins.

`board/<target>/board.toml` declares architecture, platform, kernel/boot backends and board payload. The common model is not limited to Raspberry Pi modules. `kernel/patches/series` owns patch order; each Pi target selects an ordered subset, and `kernel/config/` owns its configuration changes. `image/profiles/` selects the rootfs provider, desktop and board independently. `image/board/` owns image payloads; `image/stage-uconsole/` adapts them to pi-gen. Backend-specific logic lives in `scripts/kernel_backends/` and `scripts/image_backends/`. See [board/platform design](board-design.md) and [image composition](image-design.md). Machine-readable interfaces are exposed through `Makefile.agent`.

Provider components may declare a `boards` allowlist in `image/components.toml`.
This limits a rootfs provider to implemented board combinations independently
of desktop support; Arch Base initially allows only `cm5-io-board`.

## Kernel path

The kernel frontend dispatches by the board's kernel backend. The current Raspberry Pi backend verifies the upstream pin and applies the selected series cumulatively to a private Git index. It then creates a detached build worktree under ignored `kernel/build/`; the upstream checkout stays clean. The source identity hashes the pin and patch contents; object and artifact directories also distinguish boards. Configuration starts from the board's defconfig (bcm2711 for CM4, bcm2712 for CM5), merges its project fragment, runs olddefconfig, and checks each requested enabled/disabled setting.

Four focused patches supply the shared CWU50/OCP8178 drivers and separate CM4/CM5 overlays. Each uConsole target uses three patches. Both overlays use GPIO0/1 for a dedicated PMIC I2C bus and GPIO2 for its interrupt, through the SoC's appropriate GPIO controller. Standard upstream AXP221/223 MFD, regulator, ADC, battery and AC drivers supply power telemetry. The older fork's extra PMIC writes and copied battery limits are deferred pending evidence about the cells and charging requirements. The independent `cm5-io-board` target uses no project patches; it selects the official CM5IO base DTB, stock Pi 5 KMS and GPIO UART overlays. Its optional
`dwc2` peripheral Overlay is shipped but commented out by default.

Build products include Kbuild's gzip-compressed ARM64 `Image.gz`, modules and DTBs/overlays, an ARM64 `uconsole-kernel` Debian package and a checksummed report. The Raspberry Pi backend stages the compressed bytes under each board's existing firmware kernel filename; this reduces SD reads while preserving the decompressed kernel. Boot files occupy `/boot/firmware/uconsole/`; module files use the kernel's own release directory. The package generates a matching initramfs when configured. Its initramfs post-update hook refreshes the firmware-prefix copy after every regeneration, including the final image export. This keeps the project kernel files distinct from distribution kernel packages.

CM4 pi-gen images begin with a managed initramfs include for partition resize,
PARTUUID and first-run hooks. A delayed, CM4-specific checker permits direct
MMC/ext4 root only after validating the normal boot layout and built-in root
dependencies. It retains the recovery initrd; kernel/initrd updates restore the
managed default under a shared lock. CM5 and other backends keep their own boot
policy. The checker runs after 60 seconds to keep hash reads off the desktop
startup path.

The report records source/configuration/package hashes. Image preparation rejects stale inputs or damaged artifact files. It does not prove hardware behavior or byte-for-byte reproducibility across compilers and package archives.

## Image path

The implemented rootfs backends are pi-gen and Ubuntu Base for the raspberry-pi platform. pi-gen desktop and lite profiles choose separate stage lists. The `ubuntu-noble-base` profile builds Ubuntu Base 24.04.5 ARM64 without a desktop and defaults to CM4; CM5 can select the same backend, with build and hardware validation recorded separately. Ubuntu Desktop, XFCE and KDE remain planned and fail availability checks. Rockchip and other platforms are represented by the model; concrete kernel/boot/export backends remain future work.

Preparation archives the exact pi-gen commit into ignored `image/work/pi-gen-<board>-<profile>-<desktop>`, copies the project stage, selected kernel package and boot config, and disables intermediate image exports. The default desktop profile runs stages 0–4 followed by `stage-uconsole`, producing a Trixie ARM64 desktop image. Images preconfigure the initial account and enable SSH. The default username is `pi`; a password must be supplied through ignored local configuration. Preparation validates the password requirement and disabled first-boot renaming before creating the work snapshot, allowing headless setup.

Raspberry Pi Desktop also consumes a validated, locally built ARM64 battery plugin package. A project-owned export step installs it after pi-gen's final distribution upgrade and verifies the installed version; lite images omit that step.

Production CM4 images request the existing display modules through an early
root-userspace module list, including the DT-only HDMI DDC dependency. This
policy is independent of initramfs eligibility; CM5 and generic carrier fixtures
retain their own module policy.

An optional CM4 native Mesa builder consumes a fixed offline source/dependency
recipe and emits a private compositor package with a checked receipt. Its
independent labwc session loads V3D/VC4 Mesa without LLVM while cleaning the
private library environment before launching clients or Xwayland. It does not
replace distribution Mesa or change the default image/session. Native build,
package validation, session selection and device deployment are separate steps.

A work-directory identity includes pi-gen's pin, effective configuration and custom-stage/export contents, including the kernel package, boot config and desktop battery package. This separates root filesystems when build inputs change. The source submodule is never used as a mutable pi-gen working directory.

pi-gen requires rootful privileged container mounts and loop devices. On an x86 host, QEMU/binfmt must execute ARM64 binaries inside an empty chroot; the container also sees the host's binfmt mount. The image stage uses the observed GPIO10 headphone detection and GPIO11 speaker-enable polarity through `pinctrl`. Keyboard and trackball remain in standard USB HID paths.

### Native Arch Base path

`scripts/image_backends/arch_base.py` owns the `arch-base` profile for CM5 IO
Board. Its settings and signed Generic AArch64 rootfs inputs live under
`image/arch-base/`. The provider prepares an isolated rootfs, performs a complete
signed pacman upgrade and installs the base CLI/network packages. Omarchy and
other desktop repositories are outside this profile.

The existing project kernel archive supplies Image/modules/DTBs as files;
Debian maintainer scripts are not executed. Arch's mkinitcpio builds the private
initramfs for the exact project release, and an ALPM hook refreshes it when
relevant packages change. The signed Raspberry Pi bootloader package supplies
firmware from `/boot`, copied into the FAT tree at `/boot/firmware`.
NetworkManager owns Ethernet/Wi-Fi and the optional USB static link; the shared
CM5IO gadget/DHCP/ADB payload remains disabled by default. Arch-specific account,
SSH host-key and first-boot root-growth setup uses Arch service/package names.
The build-only pacman keyring is removed before export; a first-boot service
initializes unique local keys using the package-owned Arch ARM public keyring.
The shared file-only Raspberry Pi exporter keeps build and flashing separate.
A pinned base archive plus a package inventory records this build's inputs;
the rolling package mirror is not a complete reproducible package snapshot.

### Optional Omarchy core layer

`image/packages/omarchy/build.py` repacks fixed, signature-verified upstream
Omarchy packages as script/QML/font data. It uses the host's archive tools,
without an ARM compiler, root, downloads or a running board. The private work
area and package receipt record the selected payload, input signatures and
current project tool/patch identities. `tools/omarchy_package.py` validates
that receipt before reuse. The output is locally unsigned; board-side signing
and installation are separate explicit operations with pacman's required
signature policy retained.

`image/desktops/omarchy/` separately generates a parameterized manual
UWSM/Hyprland session and checks its files before an explicit apply. The
controller uses PAM/logind, a normal user and tty7. The equivalent uConsole
virtual output is 1280×720, scale 1.5, transform 0; WayVNC listens on loopback.
Generating or applying files does not start or enable a session. The ordinary
`arch-base` profile remains `desktop=none`; the full Omarchy installer,
boot/firewall policy and application suite are outside this core layer.

### Native Ubuntu Base path

The optional `image/packages/mesa-noble` builder produces private V3D/VC4
EGL/GBM libraries against Noble's native ABI. It is independent of the Trixie
Mesa/session package and does not replace system libraries or select a desktop.
The compositor's existing library-path configuration can select it while labwc
clears that override before starting clients. Noctalia can select the private
libraries independently; a small preload helper clears its loader environment
after glibc caches the initial lookup path, keeping later application launches
on system libraries. GPU driver selection belongs to
the recipe; a future Rockchip recipe must select its own GPU drivers rather than
inherit the Raspberry Pi selection. Ordinary Ubuntu images do not require this
experimental package.

`scripts/image_backends/ubuntu_base.py` owns its recipe, TOML settings and rootfs
assembly independently of pi-gen. The ignored
`image/ubuntu-base/config.local.toml` supplies a required password and optional
account, Wi-Fi and APT overrides. Preparation validates settings and kernel
inputs before creating an isolated mode-0700 work directory. Generated build
configuration is mode 0600 and contains the password hash and any supplied
Wi-Fi credentials; pi-gen's `image/config.local` is not consumed.

For `cm5-io-board`, board-level `image_profiles` and `image_desktops` allow
headless `ubuntu-noble-base` and `arch-base`. Ubuntu settings come from its own payload directory,
`image/board/cm5-io-board/config.toml` and ignored `config.local.toml`, with the
requested public development account `pi` / `1`. uConsole local settings are
not read. Raw settings TOML is excluded from work snapshots; the effective
settings and password hash enter the private build specification. The target
omits uConsole patches, GPIO audio, display module policy, rotation and desktop
payloads. GPIO14/15 UART0 is enabled at 115200 8N1 with an explicit serial getty,
alongside the HDMI text console. Shared package and firmware directory names
remain unchanged. Build support and device validation are separate states.

The CM5IO payload also owns an opt-in ACM/NCM USB composite. A boot-time
`dwc2` peripheral Overlay exposes the UDC; a configfs service creates the
functions with hardware-derived serial/MAC identities. Netplan assigns
`cm5usb0` a fixed `<HOST_01>/24`, and a link-scoped dnsmasq serves DHCP without
router/DNS options. Serial login uses a normal getty. An absent-by-default
marker adds FunctionFS ADB; the daemon runs as the configured ordinary user
with no TCP listener and `NoNewPrivileges=yes`. Its Linux port has no Android
key authorization. The backend verifies the pinned upstream `.deb` and extracts
only the private AArch64 adbd and copyright, avoiding upstream services and
unrelated package dependencies. Payloads and recipe enter input hashes;
inspection checks installed bytes, modes and default-off markers. This is
CM5IO policy and does not alter uConsole's board payload. See the
[USB debug note](../knowledge/usb/cm5-io-board-usb-debug.md) for activation and
separate device evidence.

The base tarball is pinned by a SHA-256 from Canonical's signed checksum list.
Ubuntu's archive keyring verifies packages from Noble, updates and security.
A pinned Raspberry Pi utils source supplies `pinctrl`, built with Ubuntu GCC
inside the target chroot; temporary development dependencies are removed.
`linux-firmware-raspi` supplies boot and wireless firmware. Compressed firmware
is also materialized under uncompressed names for retained kernels without a
compressed firmware loader. Package licenses remain in the image, including
the pinned pinctrl license.

The default kernel input uses the current strict artifact validator. An
explicit historical report plus its SHA-256 can instead select retained
kernel/package/configuration bytes while accepting recorded patch/configuration
drift. Board identity, upstream pin, selected series and artifact checksums
still have to match that report's contract. This separate path does not relax
`load_kernel_artifact()`. After unpacking the package, the Ubuntu builder
installs the current matching initramfs post-update hook before configuration;
the receipt records that rootfs integration separately from the unchanged
kernel package. Ubuntu retains initramfs on every boot and does not install
pi-gen's CM4 direct-root policy.

Rootfs assembly uses `sudo -n unshare` with a private mount namespace and
ARM64 chroot execution; x86 hosts need registered QEMU/binfmt with the F flag.
The Ubuntu image path does not create a Podman container. The file exporter
uses native mkfs, mtools and sfdisk against temporary regular files, with a
512 MiB FAT32 partition at 1 MiB and ext4 at 513 MiB. It excludes FAT contents
from the root filesystem, checks both filesystems and atomically publishes a
mode-0600 image owned by the invoking user. Root size reserves 25% plus
512 MiB, at least 1 GiB, rounded up to 256 MiB. Export does not attach loop
or physical devices.

First boot generates SSH host keys and machine identity and expands the
second root partition/filesystem through a project-owned service. Exported
receipts record source/input hashes, kernel selection, the installed hook,
partition layout and image hash; a separate package list records actual
versions. Archive updates prevent a byte-for-byte reproducibility guarantee.
Configuration and receipt details are in
`image/ubuntu-base/README.md`（工程源码：`image/ubuntu-base/README.md`）.

Ubuntu keeps the distribution NetworkManager daemon/plugins and Netplan YAML
integration. A service-only PATH entry dispatches the exact plain `netplan
generate` invocation without importing unrelated Python subcommands, while
retaining native generation and udev/systemd reloads. Audited CLI source hashes
guard this path; changed sources, custom environments and other arguments
delegate to the original CLI. Both text payloads enter the normal input snapshot
and image inspection. This is rootfs policy, independent of board identity;
it does not require a custom daemon package or change session/network ordering.

### Shared labwc desktop payload

Ubuntu's optional `labwc-waybar` profile consumes a pinned native ARM64 package
under a private Wayland prefix and project-owned files under `image/desktops`.
The rootfs backend snapshots both inputs, installs distribution dependencies,
records installed versions and enables greetd for the preset ordinary user.
The uConsole desktop plan shares output transform 270 and scale 1.5 across shells;
Kanshi configuration and installation receipts persist both settings.
The inspector compares payload bytes, permissions, package identity and session
configuration. Both desktop shells explicitly install BlueZ and the PipeWire
Bluetooth SPA plugin; controller firmware and transport remain board-specific,
and pairing/audio validation is separate. Headless profiles retain their separate path.

The session supervisor owns its compositor and client process groups; application
processes do not inherit private library overrides. Noctalia has a separate native
package recipe. Its wrapper directly executes a private ELF with a fixed RUNPATH,
so `/proc/self/exe` retains the executable identity required by its PAM helper.
Its image path snapshots both native packages and verifies both installed payloads.
Native package installation explicitly includes each package's documentation
directory so Ubuntu Base's dpkg exclusions do not drop its bundled licenses;
the global exclusion policy is unchanged.
Both shells have completed native CM4 compilation and ordinary-user deployment.
Noctalia's component and session declarations are enabled after functional
roundtrips for lock/unlock, graphical authorization, file selection, clipboard,
application launch, audio and logout. Waybar remains the default. Normal image
preparation consumes checked prebuilt packages without compiling the desktop.
`tools/desktop_packages.py` is the shared input facade for profile availability,
Ubuntu desktop preparation and image inspection. Fixed packages live under
`prebuilt/ubuntu-noble/arm64/`: only `.deb` payloads use Git LFS; sibling JSON
receipts remain ordinary Git files. An absent or empty native cache is populated
atomically from the tracked pair, then checked with the existing package validator.
A nonempty native cache takes precedence, including stale or incomplete inputs;
validation failure never silently replaces it. Unexpanded LFS pointers report a
missing environment with a `git lfs pull` hint. The facade neither fetches inputs
nor builds packages, and preserves the existing receipt schemas and hashes.
This automatic import covers Wayland and Noctalia. The optional Mesa prebuilt is
validated through an explicit directory and is not an ordinary image input.
The user confirms Waybar landscape orientation, trackball input, the terminal
shortcut and scale 1.5. See [runtime status](status.md#ubuntu-labwc-desktop-checkpoint)
and the [desktop comparison](../knowledge/desktop/labwc-shell-comparison.md) for
device observations and remaining cold-boot and cross-board limits;
configuration tests establish the software contracts separately.

Prebuilt userspace package identity follows the target architecture, userspace ABI,
sources, patches, dependency versions, toolchain and build options; CM4/CM5 board
names do not identify generic application binaries. Compatible ARM64 systems may
reuse the same validated package and receipt. Kernel, device tree, power, graphics
drivers and boot firmware remain board/backend responsibilities, including future
non-Raspberry Pi modules with a compatible connector. The current native builder
does not implement cross compilation. See the [package reuse and adaptation
method](knowledge/build/userspace-package-reuse.md) for cache and validation limits.

## Deployment boundary

Image generation creates files only. No build or check command connects to the uConsole, reboots it, changes its boot configuration, or flashes eMMC. Any future deployment command requires separate authorization, exact device identification and an established recovery procedure.

Diagnostic kernel file staging is a separate, explicit offline operation through
[`tools/kernel_candidate.py`](kernel-candidate.md). It previews by default,
rejects the host root, preserves usrmerge and existing boot inputs, and accepts
only new release-specific modules and named candidate boot files. It does not
generate initramfs, select a boot configuration, connect to a device or flash
storage. The caller identifies and mounts the offline filesystem; local fixture
validation does not establish candidate bootability.
