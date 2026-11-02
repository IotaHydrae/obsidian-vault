# Development status

## Project state

Targets: ClockworkPi uConsole CM4/CM5 and the official CM5 IO Board development baseline (`cm5-io-board`). The board model also supports other vendors' compute modules/SBCs; concrete Rockchip kernel/boot backends remain future work. Linux is pinned to `234188315037848e5b07d65b85d38edc64095caa` on Raspberry Pi's `rpi-6.18.y`. Official pi-gen is pinned to `d346cd53562b3c6b4f94c9155175f814258c456d` on its `arm64` branch. The arm64 branch is required; master exports armhf.

Four focused kernel patches (three per uConsole board, none for CM5IO), bcm2711/bcm2712 config fragments, containerized kernel build/package tooling and a project-owned pi-gen desktop stage are implemented. The profile/frontend design separates rootfs providers, desktop components and shared board files. pi-gen desktop/lite, native Ubuntu Base 24.04.5 ARM64 and CM5IO Arch Linux ARM Base paths are implemented; Ubuntu Desktop/XFCE/KDE remain planned, unavailable components. See [image composition](image-design.md). Build/inspection results below distinguish implementation from verification.

## uConsole emulator and input research (2026-11-01)

**Research, not deployment:** recorded RetroArch and candidate cores, PPSSPP
and DOSBox Pure as optional software choices. Fixed official keyboard source
shows composite Keyboard/Joystick/Mouse/Consumer HID, keyboard-arrow direction
keys and switch-dependent ABXY/Start/Select reports. Input recommendations
therefore require per-key observation and backend-specific binding rather
than assuming a complete standard gamepad or a host-visible Fn key.

The [emulator note](../knowledge/desktop/uconsole-game-emulators.md) separates
physical binding, core remaps and hotkeys, and defines future input/display/
audio/save/performance checks. No emulator or game/BIOS was installed or run,
no controller was paired and no firmware changed. No model-specific frame
rate or power result is claimed; current image profiles remain unchanged.

**Commercial product model, not a release approval:** recorded separately the
candidate design of reviewed emulators/cores plus device controls and user
content import, with factory demos restricted to original or appropriately
authorized content. The personal recommendation list does not authorize
commercial preinstallation: Snes9x/FBNeo are excluded by default pending any
necessary separate rights, and permissive/GPL/MPL candidates still require
version/dependency/source-distribution review. ROMs, BIOS and other assets
have separate rights; purchase or free download is not treated as bulk
redistribution permission. Author ROM/demo sources are recorded as personal
evaluation candidates, not authorized factory payloads. No commercial content
agreement, product release, source offer or image integration was completed.
Tobu Tobu Girl Deluxe has an explicit public-license basis (MIT code and CC BY
4.0 graphics/text/audio), so it is a potential factory demo subject to exact
ROM identity and required attribution/notices. Other author downloads were not
assumed to grant the same rights. Selected source notices also refine GPL
candidates to their actual or-later grants, and DOSBox Pure's current source
has moved to Codeberg.
See the [distribution note](../knowledge/desktop/uconsole-emulator-distribution.md).

## T527 driver source and binary-delivery audit (2026-11-01)

**Source-backed findings:** the reviewed Avaota and WalnutPi BSPs contain
substantial NPU VIP/GALCORE and current VIN/ISP implementations with Kbuild
object-to-source mappings. Avaota's relevant NPU/VIN/VE/GPU Git subtrees match
between the independent BSP main and integrated A1 5.15 branch. Its VIP/GALCORE
and VIN object checks found corresponding source files; Walnut explicitly
selects both NPU module outputs and the T527 VIN/ISP600 path. This is evidence
against a blanket assertion that these kernel drivers are distributed only as
precompiled ko. No source rebuild or 6.18 acceptance is claimed.

**Binary boundaries:** four actual Walnut packages were downloaded and
statically listed/extracted, without installation or execution. The NPU
wrapper publishes source but links AArch64 libVIPlite/libVIPuser runtime
libraries; ISP packages provide libisp/libisp_ini/libAWIspApi and static 3A
algorithm/management archives. Their complete implementation sources were not
found in the reviewed wrapper/prepared-package scope. Existing source-backed
kernel code does not make these user libraries fully rebuildable or guarantee
their ioctl/mmap ABI on a new kernel. Mesa/FFmpeg/glibc precompiled files are
not classified closed merely because an ELF is supplied.

Legacy VFE/HDMI and other optional BSP paths also contain extensionless ar
archives and explicit link rules. These are genuine partial-source/binary
paths, but not the current selected T527 VIN or A1 DRM path. A Walnut codec
libve archive contains RISC-V ELF32 objects; its presence is not established as
a selected ARM64 T527 prerequisite. Firmware, ramfs utilities and user-space
codecs are recorded separately from kernel drivers.

**Verification and limits:** public tree inventories were checked without
truncation for the inspected scopes, source/package hashes and static ELF/ar
identities were recorded, and package bytes match fixed Git-hosted index
checksums. No APT Release signature was verified, and none of these packages
was adopted as a new trusted build input. The audit does not prove all vendor
components have public source or reviewed reuse licenses. No module, driver,
image or hardware operation was performed. See the
[driver source audit](../reference/t527-driver-audit/README.md) and
[availability note](../knowledge/kernel/t527-driver-source-availability.md).

## Avaota BSP manuals and T527 6.18 feasibility (2026-11-01)

**Collected methods:** the official Linux BSP category is now indexed as an
introduction plus 17 subsystem pages, with original HTML hashes and lengths.
Two focused notes distill controller identity/DT/dependency checks, observation
versus active transfers, explicit data/physical oracles and version-specific
limits. Several source examples contain malformed DTS/C, inconsistent paths or
failed-test output, so they are not copied as executable project tests. GPIO,
I2C/SPI/UART, PWM/PCIe, clocks, crypto, Ethernet, cross-core locks, IR, LEDC,
CSI, RTC, SID and timers are covered; unrelated RTOS/HAL pages are excluded.
See the [manual index](../reference/avaota-a1/bsp-manual-index.md).

**Verified upstream source facts:** official Linux v6.18 and stable v6.18.60
already include T527 Avaota-A1 and Orange Pi 4A board DTs using the A523-family
common description. A1 enables UART, SD/eMMC, GMAC0, USB and GPU/PMIC nodes.
Those sources do not contain a complete A1 DE/TCON/HDMI/DSI display pipeline;
GPU rendering and physical display support are different outcomes. No CM2
specific upstream board DT was found in the inspected directory.

**Feasibility conclusion, not runtime validation:** 6.18 is a useful new T527
baseline. Prefer evaluating upstream support and target-board additions before
forward-porting every 5.15 BSP driver. Vendor BSP separation/conditional build
helpers do not establish a completed 6.18 target or cross-version module ABI.
A first-person Orange Pi 4A community project reports a 6.18.55 plus 143-patch
system with display/GPU and other functions; this project did not reproduce it,
and that reference does not claim working DSI or suspend/resume. Its board DT,
DDR/boot data and OPPs cannot be directly applied to CM2.

The [migration assessment](../knowledge/kernel/t527-linux-6-18-migration.md)
records source identities, known gaps and the bring-up order. The Roadmap now
includes this candidate path. This round performs no driver port, patch
application, kernel/image build, device command or flash operation.

## Avaota-A1 T527 reference collection (2026-11-01)

**Source facts:** added the official Avaota-A1 hardware and AvaotaOS build
repositories as a second T527 reference. The collection fixes hardware
`bbd81c54c9a971e8fd9397fec958a9d9afebdffe` and AvaotaOS
`bd95d7528c232cee59e4047994be520cb7551888`, preserving selected unmodified
files in an ignored cache with URL, byte length and SHA-256 catalogs. It
includes v1.7's 13-page full-board schematic, editable EDA/PCB, converted
Altium schematic/PCB, Gerber, BOM, placement data, STEP, v1.6 changes, the
used eight-layer LPDDR4 template and IBIS. The four-layer DRAM template is
explicitly documented as unused by A1; it is not an equally validated design.

**Software findings:** the selected A1 kernel is the vendor `linux-5.15`
branch, observed Makefile version 5.15.154, with AW_DRM/Panfrost and AW_DISP2
disabled. Its DSI host implements standard MIPI-DSI and DRM panel lifecycle
operations, providing a source-backed candidate for future CWU50 adaptation.
AvaotaOS selects a pinned A1 SyterKit revision; Noble GNOME's board hook still
chooses Xorg because of its documented GBM front-buffer issue. Source-level
interfaces and configuration do not establish working Wayland or Omarchy.

**Reuse scope and limits:** A1 is a full SBC design, not CM2's unpublished
internal schematic or a CM4-format module. Its PMIC/DDR/clock/IO/layout can
inform a future core design, while CM2's internal choices remain separately
unknown. Boot binaries, DDR parameters, DTB, power and image layout cannot be
substituted on SoC identity alone. A1 is referenced from the CM2 Roadmap, not
added as an available board. No kernel, image or device operation was performed.
See [A1 reference catalog](../reference/avaota-a1/README.md), the
[focused reuse note](../knowledge/hardware/avaota-a1-t527-reference.md) and the
[combined hardware reference index](../reference/README.md).

## Planned CM3 validation and WalnutPi CM2 research (2026-11-01)

**User intent and project state:** CM3 support is now in the
[Roadmap](roadmap.md), scoped to a low-spec second-hand module for validating
this project's board/rootfs/boot integration. The actual CM3/CM3+, Lite/eMMC
and carrier revision remain to be identified. Work is planned, not an enabled
board. Current pinned Raspberry Pi sources contain the ARM64 CM3 DT target;
legacy boot firmware selection and a dedicated uConsole overlay still need
implementation, along with display, USB Host/HID, PMIC and real shutdown checks.

The user has also purchased a WalnutPi CM2 based on Allwinner T527 and supplied
the official `walnutpi-2` hardware/example repository and `walnutpi-build`
image builder. A source catalog now fixes the reviewed repositories, selected
files and official Wiki snapshots by SHA-256 and byte length. CM2's published
module pinout/mechanics/3D and IO carrier schematic are recorded separately;
a complete core-module SoC/DDR/PMIC schematic was not found in those sources.
CM2 and WalnutPi 2B are distinct physical boards despite sharing the repository.

**Source facts and derived scope:** the reviewed build configuration uses the
`walnutpi-2b` T527 path, SyterKit and a vendor Linux 5.15.147 branch. The
investigation does not establish complete CM2 image support from that directory
name alone. Allwinner needs its own boot/kernel/export contracts and hardware
adaptation; Raspberry Pi `config.txt`, RP1 GPIO and VC4/V3D do not establish
compatibility. Selected DSI and USB signal positions are promising interface
intersections, not electrical or functional acceptance of a CM4 adapter.
See [CM2 references](../reference/walnutpi-cm2/README.md) and the
[focused platform note](../knowledge/hardware/walnutpi-cm2-platform.md).

**Validation boundary:** this round changes planning and reference documents.
It adds no available board or menu item, executes no vendor build, and neither
builds nor flashes a new image. No CM3/CM2 runtime result is claimed; module
power, recovery, full pin compatibility and uConsole display/keyboard remain
separate validation work.

## Official hardware reference library (2026-11-01)

**Documented source facts:** the repository now indexes current official CM4 /
CM5 datasheets, both 200-pin BTB tables, module mechanical STEP files and IO
Board KiCad projects. The current pinned module documents are CM4
`RP-008168-DS-4` (internal Release 4, 2026-06-30) and CM5
`RP-008180-DS-7` (internal Release 3, 2026-06-08). Original downloads remain
unmodified in an ignored local cache; tracked catalogs record fixed URLs,
SHA-256, byte lengths and repository-relative cache paths. Independent CSVs
store pin facts, with explicit PDF/printed page numbering and a name-only
comparison that does not claim electrical equivalence.

ClockworkPi uConsole sources are pinned to
`53a05efca806ca024aab4fbfd28a680688c2541c`: mainboard V3.14/V5, CM4 adapter,
A06/R01 core schematics, keyboard, 4G, display-controller and assembly materials,
plus mainboard Gerber and adapter manufacturing archives. The 200-pin J1 core
edge connector and custom 52-pin J4 expansion connector have separate fact
tables. ClockworkPi's official product page lists CM3 compatibility, and a
first-person community port demonstrates direct CM3+ insertion without the CM4
adapter. Clockwork's use of CM3 NC positions for extra functions does not itself
disprove compatibility. The community also reports internal 5V shutdown issues
on a CM3L+ assembly; no universal plug-and-play or project CM3 runtime validation
is claimed. J4 does not carry standard mini PCIe signalling. The official page,
pinned community README and original forum report are catalogued separately.

**Derived design scope:** an alternative core retaining the internal display
and keyboard needs compatible power/ground/IO references, DSI with panel
reset/backlight control and a USB Host link to the motherboard hub. At least
one boot storage path is required; the motherboard SD slot is conditional on
the storage design. HDMI is optional as explicitly requested. PMIC management,
wireless, audio and expansion roles are qualified separately in the
[minimum-interface note](../knowledge/hardware/uconsole-custom-core-interface.md).

**Limits:** this work does not provide a full Raspberry Pi SoM CPU/RAM/PMIC
PCB clone design, nor has a new module been fabricated, powered or electrically
validated. Mainboard mechanical dimensions have not been reconstructed from
CAM. No board settings, running desktop, image or storage were changed.
See [Raspberry Pi references](../reference/raspberry-pi-cm/README.md) and
[ClockworkPi references](../reference/clockworkpi-uconsole/README.md).

## Reusable Omarchy core package (2026-10-30)

**Implementation and offline evidence:** a project-owned, offline data-repack
recipe now builds `omarchy-cm5-test` 4.0.6-2 from the two fixed, signed upstream
4.0.6 packages. It carries the reviewed version-name fallback and a local
provenance/manifest receipt, checksum and license. The fixed settings input's
2950-byte `.INSTALL` is accepted only under its explicit discard hash after
input verification; it is never executed and is absent from the output.
Generic and output archive readers reject installation hooks, ELF, unsafe
paths/links, conflicting files and special file types. Output validation reads
actual tar permissions/ownership before enforcing root/normalized metadata.

Two independent builds produced byte-identical package and receipt. Output is
120,572,154 bytes, SHA-256
`9e38a5f2bd04dbb6f50180b5557bb5db47a3ffa3ba2576bc880909f3ffc46403`.
The current loader accepted all 1,865 archive entries; the 1,675 source payload
entries match the previously device-tested prototype by path, type, content
hash/link target and normalized mode. Eighteen package tests pass, including
real signature rejection, raw archive metadata, stale receipts, work-directory
protection and the version helper's four query-priority outcomes.

**Scope and unknowns:** output remains locally unsigned. Deployment separately
checks the trusted digest after transfer and signs with the board's local
pacman master key while retaining Required signature policy. Some installer,
update and migration helpers remain inert data; this is not full Omarchy
provisioning. This reusable package was not installed on the current board.
The build did not generate an OS image, flash storage or reboot a device.
See the package recipe and usage（工程源码：`image/packages/omarchy/README.md`）.

## Reusable manual Omarchy session (2026-10-30)

**Implementation and offline evidence:** a separate parameterized generator
produces eight native UWSM/Hyprland session files and a template-bound manifest.
The apply tool previews by default and requires an explicit target root and
`--apply` for writes; live `/` also requires `--live-board` on the CM5. It
checks runtime dependencies, user/UID/home/board and separately seeded user
settings/generated theme before installation. Conflicting files or symlinks
fail preflight. File application does not invoke external commands or start,
enable or reload services. Thirteen session tests pass, covering read-only
preview, staged-file integrity, conflict/symlink rejection, idempotence and
exact viewport verification before application startup. Combined `make check`
passes 555 tests, pinned patch application and the knowledge audit.

**Device observations and limits:** read-only queries confirm the existing
prototype's native session, shell and loopback WayVNC are still active. The
active output remains 1280×720, scale 1.5, transform 0. New generated session
files were not applied to the board; the earlier prototype's runtime evidence
is distinct from validation of these reusable tools. Ordinary Arch Base stays
`desktop=none`; full desktop image integration and the known warm-reboot USB
failure remain unresolved.
See the manual-session instructions（工程源码：`image/desktops/omarchy/README.md`）.

## CM5IO native Omarchy core desktop (2026-10-30)

**Result:** the native Arch Linux ARM installation now runs an ordinary-user
Omarchy core desktop through UWSM, Hyprland and Quickshell. This is a native
PAM/logind tty7 session on seat0, with a DRM allocator and a compositor-owned
headless output. It is a manual test layer; the boot default remains
`multi-user.target`. No image was built, no eMMC was flashed, and no board reboot
was requested during this round.

**Inputs and installation:** the user explicitly supplied Wi-Fi access; its
credential remains only in the board's mode-0600 NetworkManager profile. The
board clock was synchronized before repository/signature operations. A complete
signed Arch ARM/Omarchy dependency transaction installed Hyprland `0.56.2-7`,
Quickshell `0.3.2.r7.g2b4501a-1`, Aquamarine `0.15.1-1`, UWSM `0.27.1-1`,
Mesa `26.2.6` and WayVNC `0.10.2-1`. A Quickshell TLS early-EOF download was
recovered by verified 256 KiB Range fetches; neither TLS nor package signature
checks were disabled.

The registered local `omarchy-cm5-test 4.0.6-2` package contains the upstream
4.0.6 desktop scripts/QML/themes/fonts and selected user defaults from the
signature-verified ARM packages. Its SHA-256 is
`63daa4812e07d49184babc4483f3337ad699e854c947c098156bde447a83cf09`.
It has normalized file modes and an explicit `inotify-tools` dependency. The
only upstream source patch adds this local package name as the final query
fallback in `omarchy-version`; the original application launcher remains
unchanged. Original package signatures use the pinned Omarchy fingerprint;
the curated package was signed by the board's unique local pacman key and
installed without relaxing required local-file verification. The full installer,
root settings/boot hooks, disk and firewall provisioning were excluded.

**Device/UI observations:** the output was checked as `CM5-NATIVE`,
1280x720, scale 1.5, transform 0. Browser pointer interactions opened the Omarchy
menu and Apps list, then launched both Nautilus and an additional Foot terminal.
The latter accepted real individual key events and printed the expected marker.
The original `uwsm-app -- gtk-launch` path was used, rather than the Gio patch
from the earlier container trial. Tokyo Night user state was generated with
post-theme provisioning hooks disabled; GTK and Foot use the user dark theme.
The welcome helper terminal was stopped after the menu-launched terminal passed;
the actual desktop, shell and localhost-only WayVNC remain running.

Quickshell logged Broadcom V3D 7.1.10.2 / OpenGL 3.1. Deduplicated stable V3D
clients completed positive bin/render jobs in both processes during real UI work:
the first sample increased by 949 each in Hyprland and 56 each in Quickshell;
the final themed sample increased by 779 each and 48 each respectively. These
are functional hardware-rendering observations, not a performance acceptance.
USB SSH, gadget/DHCP and the default development boot configuration remained
unchanged and healthy. The final light-load sample was 40.0 C / throttled 0x0.

**Retained state and limits:** start/stop the manual session with
`systemctl start/stop cm5io-omarchy-native.service`. The host browser viewer was
retained with a reconnecting, task-specific SSH tunnel. Sources, package and
session manifests, UI/GPU observations, screenshots and logs are under ignored
`image/work/cm5io-omarchy-native-20261030/`. Baseline `make test` passed 524 tests;
knowledge and diff checks passed. Full Omarchy system provisioning, the wider
application/AI bundle, SDDM/autologin, audio, lockscreen, recovery/update behavior
and long-run stability are outside this result. The separately documented
software-reboot USB enumeration issue was not retested or declared fixed.
See the [native desktop note](../knowledge/desktop/cm5-omarchy-native.md).

## CM5IO Arch Base image (2026-10-29)

**Implementation:** `PROFILE=arch-base BOARD=cm5-io-board DESKTOP=none` selects
an independent Arch Linux ARM rolling/aarch64 rootfs backend. The interactive
menu offers it beside Ubuntu Base; Ubuntu remains the CM5IO default. Arch uses
its own settings, signed pacman repositories and mkinitcpio, with the stock CM5
board payload and matching project kernel. It includes the requested development
account, SSH, Ethernet DHCP/NetworkManager, GPIO UART login, root expansion,
`vcgencmd`, `pinctrl`, `net-tools`, Bash completion and rsync. Optional USB
ACM/NCM/DHCP/ADB files are present with the boot overlay and ADB marker disabled.
Omarchy is outside this base image.

**Build observations:** the actual kernel artifact is
`kernel/build/artifacts/cm5-io-board/5a3ea76a2af78538`, release
`6.18.57-v8-16k-uconsole`. The pristine 2026-08-05 Generic Arch ARM archive was
verified by SHA-256 and detached upstream signature with the full official
build-key fingerprint. A full signed pacman update completed on 2026-10-29.
The build removes the generic kernel and unrelated firmware, preserves usrmerge
and the Arch CA bundle link, masks inherited networkd/resolved ownership, and
exports no build GPG keyring. A first-boot service initializes a unique local
pacman key from the signed package-owned public keyrings.

Final artifact: `image/deploy/cm5-io-board-arch-base-none-arm64.img`,
2,685,403,136 bytes (about 2.50 GiB), 189 installed packages.
SHA-256: `89c93a4a3208dd3bec5420c96b5dc237103ab78d7b0a4f3025641a726ecf11d7`.
The adjacent receipt, package inventory and checksum were exported together.
`make check` passed 522 tests and the knowledge audit passed 56 documents / 233
local links with no errors. The strict kernel-artifact command and actual
read-only `image-check` both passed.

**Export verification:** a separate read-only loop mount of this final image
verified the requested password, the preserved `/lib -> usr/lib` and CA links,
an empty 0700 pacman keyring, interactive Bash completion, parsed pacman policy,
ADB dynamic library resolution, and listed 491 initramfs entries including `init`.
The image hash and builder/exporter receipt hashes match current sources.
Separately, the AArch64 first-boot keyring command sequence completed under QEMU
starting from an empty keyring. The complete logs and JSON observations remain
under ignored `image/work/arch-base-20261029/`.

**Deployment observation (2026-10-29):** after the user explicitly connected the
nRPIBOOT jumper and requested flashing, the official usbboot loader at commit
`465740b6bc526991cc96b1bda86a9469091b0af2` exposed the same previously identified
31,268,536,320-byte CM5 eMMC. Device identity, capacity, USB transport, disk
sequence, mount state and separation from the host NVMe root were checked before
an exclusive write. The exact 2,685,403,136-byte Arch image was written,
synchronized, its block cache invalidated, and the complete image range read back.
Its SHA-256 matches the source and build receipt above. Write/sync/readback took
130.05 seconds. The eMMC partitions remain unmounted; the loader is still running
in RAM with the user's jumper installed. The previous boot configuration,
including the 2.8 GHz and fan curve settings, was preserved privately; the new
image retains its default development settings.

The guarded procedure, current device identity, loader hashes, complete log and
successful receipt are retained under ignored
`image/work/arch-base-flash-20261029/`. No claim is made about eMMC bytes beyond
the image range or about native boot.

**USB-only deployment recovery (2026-10-29):** the user had already normally
powered on the board and then clarified that no Ethernet cable was available.
The deployment had incorrectly retained the image's default-off USB Overlay,
leaving no active host access path. After the user reentered nRPIBOOT mode,
only the boot partition's `config.txt` was changed to enable the existing
`dwc2,dr_mode=peripheral` Overlay. Its hash changed from
`47d9924d5f9e42244615aad8d1ba4295ea42b12f0a5c2d8074b36a189248cae5` to
`7d512065322171c9151e0751c640ecae0e27cae20488b66447934c2211224a2b`;
after unmount/cache flush/read-only remount the bytes matched the prepared change.
The rootfs was inspected read-only and preserved. It now has a 28.6 GiB root
partition, root-expanded and pacman-keyring-ready markers, generated SSH host
keys, an enabled gadget unit and the native USB NetworkManager keyfile. The new
SSH public key was captured directly from this identified eMMC for strict
post-reboot host-key verification. The base image hash above remains historical
flash evidence rather than a hash of the now-booted/modified device.
Logs and the separate deployment-config receipt are under ignored
`image/work/arch-base-usb-recovery-20261029/`.

**Arch USB runtime observations (2026-10-29):** after the user removed the jumper
and normally powered on, the expected ACM and NCM functions enumerated.
Password ACM login as the project user succeeded and reported Arch Linux ARM
with kernel `6.18.57-v8-16k-uconsole`. The NCM host driver and interface existed,
but the board's interface was down: Arch's vendor `85-nm-unmanaged.rules` set
`NM_UNMANAGED=1` for `DEVTYPE=gadget`, and NetworkManager reported reason 77.
A narrow `/etc/udev/rules.d/99-cm5io-usb-network.rules` override for only
`cm5usb0` set `NM_UNMANAGED=0`. After rule reload/trigger and activation of the
existing profile, the board acquired `<HOST_01>/24`, the host obtained an address
by DHCP, and strictly pinned password SSH succeeded. A 2 MiB transfer each way
had identical source, board and returned SHA-256; the host Wi-Fi default route
and DNS/gateway policy remained unchanged. The source fix and inspector are
updated; `make check` passed 524 tests. No image was rebuilt, and the saved
`89c93...26ecf11d7` image remains the historical base lacking this later rule.

**Current cold-start verification (2026-10-29):** after the user cold power-cycled
with nRPIBOOT removed, USB ACM/NCM returned, the host automatically obtained its
USB address, and strictly pinned SSH again succeeded as the normal user.
The persistent udev override remained active without a manual network command.
The native root mount is `/dev/mmcblk0p2`, ext4, about 28.1 GiB; expansion and
keyring-ready markers remain present. Core gadget/DHCP/NetworkManager/sshd
services are active. Subsequent boots correctly skip the completed one-shot
expansion/keyring units, which report inactive with false conditions rather
than requiring them to remain active. ACM password login again succeeded and
identified the controlling terminal as `ttyGS0`. The computer's old SSH key for
the fixed USB address was replaced with the new public key read directly from
the identified eMMC, and the ordinary user-facing SSH command was verified.
The sampled temperature was 41.1 C with `throttled=0x0`; this is a light-load
observation, not thermal or power-margin acceptance.

**Unresolved warm-restart observation:** the preceding software reboot test did
not return a CM5 USB enumeration or SSH within 120 seconds. Persistent journal
records now show that the OS actually rebooted and the gadget service completed:
DWC2 bound the gadget at about 7.016 seconds in that failed boot, but no subsequent
high-speed reset or USB address assignment was recorded. The successful cold boot
bound at about 7.097 seconds, followed by reset and address assignment. Shutdown
also completed without an observed gadget/getty stop timeout. Thus this occurrence
is not explained by the one-shot UDC condition or by NetworkManager's earlier
unmanaged rule. The controller/host connection and enumeration layer remains
unisolated; no kernel/firmware cause is asserted. The board is currently online
following the cold recovery, and no further restart was performed. Cold-start
USB success does not establish software-reboot reliability. Evidence remains
under ignored `image/work/arch-base-usb-validation-20261029/`.

**Unknowns and device boundary:** the observed partition growth and first-boot
markers establish those offline outcomes. USB enumeration, ACM login, USB DHCP
and SSH also passed after cold recovery; software-reboot USB recovery remains
unresolved. GPIO UART/other IO
remain separately unverified on Arch. Earlier Ubuntu boots and the isolated Arch
desktop trial do not establish these runtime results. A pinned base and package
inventory are provenance records; the rolling package mirror is not a complete
version snapshot.
See Arch Base instructions（工程源码：`image/arch-base/README.md`） and the
[focused integration note](../knowledge/build/arch-arm-base-image.md).

## CM5IO isolated Arch ARM / Omarchy trial (2026-10-29)

**Device observations:** after the user requested a simple Omarchy trial before
considering Arch Base images, prepared an independent Arch Linux ARM rootfs
under `/var/lib/machines/cm5io-omarchy-test`. The existing Ubuntu installation,
project kernel, boot files, USB access and Noctalia session remain. Installed
Hyprland 0.56.2-7, patched Quickshell 0.3.2.r7 and Mesa 26.2.6 from the matched
ARM repositories with package signature checks. Verified Omarchy 4.0.6 source
payload/signature and extracted desktop files without system provisioning.

The initial nested output failed with an xdg_surface configuration error at the
host labwc boundary. A separate Hyprland headless output now works through a
second loopback-only VNC/USB SSH route. It uses equivalent horizontal
1280x720/scale-1.5 output; rotated VNC pointer mapping remains a separate issue.
Real browser input produced expected terminal output; Omarchy's menu displayed
Apps, and launching Foot produced another tiled terminal with working input.
The container-only app launcher uses Gio in place of UWSM, whose complete
login/session service is not running in this trial.

Qt reported Broadcom V3D 7.1.10.2 / OpenGL 3.1 / Mesa 26.2.6. Deduplicated DRM
clients completed 221 additional bin/render jobs each in Hyprland and 9 each
in Quickshell during the sampled menu/input work. This is hardware-rendering
and functional evidence, not performance or stability acceptance. Kernel logs
also recorded transient undervoltage followed by normalization; final flags
were historical 0x50000 and temperature 51.6 C. Supply margin remains unresolved.

**Retained state and exact boundary:** the isolated trial and its second browser
viewer remain available alongside Noctalia. Units are manual/transient, not
boot-enabled, and startup still needs test-specific output/viewer preparation.
No image was generated, modified or flashed. This supports proceeding to an
Arch Base design; native boot, first-user/network/resize, full UWSM/polkit,
audio, lockscreen and Omarchy recovery/update behavior remain unverified.
The [focused trial note](../knowledge/desktop/cm5-omarchy-arch-trial.md) records
inputs, the two compatibility workarounds, source licensing and evidence in
ignored `image/work/cm5io-omarchy-20261029/`.

## CM5IO Noctalia trial (2026-10-29)

**Device observations:** at the user's request, installed the verified
`uconsole-noctalia` 5.3.0+uconsole1 package and runtime dependencies on the live
CM5IO. The existing labwc/VNC session now uses Noctalia instead of Waybar/Mako,
with the same 720x1280 / transform 270 / scale 1.5 output. Scoped configuration
omits DSI brightness/battery controls and disables automatic lock/DPMS for this
trial. Original test configs were backed up; manual-session autostart and
launcher shortcuts select Noctalia. It is not enabled at boot.

The browser showed the first-run screen, launcher and control center. Selecting
Foot launched a terminal in which actual VNC key events produced the expected
command output. Noctalia reported Broadcom V3D 7.1.10.2 / OpenGL ES 3.1; its
own DRM client completed 247 additional bin/render jobs each during menu work.
This is rendering evidence, not an FPS result. PipeWire and WirePlumber connect
through the ordinary user runtime; the default audio sink is auto_null, so
hardware/audible output was not verified. Package verification and dpkg audit
were clean. Evidence is in ignored `image/work/cm5io-noctalia-20261029/`.

**Known limits:** the manual system-service test lacks a logind graphical login;
Noctalia's graphical polkit agent registration fails with NoSessionForPID.
Graphical authentication and locking remain unresolved. Its Monitor UI reports
176% while the actual compositor output remains 150%; this display difference
is recorded without assuming its cause. Other desktop functions, physical IO
and long-duration behavior remain separate tests. See the
[headless desktop note](../knowledge/desktop/cm5-headless-desktop.md).
No image was generated, modified or flashed; build-profile defaults remain
headless. Repository baseline passed 477 tests before device work.

## CM5IO manual headless desktop test (2026-10-29)

**Device observations:** the connected 4GB CM5 / official IO Board had no HDMI
monitor; both HDMI connectors reported disconnected. The existing Ubuntu Base
installation retained its kernel, boot configuration and USB debug access.
Installed the checked ARM64 labwc/wlroots 0.20.2 + Waybar 0.15.0 package and
runtime/test clients, then started an ordinary-user headless session with a
1280x720 normal/scale-1 output. The session does not use uConsole's display,
battery or backlight policy. GL reported Broadcom `V3D 7.1.10.2`; the same DRM
client's bin/render jobs increased from 123 to 212 during GUI work.

A loopback-only WayVNC server was reached over USB SSH forwarding and a local
browser. Real keyboard input produced the expected terminal output, dragging
moved the file-manager window, and Mousepad saved the exact test text later
read independently over SSH. A matching grim capture contains the tested
windows at 1280x720. The user then requested uConsole dimensions: the same
output is now 720x1280 with transform 270 and scale 1.5, matching the existing
Ubuntu desktop plan (about 853x480 logical area). The live change and autostart
were verified; the compact icon taskbar and maximized editor fit this layout. dpkg audit was empty; final temperature was 37.3 C and
protection flags zero, without a stress/stability claim.

**GPU confirmation:** real Wayland EGL contexts reported Broadcom V3D for both
OpenGL 3.1 and OpenGL ES 3.1. During a 3-second ordinary-user
`es2gears_wayland` run, its V3D client completed 184 bin/render jobs each and
the same labwc client completed 368 each, deduplicating repeated client IDs.
A matching screenshot shows the gears. The test application was stopped;
no failed units or protection flags were present afterward. This confirms
hardware rendering for the compositor and tested GLES application, with no
FPS, video decode or all-application claim. See `gpu-confirm-final.json` and
`gpu-proof.png` in the headless work directory.

**Retained state:** `cm5io-headless.service` remains running for user access.
 It is static/manual, not enabled at boot; the system default
is still multi-user.target. The browser and host tunnel/proxy remain available
for this connection. Evidence is under ignored
`image/work/cm5io-headless-20261029/`; the [headless desktop note](../knowledge/desktop/cm5-headless-desktop.md)
records the mechanism, commands and exact limits.

**Boundary:** no image was generated, changed or flashed. CM5IO image profiles
remain headless; this test does not establish a full production desktop,
Noctalia, audio, clipboard, portals, lockscreen or physical display behavior.
The repository baseline passed 477 tests before device work; subsequent edits
are this observation, the focused knowledge note and its index only.

## Ubuntu Base rsync default (2026-10-28)

**Implementation:** the common Ubuntu Base package list includes `rsync`;
image inspection now requires its executable alongside the existing network
tools. The missing/non-executable regression is covered by the common fixture.
The user reports installing rsync on the live board; no board operation was
performed for this addition.

**Verification and boundary:** `make check` passed 477 tests and the
53-document/218-link knowledge audit. No image was regenerated or modified for
this change, per the user's instruction to defer image generation until
explicitly requested. The next requested build will install the new default;
the existing exported image still represents the Bash-completion checkpoint
below. A regenerated artifact and its rsync behavior remain unverified.

## Ubuntu Base Bash completion (2026-10-28)

**Implementation:** the common Ubuntu Base package list now explicitly installs
`bash-completion`. Ubuntu's existing skeleton and account `.bashrc` already
load its framework, so no custom shell startup file is added. Image inspection
requires nonempty, world-readable framework and profile startup scripts; the
regression rejects missing, empty and root-only copies.

**Device observations:** the live CM5IO was missing the package. Installing
Ubuntu Noble `bash-completion` 1:2.11-8 restored programmable completion for a new
ordinary-user Bash login. A real SSH PTY/Tab test completed `systemct` to
`systemctl` and `apt-get inst` to `apt-get install`; no reboot was needed.
Basic command/path completion is Bash/Readline behavior independently of this
package; complete loss of Tab should also prompt shell/terminal inspection.

**Artifact verification:** the retained offline CM5IO rootfs was copied with
metadata preserved, extended with the checked package and re-exported. Its
ordinary-user login shell loaded the completion framework and suggested
`install` for `apt-get inst`. The prior image and helpers remain archived under
the ignored USB work directory, and the sidecar `offline_updates` records the
package/source/finalizer and base-image hashes. The updated image SHA-256 is
`b4c9ed4d17740feaa9f3ee35d7df6033fa5ae47b7f7b9aae4e2f5876a9699e9e`.
`make check` passed 477 tests plus the 53-document/218-link knowledge audit,
and the updated image passed read-only inspection. This does not establish
a fresh-image boot; no image was flashed for this fix.

## CM5 IO Board optional USB debug composite (2026-10-28)

**Implementation:** CM5IO has an opt-in boot-time
`dwc2` peripheral Overlay and project-owned ACM login / NCM Ethernet configfs
payload. The generated-image default leaves the Overlay commented out.
The final board NIC is `cm5usb0`, fixed at `<HOST_01>/24`; link-scoped dnsmasq
serves DHCP without router or DNS options. A separate absent-by-default marker
adds FunctionFS ADB. The pinned RoboParty v1.0.7 Linux adbd runs as the configured
ordinary image user with `NoNewPrivileges=yes` and USB transport only; its
Linux port omits Android key authorization. Only its checked ARM64 binary and
copyright are extracted, avoiding upstream package services and unrelated
scrcpy/SMB dependencies. See the [USB note](../knowledge/usb/cm5-io-board-usb-debug.md).

**Device observations:** the user reconnected the official CM5 IO Board to the
Linux computer using its only USB-C port, J11, which carries both power and USB2
data. The board still uses Ubuntu Base 24.04.5 and
`6.18.57-v8-16k-uconsole`; its selected 2.8 GHz configuration and device-specific
fan curve were preserved. A boot-time Overlay and prototype composite created
ACM/NCM, with ordinary password serial login and a simultaneous 2 MiB USB SSH
transfer matching SHA-256. In the requested board-DHCP mode, the computer
received an automatic USB subnet address with no DHCP gateway or DNS and kept
its existing default route. Adding FunctionFS ADB and normally rebooting
produced a `pi` shell and a matching 2 MiB push/pull SHA-256 round trip.
The final project payload was then deployed and normally rebooted: ACM password
login, 2 MiB SSH over the USB fixed address and 2 MiB ADB push/pull all completed
concurrently with matching content hashes. ADB remained ordinary `pi` with
`NoNewPrivs=1`; the final NIC was `cm5usb0`. The joint receipt is
`final-concurrent-test.json` in the USB work directory. After the user configured
Wi-Fi and unplugged Ethernet, the cable carrier was down, the external route
used `wlan0`, DNS resolved and a certificate-verified HTTPS HEAD returned 200.
USB SSH to the fixed address remained usable. These observations apply to the
user-configured live installation, not a fresh image
(`wifi-network-test.json`, `wifi-usb-ssh-test.log`).

A scoped host udev rule prevents ModemManager AT probing of this ACM and grants
ordinary-user ADB access. Fresh enumeration was needed after an earlier ADB
permission failure. Dynamic Overlay setup had host-to-board RX failures and
runtime rebind showed DWC2 stop-transfer timeouts; their root cause was not
isolated. Boot-time initialization is the working path. Early boot on the
computer's USB supply briefly set historical undervoltage/throttle bits, then
current low bits cleared; no new stress or supply-margin claim follows.
Raw test receipts are under ignored `image/work/cm5io-usb-20261028/`.

**Default-off device checks:** after removing only the ADB marker and
rebooting, ACM/NCM and board DHCP remained active while adbd and FunctionFS ADB
were absent. With the boot Overlay commented out, a reboot exposed no UDC or
gadget, left all three services inactive and FunctionFS unmounted. The final
board was then restored to ACM/NCM/ADB enabled at the user's request. The
checks use the deployed project payload, not a fresh-image boot.

**Offline verification:** `make check` passed 476 tests and the indexed
53-document/218-link knowledge audit. The rebuilt matching CM5IO kernel
artifact validated successfully. The native Ubuntu Base build completed, then
the user-requested `net-tools` 2.10-0.1ubuntu4.4 ARM64 package was added offline
to a metadata-preserving rootfs copy and exported with the same file exporter.
The package SHA-256 came from the APT metadata already verified by the build;
`ifconfig`, `route` and `netstat` ran successfully under the target chroot.
The sidecar `offline_customization` records this late package addition, the
base image hash and finalizer hash; the original builder source and image
remain archived in the private USB work directory. Future ordinary builds
install `net-tools` directly from the default package list.

The final 1,611,661,312-byte image passed read-only kernel/module/DT/USB/account/
service inspection with SHA-256
`2675a59c0c8739c30663b169afb83f60068f8fcc8e272c612293c808f1cebf17`.
It includes the optional USB payload with the boot Overlay commented out and
ADB marker absent; Avahi is not preinstalled.

**Board-only mDNS test:** at the user's request, Avahi 0.8-13ubuntu6.2 was
installed and enabled on the live board. The Linux host resolved
`cm5-io-board.local` to the USB fixed address and password SSH through that
name completed as `pi` over NCM (`avahi-ssh-test.json`). The first package
fetch had a transient DNS resolution failure; resolution and the retry
succeeded without changing the user's Wi-Fi settings. The user explicitly
kept this as a live-board test, separate from image defaults.

**Unknowns and exact boundary:**
Windows/macOS operation, long-duration stability and fresh-image cold boot are
not established. The temporary overclock watchdog cleanup below remains pending;
USB functional tests do not complete overclock stability verification. No image
was flashed during this USB work and the host remains awake.

## Build scripts directory (2026-10-28)

At the user's request, kernel/image build frontends, their backend packages and
`image_container.sh` moved from `tools/` to `scripts/`. Root `build.sh` and
Makefile targets retain their commands. Shared configuration, artifact checks,
image inspection and diagnostics stay in `tools/`; machine interfaces remain
behind `Makefile.agent`. Makefile/container commands, source hash paths, test
imports and documentation links were updated without compatibility wrappers.
Direct CLI help also works from outside the checkout. The full 476-test check
passed after relocation; current-image preparation and read-only inspection
exercise the relocated scripts separately from the earlier native image build.
See the script map（工程源码：`scripts/README.md`）.

## Raspberry Pi tools in Ubuntu Base images (2026-10-28)

**Implementation:** new Ubuntu Base builds for the supported Pi boards install
Ubuntu Noble's `libraspberrypi-bin` for `/usr/bin/vcgencmd`. The existing pinned
Raspberry Pi utils source still supplies `/usr/local/bin/pinctrl`; no additional
pinctrl package or Raspberry Pi OS APT source is introduced. `raspi-config` is
not installed. The common Ubuntu image inspector now requires both executable
files without running target hardware commands on the host.

**Offline verification:** the full `make check` passed 459 tests, including
missing/non-executable tool rejection, kernel patch application and the
52-document/214-link knowledge audit. The prior 458-test baseline also passed.
The CM5IO image build and `make image-check` completed successfully. Additional
read-only inspection found both tools as executable AArch64 ELF files (0755),
verified vcgencmd against its package checksum and found the pinctrl license.
All 219 installed packages have consistent states with no pending dpkg updates;
`libraspberrypi-bin` and `libraspberrypi0` are
`0~20230913+gitcc1ca18-0ubuntu2`. Neither raspi-config package is installed.

The new `image/deploy/cm5-io-board-ubuntu-noble-base-none-arm64.img` is
1,611,661,312 bytes, SHA-256
`3920fe2ac88b83373bdd053a46f41634bdc35ee72d92ca3565bfc589b494bdb4`.
It has not been flashed or booted. The original flashed image and sidecars
were copied and its original SHA-256 verified under ignored
`image/work/cm5-io-board-20261027/original-image-2f7265558fb9/`.
New build, inspection and tool receipts are retained in the parent work directory
as `pi-tools-image-build.log`, `pi-tools-image-check.log` and
`pi-tools-artifact-check.json`.

**Device boundary at the tools checkpoint:** CM5IO was disconnected. Its earlier overclock session
installed and used Ubuntu's vcgencmd package; that observation is separate from
preinstallation in a newly generated image. No device change, flash or reboot
is performed in this tools integration, and the pending overclock cleanup below
is unchanged.

## CM5 IO Board overclock selection and fan control (2026-10-28)

**Device observations:** on the 4GB/32GB CM5 with official IO Board, a third-party
Pi 5-style cooler is connected to the fan header; its exact model and the PSU
rating are unknown. At PWM 75 the tachometer reported 0 RPM; PWM 175 started
from rest and sustained about 3000 RPM, while PWM 255 sustained about 6400 RPM.
The device-specific curve now uses 40/50/60/70°C, PWM 175/200/230/255 and 5°C
hysteresis. Live DT matched after a normal boot, and the user confirmed continuous
rotation. This is not an official cooler or a new generic image default.

At stock 2.4 GHz the corrected-curve CPU/VM mixed180 control passed, peak
55.1°C and sampled protection flags zero. Earlier, 2.8 GHz / delta0 completed
the full staged short test (20s int64, 90s CPU all, 45s VM, locked 256MiB
memtester one loop), peak 68.85°C with the original fan curve and flags zero.
Its int64 metric was 8296.24 versus stock 7117.09 bogo ops/s; these are single
method observations, not a whole-system performance guarantee.

2.875 GHz / delta0 passed an earlier staged test but failed mixed180 with 11
memory bit errors. With the corrected fan curve it again failed VM verification
with one `swap bytes` bit error, exit 2, peak 60.05°C; memtester was not reached.
An additional 25mV attempt stopped after about 22s on new `0x50000` historical
undervoltage/throttle flags, peak 52.35°C. 3.0 GHz / delta0 produced a telemetry
command SIGSEGV and kernel trace; +25mV and +50mV attempts lost networking and
have no complete passing receipt. Full power and USB disconnection was needed
to recover; the watchdog did not guarantee automatic recovery from these faults.

**Selection and exact stop boundary:** the user stopped further probing and
selected 2.8 GHz without added voltage. The normal boot configuration was
written, fsynced and read back with SHA-256
`e3ebddbfe8ffce031117ecb385cfd4100d7f979ee8108ae44cbd36356e9b58c2`,
preserving the new fan curve and original normal cmdline, with force_turbo=0
and initial_turbo=0. This round's tryboot/cmdline-oc files were removed and a
normal reboot requested. The user then disconnected CM5IO. Final SSH/clock
verification and reboot smoke testing were not completed at that stop boundary;
2.8 GHz mixed180 was not performed. The later USB checkpoint above records
reconnection and functional reboots, separately from overclock validation.

The temporary PID1 watchdog drop-in
`/etc/systemd/system.conf.d/90-cm5io-oc-watchdog.conf` remains installed with
15s Runtime/Reboot settings. Its removal, daemon-reexec and RuntimeWatchdog=0 /
inactive confirmation remain pending after the next device verification. Original
configuration backups and device test directories remain; full directory archival
was not completed. Local receipts/logs are under ignored
`image/work/cm5-io-board-20261027/`. The host remains awake as requested.

**Derived conclusions and unknowns:** the default low PWM is unsuitable for
this specific fan, but the exact minimum was not measured. Lower temperature
did not make 2.875 GHz pass correctness checks; this does not establish physical
RAM damage or isolate the PSU/cable/silicon cause. The repository's generic
image remains at default CPU frequency. Long-term stability, full-RAM coverage,
repeated cold starts and other devices remain unknown. See the
[overclock note](../knowledge/hardware/cm5-io-board-overclock-testing.md) and
[fan note](../knowledge/hardware/cm5-io-board-fan-control.md) for evidence and scope.

**Repository verification before commit:** `make check` passed all 458 local
tests, the pinned kernel patch application checks, and the knowledge audit
(52 Markdown documents, 214 local links, zero errors). `git diff --check` was
clean. These are offline repository results and do not complete the pending
device reboot verification or watchdog cleanup.

## CM5 IO Board first boot and eMMC speed observation (2026-10-28)

**Device observations:** after the user removed nRPIBOOT and power-cycled the
official IO Board, the flashed image booted from eMMC. Password SSH login as
`pi` / `1` succeeded. Boot ID is `00000000-0000-4000-8000-000000000019`;
the running kernel is `6.18.57-v8-16k-uconsole`, architecture AArch64 with
16,384-byte pages, and `/etc/os-release` reports Ubuntu 24.04.5 LTS. The boot
configuration selects the stock CM5IO DTB and standard KMS/UART0 overlays.

The mounted ext4 root is `/dev/mmcblk0p2`, PARTUUID `021d3d33-02`, matching the
image receipt. Automatic growth completed successfully: partition size is
30,730,599,936 bytes and `df` reports 30,209,093,632 filesystem bytes (about
28.1 GiB). The completion marker exists and the service log records successful
online resize. Ethernet DHCP and DNS resolution work through NetworkManager;
SSH, host-key generation and serial getty services are successful. Systemd
reports `running` with no failed units at inspection.

`tty1` and `ttyAMA0` are registered active consoles. GPIO14 and GPIO15 report
TXD0 and RXD0 respectively, and `serial-getty@ttyAMA0.service` is running.
This confirms UART software state; no external serial adapter was connected
for TX/RX or login verification.

**Simple speed observation:** one 256 MiB temporary file on the eMMC ext4
root was written with `dd bs=4M count=64 oflag=direct conv=fdatasync`, then read
with `iflag=direct` to `/dev/null`. Remote monotonic elapsed times were:

| Operation | Time | Throughput |
| --- | --- | --- |
| Sequential write, including fdatasync | 2.435 s | 105.1 MiB/s |
| Sequential read | 0.794 s | 322.5 MiB/s |

The temporary file was deleted. Direct I/O bypasses the Linux page cache;
this single short sample does not establish steady-state, random-IO or
endurance performance and has no pass/fail speed threshold. Private raw and
JSON evidence is retained as `boot-probe.{log,json}` and
`emmc-speed.{log,json}` under ignored `image/work/cm5-io-board-20261027/`.

**Other observations and unknowns:** the boot logs contain a PCIe link-down
message, Bluetooth dummy-regulator messages, a missing CM5-specific first
brcmfmac firmware request (wlan0 nevertheless registers), and an unavailable
Yama ptrace sysctl. These were not treated as a blanket startup failure or
silently removed. Wi-Fi/Bluetooth connections, physical UART, HDMI, M.2,
other IO, power behavior and repeated cold starts remain untested. Systemd
reports 1.784 s kernel plus 6.864 s userspace; this is a boot-timing observation,
not a measured power-on-to-ready interval or a performance test result.

## CM5 IO Board build and eMMC write (2026-10-28)

**Build evidence:** both upstream submodules are initialized at the manifest
pins. Native `make kernel-build BOARD=cm5-io-board JOBS=12` built release
`6.18.57-v8-16k-uconsole`, artifact identity `5a3ea76a2af78538`; the strict
`kernel-artifact` check passed. The selected project patch series is empty.
The CM5IO DTB plus standard KMS and UART0 overlays also merged successfully
with the kernel's DTC tool; this is an offline result, not a GPIO observation.

`make image-build BOARD=cm5-io-board PROFILE=ubuntu-noble-base` built the ARM64
Ubuntu Base 24.04.5 image with no desktop. The final package inventory contains
215 installed packages; no temporary GCC/development packages or desktop
applications remain. `make image-check` with the same board/profile and
`IMAGE_USER=pi` passed. Additional read-only checks confirmed the requested
`pi` / `1` password, `tty1` and GPIO UART `ttyAMA0` consoles, matching fstab and
kernel partition references, multi-user target, Ethernet DHCP, enabled root
growth and SSH services, and fresh machine-id/SSH host-key generation policy.
The target package database has no unfinished package states or pending dpkg
updates. These file checks do not establish a successful first boot.

| Artifact | Value |
| --- | --- |
| Archived flashed image | `image/work/cm5-io-board-20261027/original-image-2f7265558fb9/cm5-io-board-ubuntu-noble-base-none-arm64.img` |
| Image size | 1,611,661,312 bytes |
| Image and device readback SHA-256 | `2f7265558fb9ca3cdfbe1d6f91392438680690ca46f06e207ff1de7941b44278` |
| Initial partition sizes | 512 MiB FAT32 boot; 1 GiB ext4 root |
| Target capacity | 31,268,536,320 bytes (32GB eMMC) |

**Device observation:** under the user's explicit flashing authorization, the
same CM5 USB mass-storage instance was reidentified by its USB identity,
physical path, disk sequence and capacity, checked unmounted, then opened
exclusively. The image was written, fsynced, the block cache invalidated and
the complete image range read back. Readback SHA-256 matched the image and
build receipt. This verifies the written image range, not erasure of unused
eMMC space. Build/inspection logs and private identity/flash receipts are
retained in ignored `image/work/cm5-io-board-20261027/`; image metadata,
package inventory and checksum sidecar are beside the image.

**Initial flashing boundary:** that session stopped without a CM5 power cycle
or nRPIBOOT jumper removal. The subsequent user-controlled power cycle and
first-boot verification are recorded above; physical UART and other IO still
have their stated limits. Image generation and physical flashing were separate
operations; ordinary build/check commands still never write physical media.

## CM5 IO Board and portable engineering baseline (2026-10-27)

**Implementation/source findings:** `cm5-io-board` is selectable in the menu
and direct CLI, restricted to Ubuntu Base 24.04.5 ARM64 with no desktop. It uses
the stock CM5IO DTB, BCM2712 configuration and an empty project patch subset.
uConsole panel/backlight/PMIC adaptation, GPIO audio, rotation and desktop
payloads are omitted. Kernel reports and work directories are board-specific;
shared package/firmware directory names remain project conventions.

The target has independent settings under `image/board/cm5-io-board/`, including
the requested development account `pi` / `1`. It does not read uConsole local
credentials or network/kernel overrides. Preparation hashes the password and
excludes raw settings TOML from snapshots. GPIO UART0 on GPIO14/15 (pins 8/10)
is enabled at 115200 8N1, with `console=ttyAMA0,115200n8` and an explicitly
enabled serial getty; HDMI text console remains available. Source references
and RP1 initialization limits are in the board note（工程源码：`board/cm5-io-board/README.md`）.

Seven portable skills and five references were adapted from the user-provided
reference engineering project. Seventeen focused knowledge articles were added;
all 32 pre-existing articles were reviewed and revised around conclusions,
methods, mechanisms and evidence. Each knowledge document begins with TL;DR
and has fewer than 300 lines. Historical measurements retain their hardware,
software, boot identities, evidence and unknowns. Source-project private data,
unrelated hardware and product code were excluded. The
[adaptation inventory](reference-adaptation.md) records actual reuse and scope.

Three tools are available: structural knowledge audit, separate zstd/Zip64 image
compression and explicit-port UART receive capture. Menu selection now uses
private temporary files and atomic replacement. These are human CLI tools;
existing Makefile.agent JSON schemas and exit semantics are unchanged.
Commands and tool-specific exits are in tools/README.md（工程源码：`tools/README.md`）.

**Offline verification:** the local unit/fixture suite passed 458 tests,
including account hashing/isolation, menu restrictions and failed-save
preservation, stock board metadata, prohibited uConsole payloads, UART getty
link/template, compression roundtrips and transcript UTF-8/partial lines.
The knowledge audit passed for 50 Markdown documents (49 articles plus index),
with no structural or local-link errors. These checks are independent of the
historical build/device results recorded below.

**At the implementation checkpoint:** both upstream submodules were still
uninitialized; handoff reported script exit 3 (make exit 2). No kernel/image was
built or storage written during that implementation phase, and no UART was
opened. Subsequent build, flash, first-boot and networking results are recorded
above; physical UART login, HDMI and other IO remain unverified. The earlier
rpiboot enumeration and sampled eMMC reads established only the USB recovery
path, not this system's bootability.

## Earlier uConsole display observations

Earlier CM4 trials reproduced DSI timeouts or a backlight-only display with
both project and reference kernels. After the successful CM5 comparison and
physical module exchange on 2026-10-18, the user reports visible CM4 output.
Its original `6.18.57-v8-uconsole` Image, installed panel module, boot
configuration and board overlay match the last black-screen setup; the new
boot has no captured DSI timeouts. Root cause and startup repeatability remain
unknown: power/reset state and incomplete seating or intermittent connector
contact are alternative explanations, with no isolated cause established.
See the [CM4 recovery evidence](../knowledge/boot/cm4-uconsole-boot-debug.md#cm4-recovery-after-the-cm5-comparison).

On 2026-10-18 the project CWU50 source was built as a single external module
against the working CM5's exact `6.12.94-v8-16k+` headers/configuration and
symbol table, then loaded in place of its original panel module. The kernel,
live device tree, RP1 DSI/backlight drivers and supply configuration remain
unchanged. LightDM was stopped for both fixed-color-bar framebuffer stages.
The user confirms visible color bars with the project candidate loaded:
this warm module replacement passes the visual display criterion on the
current CM5 stack. Original-driver removal produced DSI/regulator warnings,
and RP1 rebind changed the connector number with an allocation warning.
Retained regulator enable references prevent this result from validating
independent cold-start power sequencing. See the [controlled CM5 comparison](../knowledge/display/cm5-panel-module-comparison.md)
for module fingerprints, limitations and the original-module recovery path.

## Offline repair completed: CM4 diagnostic staging (2026-10-25)

A diagnostic archive was mistakenly extracted directly at the running CM4's
root. Its top-level `lib/` directory replaced the existing `lib -> usr/lib`
symlink. Original libraries remained under `/usr/lib`, but the ELF interpreter
path was broken: SSH authentication succeeded and shell startup reported
`/bin/bash: No such file or directory`; starting sudo also failed with ENOENT.
At the staging failure, no reboot or suspend had occurred. Normal boot
configuration, kernel and initramfs were not changed; the candidate copy failed
before depmod, initramfs generation or tryboot activation. The user subsequently
reported a power-on with no display or backlight. The broken interpreter path
is sufficient to prevent userspace boot.

The user attached the original SD for offline repair. Read-only inspection
confirmed that `/lib` contained only the candidate module tree and that the
original loader and normal modules remained under `/usr/lib`. Unmounted
`e2fsck -fn` completed with exit 0. All five normal boot hashes captured before
extraction matched the card.

The wrong directory was preserved as `/lib.failed-s2idle-20261025`; the relative
`lib -> usr/lib` symlink was restored. The loader bytes, all 416 firmware files
and the five root `/boot` files remained unchanged. All 90 preserved module
files matched a host-side archive of the original wrong directory. With the
card remounted read-only, its ARM64 Bash and `/usr/bin/true` executed through
the host's existing binfmt/QEMU; its loader resolved Bash's libc/libtinfo
dependencies inside the SD root. The card was synced, unmounted and safely
powered off. No image was flashed or candidate activated.

Physical display and SSH recovery still require a boot of the repaired card.
Further sleep or kernel deployment experiments remain stopped. Evidence is in
`~/.cache/uconsole-desktop-20261025/powerkey/sd-recovery/` (`before.json`,
`after.json`, `wrong-lib-before.tar`, `fsck-readonly.log`); the durable handoff
is `HANDOFF.md` in the parent directory. The diagnostic archive remains unsafe
for extraction at `/`. The host's separate 04:00 shutdown is retained; loss of
network access is not evidence of remote shutdown.

## Power-key gestures and system sleep (2026-10-25)

Both labwc client plans now run a dedicated evdev power-key handler under a
`handle-power-key` block inhibitor acquired inside the actual graphical login.
Only KEY_POWER-only devices are grabbed. A short release toggles output power;
a hold reaching two seconds turns the output on and opens the selected shell's
power menu once. Startup-held keys, input loss, VT changes and compositor exit
have explicit cleanup/suppression paths. The global logind poweroff policy and
PMIC hardware thresholds are unchanged.

Device observation, unchanged boot `00000000-0000-4000-8000-000000000035`:
UID 1000 holds the inhibitor and a second evdev grab returns EBUSY. The user
confirmed physical short-press off/on and the two-second Noctalia menu. Logs
contain short/short/menu, the final output is on with `bl_power=0`, and no
failed units appeared. The temporary launch binding was removed and original
rc.xml bytes restored. Existing scale/rotation, audio services and the live
user's temporary lock-disable preference remain. Waybar dispatch and gesture
lifecycle have software coverage; Waybar physical keys, CM5 and other cores
remain unvalidated. The original feature check passed 398 tests with one
optional skip. A subsequent local process-group cleanup fix covers menu
descendants and inhibitor-wrapper early exit; the final host check including
offline staging passed 424 tests with one optional skip. The cleanup revision
has not been redeployed to the device.
See [desktop power policy](../knowledge/power/desktop-power-policy.md).

Actual system sleep is a separate research track. The running kernel disables
SUSPEND and HIBERNATION; source review found the BCM2711 GPIO parent IRQ wake
request returns EINVAL, so enabling SUSPEND alone cannot establish power-key
wake. An isolated `6.18.57-v8-uconsole-s2idle-r1` diagnostic kernel and 76
runtime modules were built with the native host toolchain. ARM64/gzip headers,
vermagic, dependency closure and depmod symbol/version checks passed offline;
a matching software `rtc-test` module was also built. These are not in the
production patch series, configuration or exported images. Device staging
failed as described above; no candidate boot or PM test occurred.
Deep suspend lacks a registered platform entry with the current spin-table
boot, and ARM64 hibernation explicitly rejects its unavailable secondary-CPU
offline path. No real suspend/resume or power saving is established yet.
See [CM4 sleep and wake research](../knowledge/power/cm4-system-sleep.md).

## Refreshed Noctalia image (2026-10-25)

The current CM4 Ubuntu Noctalia image was rebuilt from an empty rootfs and
exported to
`image/deploy/uconsole-cm4-ubuntu-noble-labwc-noctalia-labwc-noctalia-arm64.img`:
3,222,274,048 bytes, SHA-256
`e96cb4d2cfe9fdf55b08199fea15feffca1d095ec938799fe3d0e659ab9a3556`.
It retains the explicitly selected production `6.18.57-v8-uconsole` kernel;
no s2idle candidate is included. The previous export and receipts were preserved
under `~/.cache/uconsole-desktop-20261025/image-previous/`.

This export includes the browser/media applications, screenshot dependencies,
Celluloid defaults, Amberol layout, lockscreen/idle policy and final power-key
handler. Installed versions include Firefox `158.0.1~build1`, Celluloid
`0.26-1build2`, Amberol `0.10.3-3ubuntu1`, Noctalia `5.3.0+uconsole1` and
Wayland stack `0.20.2+waybar0.15.0-1`. A clean build exposed the missing PATH
schema compiler: Noble's GLib runtime only provides a private copy.
Both desktop plans now explicitly install `libglib2.0-bin` (`2.80.0-6ubuntu3.10`
in this export) before compiling the GSettings override.

`make image-check` passed with the matching retained-kernel report/hash and
the existing host inspection wrappers. It checked the nine kernel inputs,
module/initramfs integration, preset user/SSH and desktop payload; the completed
rootfs also retains `lib -> usr/lib`. Inspection loop devices were detached.
The final Ubuntu suite passed 58 tests with one optional native-GLib skip;
the updated desktop dependency check passed separately. Build and inspection
logs and the export summary are in
`~/.cache/uconsole-desktop-20261025/powerkey/`.

This file has not been flashed or booted. It does not repair the existing SD
installation described above. Waybar refresh attempts failed during signed APT
index acquisition with TLS handshake errors; no refreshed Waybar image was
published. The Noctalia build succeeded through the authorized local proxy;
repository signatures and TLS verification remained enabled.

## Desktop peripheral integration (2026-10-24)

Boot optimization is paused at the user's request. On the existing CM4 Ubuntu
SD installation, the kernel already exposed `hci0` and loaded BCM4345C0 firmware,
but BlueZ and the PipeWire Bluetooth SPA plugin were absent. Both labwc desktop
recipes now explicitly include `bluez` and `libspa-0.2-bluetooth`; headless profiles
retain their separate package selection.

Device observation, boot `00000000-0000-4000-8000-000000000035`: BlueZ
`5.72-0ubuntu5.8` and the SPA plugin `1.0.5-1ubuntu3.3` are installed. After a
fresh desktop session and a user WirePlumber restart, ordinary-user Noctalia
Bluetooth disable/enable commands matched actual BlueZ `Powered` false/true
and shell off/on. The original on state was retained, with no discovery or
pairing. Audio Sink/Source and Handsfree UUID registration establishes local
profile availability, not Bluetooth audio playback. Discovery, pairing and
real audio remain unvalidated. See the [Bluetooth integration note](../knowledge/network/bluetooth-userspace.md).

The same unlocked Noctalia session completed a real ScreenCast portal/PipeWire
roundtrip: Create/Select/Start response 0, valid remote FD and matching node,
75 mapped RGBx frames in an 8-second acquisition. Two saved frames show the
independent controlled window in red and blue, satisfying the visual oracle;
frame/change counts are observations rather than performance limits. The raw
720×1280 buffer carries transform metadata 3, while the portal reports the
853×480 logical output. All probe sessions, nodes and processes were cleaned;
production portal configuration and the 300-second lock policy were unchanged.
Browser/OBS clients, shared audio and multiple outputs remain unvalidated.
See the [ScreenCast evidence](../knowledge/desktop/labwc-shell-comparison.md#屏幕共享的真实取帧验证2026-10-24).

Both labwc desktop recipes now include `gvfs`, `udisks2` and `xdg-user-dirs`.
On the same CM4 installation, UID 1000 completed a temporary-file trash/restore
roundtrip with identical bytes. Isolated XDG defaults and preservation of an
existing custom mapping passed; the real pi account had no mapping and now has
eight standard directories. The deployed client launcher runs the ordinary-user
directory updater without forcing a reset.

A client launched from the actual graphical logind session discovered a
32 MiB file-backed ext4 loop volume through GIO, mounted it with UDisks,
wrote/read identical UTF-8 bytes, unmounted it and deleted the loop device.
The SSH caller's different authorization context was kept separate. All owned
test mounts/devices and the terminal were removed, and the original locked
state was restored. This validates the local userspace chain, not physical USB
hotplug, transfer or power. See the [desktop storage note](../knowledge/desktop/removable-storage.md).

The CM4 Ubuntu Noctalia image was refreshed after the storage and PAM
integration as
`image/deploy/uconsole-cm4-ubuntu-noble-labwc-noctalia-labwc-noctalia-arm64.img`
(2,416,967,680 bytes), SHA-256
`6a2b34c732701e0ede98dae43b4ed7353b0177f7be961e144727873adcb75184`.
It retains the explicitly selected `6.18.57-v8-uconsole` kernel and installs
`uconsole-noctalia 5.3.0+uconsole1`, Wayland stack `0.20.2+waybar0.15.0-1`,
and the BlueZ/SPA versions above. GVfs is `1.54.4-0ubuntu1~24.04.4`,
UDisks2 `2.10.1-6ubuntu1.5`, and xdg-user-dirs `0.18-1build1`.
`make image-check` passed with the matching retained-kernel report/hash;
supplemental read-only ext4 inspection confirmed the new client launcher bytes
and the `-session optional pam_lastlog.so` rule. All inspection loop devices
were detached. The host suite completed 349 tests with one optional skip;
patch checks passed. This exported file has not been flashed or booted.
The live-system functional observations above remain separate from fresh-card
validation. Build, inspection and source-check logs are retained under
`~/.cache/uconsole-desktop-20261024/functional/storage-*`.

Snapshot boundary: the screenshot tools, lockscreen transition policy,
browser/media applications and Amberol layout below were added after this
export. Its package
manifest contains neither `grim` nor `slurp`, nor the new application set.
The 2026-10-25 Noctalia export above closes this boundary. Waybar's current
preparation plan includes the changes, but its refresh has not been exported.

Both desktop plans now explicitly include `grim` and `slurp`, required by the
configured wlr Screenshot backend. The live CM4 already had grim
`1.4.0+ds-2build2` but no slurp: an interactive public Screenshot request failed
with response 2 and no URI. Installing only slurp `1.5.0-1` made the same path
return response 0 and a readable PNG containing the controlled titled window.
Fresh Waybar and restored Noctalia sessions both completed real region
selection and image-content checks. Noctalia PickColor returned exact RGB
`(1, 0, 0)` at a known pure-red window point. The originally absent
`/tmp/out.png` was absent again after owned-output cleanup. These observations
are separate from ScreenCast and fresh-image validation; see the
[screenshot integration note](../knowledge/desktop/screenshot-portal.md).

The stock Noble login stack references optional `pam_lastlog.so`, which its
PAM build omits. The Ubuntu backend now prefixes only the bare optional rule
with PAM's documented `-session` form when the module is absent; authentication,
control rules, file mode and custom configuration remain unchanged. Host
preservation/idempotence tests pass. The same helper was deployed on CM4 with
an original-file backup; only one prefix byte changed and mode/owner were kept.
A native isolated libpam probe returned 0 for start/open/close/end both
before and after; the missing/faulty-module logs
appeared only before. Temporary probe services and executables were removed.
Subsequent normal greetd initial-session starts opened Waybar session 122 at
19:47:24 and Noctalia session 147 at 19:54:59. Both recorded the pi PAM session
opening with no lastlog missing-module logs. This covers the real desktop
session stack, not new interactive password authentication or cold boot. See the
[PAM integration evidence](../knowledge/rootfs/ubuntu-base-integration.md#noble-的可选-pam_lastlog-日志).

The fresh Waybar session also completed an owned Mako notification lifecycle:
Notify returned an ID present in the notification list; CloseNotification for
that ID removed it without clearing user notifications. Its ScreenCast request
returned successful Create/Select/Start responses, a matching PipeWire FD/node
and saved frames containing the controlled titled red and blue window. The
64 frames and 63 changes are observations, not performance thresholds. See the
[dual-session verification](../knowledge/desktop/labwc-shell-comparison.md#双会话暖切换复验2026-10-24)
and [pixel evidence](../knowledge/desktop/labwc-shell-comparison.md#屏幕共享的真实取帧验证2026-10-24).

Switching back to Noctalia removed all owned Waybar desktop processes while
preserving the three user audio service PIDs. Its original selector bytes and
locked state were restored, with the same boot ID and 300-second lock policy.
An additional bounded FileChooser request did not complete after automated
input and was explicitly closed; no success or regression cause is established.
The GTK portal exited on the old compositor's broken Wayland connection and
was started normally in the restored session. Final system/user failed-unit
lists are empty; no owned probes, temporary input devices or portal sessions
remain. Evidence is under
`~/.cache/uconsole-desktop-20261024/functional/waybar/`.

The user subsequently reported stuttering lock/unlock animations. Although
ordinary UI animation was already disabled, the CM4 effective lockscreen
configuration still contained the six default effects with a 1500 ms duration.
Both the project default and current pi configuration now set
`[lockscreen] transition = []`, the pinned upstream's explicit switch for both
transitions. Validation passed and live config reload returned ok; the merged
export confirms an empty list, with locking enabled, zero grace period and the
300-second idle policy retained. GUI overrides were unchanged. Upstream caches
the effect when locking: the already active lock may still animate on its first
unlock; the next new lock/unlock cycle uses the empty pool. The device is
still locked on the same boot, with no failed system or user services; no visual
frame-rate improvement is claimed from configuration checks. The previous image
export also predates this lockscreen policy. See the
[lockscreen note](../knowledge/desktop/labwc-shell-comparison.md#锁屏与解锁的独立过渡).

## Desktop applications (2026-10-24)

Both Ubuntu labwc desktop plans now include Firefox, Celluloid, Amberol,
`gstreamer1.0-libav` and `fonts-noto-cjk`. The application list, Mozilla APT
source, scoped keyring and MIME defaults belong to the Ubuntu provider;
headless profiles do not receive them. Mozilla's native ARM64 Firefox replaces
Noble's Snap transition
package selection. The source permits only Firefox from that repository and
rejects Ubuntu's transition package; it does not broaden global APT trust.
The reviewed key bytes and full fingerprint are pinned in project inputs.
They were obtained from Mozilla's official HTTPS key endpoint and used to
verify the current repository InRelease signature. This does not claim a second,
independently authenticated publication of the key identity.

Distribution MIME defaults preserve modern and legacy user overrides. They
explicitly keep directories assigned to Thunar: without that default, GIO on
the device selected Amberol because its upstream desktop file declares
`inode/directory`. Ubuntu application payloads are snapshotted during
preparation and checked again before execution; image inspection verifies their
exact bytes, file type,
mode and declared installed packages. Both Waybar and Noctalia preparation
completed with the five application/support packages and matching input
hashes. After the final source, directory-default, CJK and compact-layout
adjustments, the host suite completed 358 tests with one optional skip, and
patch checks passed.
The affected Ubuntu integration subset also completed 25 tests with one
optional skip. No new image was exported or flashed.

Device observation on the same CM4 boot: native Mozilla Firefox
`158.0.1~build1`, Celluloid `0.26-1build2` and Amberol `0.10.3-3ubuntu1` are
installed without Snap. Firefox's support page reports the Wayland window
protocol. A controlled local page rendered RGB blocks and, after installing
Noto CJK, readable Chinese; keyboard interaction produced the expected
JavaScript result. Firefox and Celluloid appeared in Noctalia launcher searches
and opened their application windows. Celluloid rendered the known video and
English/Chinese sidecar subtitles, with playback controls inside the output;
its preferences window opened. A tagged media input was discovered by Noctalia,
and shell pause/play commands matched player status and stopped/advancing
positions. Untagged input was filtered by the pinned shell's metadata policy;
restarting the shell was unnecessary.

Amberol's original minimum layout clipped the bottom controls at scale 1.5.
The reviewed GLib resource overlay reduces cover size and vertical spacing,
preserving font and button sizes. No-cover and embedded-cover/long-Chinese-title
screenshots contain the complete playback, volume, queue, shuffle and repeat
controls. GIO opened the audio MIME default through the project wrapper, and
the resulting process used the installed overlay. A version and whole-resource
hash guard falls back to the original app after incompatible updates or explicit
user resource overrides; no application recompilation is involved. Noctalia's
play/pause roundtrip also advanced/stopped the known MP3 position.
In a fresh instance with isolated XDG state, the explicitly identified AAC
track advanced from 0 to 1 second while playing, then remained at 1 second
after Noctalia pause. Earlier attempts reused the existing single-instance
player or queried before its MPRIS name appeared; their incomplete observations
were retained separately rather than treated as codec failures.

At the user's request the live pi configuration now temporarily sets
`idle.behavior.lock.enabled = false`; validation and reload succeeded. The
backup is `~/.cache/uconsole-applications-20261024/noctalia-before-temporary-lock-disable.toml`.
The repository's 300-second lock default and disabled transition policy remain
unchanged. Hardware decoding, online DRM/services, exhaustive codec coverage
and physical audio/video synchronization are not established by these probes.
Owned application windows and the probe-only playlist were removed; audio was
restored to the original unmuted 40 percent. The original PipeWire,
PipeWire-Pulse and WirePlumber PIDs remain, the boot ID is unchanged, and system
and user failed-unit lists are empty. Noctalia is unlocked with no active panel
and no remaining MPRIS players. Evidence is retained under
`~/.cache/uconsole-desktop-20261024/applications/`.
See the [application selection and verification note](../knowledge/desktop/desktop-applications.md).

## Desktop power policy (2026-10-25)

The Noctalia defaults now warn at 20 percent, disable the idle fade, and turn
outputs off after 600 seconds with a paired input-resume command. Both labwc
recipes include `wlopm`; Noctalia uses a custom idle command because the pinned
upstream's generic labwc backend calls `wlr-randr --off/--on` without an output
selector. Its native menu/IPC power commands retain that upstream limitation.
The default 300-second lock and empty lock-transition list remain; the live
user's temporary disabled idle lock is preserved. No automatic suspend is
configured: this CM4 exposes no `/sys/power/state` values and logind reports
`CanSuspend=na`.

Device observations on the same boot `00000000-0000-4000-8000-000000000035`:
UPower 1.90.3 already uses percentage thresholds 20/5/2 with effective critical
action `PowerOff`. A temporary Noctalia warning threshold of 100 generated a
real `Battery low` notification carrying the unchanged 64-percent reading;
the notification was captured and only its owned ID closed, then the threshold
was restored to 20. No battery values or system critical thresholds were changed.

A temporary three-second idle timeout automatically changed `DSI-1` and DRM
power to off and backlight `bl_power` from 0 to 4. Injected keyboard input
automatically restored on/On and `bl_power=0`, retaining brightness 4/9.
The restored screenshot contains the normal landscape desktop. The final
fallback-on command ran after those automatic-resume observations. The timeout
is back at 600 seconds. In a repeated 15-second trial, the user confirms the
backlight completely turns off and visible output restores normally. Wake input
was injected; physical keyboard/trackball wake, critical shutdown, battery
calibration and real suspend were not tested.

The host suite completed 360 tests with one optional skip and patch checks
passed. Current image inputs include the policy; the exported image is unchanged.
See the [power-policy evidence](../knowledge/power/desktop-power-policy.md).

## CM4 video playback (2026-10-25)

Celluloid now receives the Ubuntu distribution GSettings default
`--hwdec=auto --profile=fast`; explicit user settings retain precedence. The
override participates in preparation and byte inspection, and schema compilation
runs after package installation even when APT finds the application already
installed. A native GLib backend test verifies existing user values survive and
reset reveals the distribution default. Both Waybar and Noctalia preparations
include the current power payload, `wlopm` and the new override. The exported
image is unchanged.

Same-boot device observations: H.264 automatic decoding actually opens
`/dev/video10` and reports `v4l2m2m-copy`. Hardware decoding alone leaves
substantial video-output drops. With the fast rendering profile, two 720p30
stable sampling windows add zero output drops over 15.54/15.52 seconds; stock
controls add 214/215 over 15.48/15.49 seconds, including the reversed repeat.
1080p30 improves from 319 drops with automatic hardware decoding alone to 22
with fast over approximately 16 seconds, so flawless 1080p playback is not
established. All these windows add zero decoder drops. Fullscreen captures show
the controlled input. The lower-quality scaling/dithering tradeoff is recorded
in the [video note](../knowledge/desktop/cm4-video-playback.md).

After deployment, a new player without explicit hwdec/profile arguments selects
the installed default and adds no output drops across 12 stable samples. With a
temporary three-second idle timeout, advancing playback keeps output on; after
the owned player exits, idle screen-off resumes and injected input wakes it.
The original configuration is restored byte-for-byte with 600-second screen-off
and the temporary disabled lock preserved.

HEVC hardware decoding, physical audio/video synchronization and battery life
remain unvalidated. Battery-side whole-device readings and internal AV timing
are observations, not calibrated power or physical synchronization results.
The full host checks complete 361 tests with one optional skip; patch checks pass.
Evidence is under `~/.cache/uconsole-desktop-20261025/video/`.

## Desktop prebuilt distribution

On 2026-10-21 Git LFS 3.7.1 was installed on the Fedora build host and the
repository-local filters/pre-push hook were initialized. The existing Noble
ARM64 Wayland stack and optimized Noctalia packages are now tracked under
`prebuilt/ubuntu-noble/arm64/`, totaling 17,330,696 bytes. Git stores two LFS
pointers; the original sibling JSON receipts remain ordinary Git files.
Package hashes match the desktop checkpoint below. Their builders, recipes,
patches and receipt validators are unchanged.

The shared desktop package facade imports the tracked pair into an absent or
empty native cache before profile availability checks, Ubuntu desktop planning
or image inspection. Nonempty native caches remain authoritative and strict
receipt validation still applies. No build step implicitly fetches LFS objects
or compiles these packages. See the prebuilt instructions（工程源码：`prebuilt/README.md`）
and [package reuse method](../knowledge/build/userspace-package-reuse.md).

Host observations: local LFS object/pointer checks pass. A temporary bare
repository received both LFS objects through the local file transport. A new
clone with smudging disabled rejected pointer inputs with a `git lfs pull`
hint and did not create a cache. After an explicit pull, both packages and
unchanged receipts passed native validators, populated fresh caches and
produced the Ubuntu Noctalia desktop input plan. All 313 host tests also pass
in that clone, including the real dpkg documentation-filtering check using
the existing host-tool wrappers. This did not initialize upstreams, compile
the desktop or generate another image. Evidence is retained in the host's
`~/.cache/uconsole-desktop-20261018/lfs-clean-clone.log`.

GitHub refs and LFS objects have not been pushed; GitHub download/authentication
is not established by the local transport check. A fresh GitHub clone cannot
receive these new package inputs until they are published. No device or SD
card was changed by this integration.

## Ubuntu CM4 Wi-Fi channel survey

On 2026-10-19 the user reported boot-time `brcmf_set_channel` errors with
reason `-52`. The captured errors occur before association; the current Wi-Fi
link and SSH connection work on channel 9. In the pinned driver this path is
channel survey, and firmware errors map to `-EBADE`; the log does not identify
the original firmware error.

The current Ubuntu image was missing `wireless-regdb`, with an explicit
`regulatory.db` load failure. The package and `iw` are now installed on the
device and included in future Ubuntu images. `iw reg reload` succeeds and
changes the global world-domain rules while retaining the connection. Whether
the original pre-association channel errors disappear remains unverified;
this work has not rebooted or disconnected the device. See the
[channel survey evidence](../knowledge/network/cm4-brcmfmac-channel-survey.md).

## Ubuntu labwc desktop checkpoint

On 2026-10-20 the native Ubuntu CM4 build completed for labwc/wlroots 0.20.2
and Waybar 0.15.0. Noble required additional pinned private dependencies:
Pixman 0.46.4, fmt 11.1.4, spdlog 1.15.3 and gtk-layer-shell 0.9.2.
The package is 2,691,292 bytes with SHA-256
`231199876528f40aa0176dfd88265ea6c47b1d933a6fee7c7e65c6d4f6cb593e`.
Its source/builder/validator receipt matches the current project and the package
is cached under `image/work/wayland-stack/packages/`.

Device observations: the package and shared image payload were installed on the
current Ubuntu CM4. In the Waybar checkpoint, greetd started the ordinary `pi`
session on tty1 with labwc, Waybar, kanshi and mako. The compositor's DRM fdinfo
reported V3D bin/render jobs, and grim produced a 1280x720 screenshot with the
status bar, network, brightness, volume and actual battery capacity. Output transform is 270.
The battery reported charging at 90% during the first capture. The user confirms
correct landscape orientation, trackball input and the Ctrl+Alt+T terminal shortcut.
The initial 1.0 scale was too small; the live output and persistent Kanshi profiles
now use 1.5, which is also the default for Ubuntu uConsole desktop image plans.
The updated screenshot retains the complete panel, and the user confirms that
1.5 gives suitable text and control sizes on the physical screen.

In the Waybar session, user audio services and GTK/wlr portal backends were active.
D-Bus exposed FileChooser, Screenshot and ScreenCast. A real FileChooser OpenFile
request completed with response `0` and URI `file:///etc/hostname` after injected
Return input; the visible dialog and returned selection belong to the same
interaction. ScreenCast and audible playback were unverified at that
checkpoint. Ordinary-user brightness initially failed because
`--no-install-recommends` omitted
`brightness-udev`. Installing it and applying the distribution rules makes a
same-value write succeed; the recipe now requires that package. At that
checkpoint, greetd logged an optional missing `pam_lastlog.so` from Ubuntu's
login PAM stack while the user session opened successfully; the later fix and
real session checks are recorded above.

Further device observations: injected terminal and launcher shortcuts start
ordinary-user applications; Mousepad loads no private compositor libraries.
The original percentage brightness increment rounds to zero with a maximum of
nine. Shared controls now round a tenth of the range upward; the brightness
shortcut moves 5 to 6 and back to 5. The volume shortcut moves 40% to 45%, then
the observation restores 40%. Stopping greetd originally left a detached labwc
session because the wrapper did not handle SIGHUP. After the fix, all owned
desktop processes exit and a new session starts, while the separate user
WirePlumber PID remains unchanged.

The Noctalia 5.3.0 native build and package installation are complete. The installed
optimized package is 14,639,404 bytes with SHA-256
`b170d9f487b21e2fabacc39df8915531f54591e87055a43853603b3b934c9e75`.
Its cache receipt validates; the final incremental packaging took 66.027 seconds
and required no source recompile. Noble's Wayland 1.22
lacks `wl_proxy_get_display()`; a focused patch uses the existing main-loop flush.
The source tree matches an independently extracted, patched pinned archive, and
the recipe and receipt verify Meson's actual release optimization `3` (`-O3`).
The public wrapper directly executes the private ELF with its fixed private
RUNPATH, preserving `/proc/self/exe` for the PAM helper. Helper authentication
succeeded, and an actual lock/unlock interaction completed
`locked=false → true → false` with injected password and Return input.

The user initially observed severe Noctalia menu stutter. Two focused rectangle
renderer patches retain the full edge/shadow path while shortcutting safe shape
interiors and skipping fully transparent fill/border submissions. Single-object
rebuilds, including archive/link, took 9.17 and 9.64 seconds. On the actual CM4
V3D 4.2.14.0 / Mesa 25.2.8 driver, all 241 shader cases were byte-identical to the
original; a negative control detected a visible change. The CPU draw oracle's
35 cases also passed. With the same compact configuration and 100 pointer events
40 ms apart, three original-binary trials measured 21.8655–22.0389 ms of GPU
render time per submitted job; both patches measured 14.6776–14.8755 ms. These
are GPU counter observations, not display FPS. The user then confirmed the owl
control-center menu was "明显流畅了" (noticeably smoother). Waybar remains the
default; the subsequent functional checks below permit Noctalia as an optional
Ubuntu Base selection. Full measurement details are recorded in the focused note.

Noctalia first-login defaults enable its policy agent, bind the DSI backlight and
configure a 300-second idle lock, restored after profiling. The compact bar omits
the SSID label at scale 1.5; shadows, regular animations and borders are disabled,
and the control center uses width 600 with compact sidebars. Wallpaper and rounded
corners remain enabled. Noctalia brightness and volume controls changed 4→5→4
(maximum 9) and 40%→45%→40%; original levels were restored. This verifies control
changes, not audible playback. Its UPower dependency is
installed; UPower and sysfs both reported 95% during the same observation, without
invented energy capacity. This does not establish battery calibration accuracy.
On 2026-10-21 the Noctalia FileChooser request returned response 0 and
`file:///etc/hostname`. An independent wlr-data-control client verified exact UTF-8
clipboard transfer in both directions and restored the originally empty selection.
The graphical pi session opened Noctalia's polkit panel for
`pkexec --disable-internal-agent /usr/bin/id -u`; authentication returned UID 0
and exit status 0. Actual launcher/terminal input started ordinary-user applications
without private library mappings or nonempty library overrides. Noctalia logout
removed all five owned desktop processes while preserving the four independent
user audio PIDs; both shells restarted successfully.

PipeWire linked a generated tone to the bcm2835 Headphones ALSA sink, playback
was RUNNING and speaker GPIO11 high. After a repeat at 60% volume, the user confirmed
normal audible output; volume was restored to 40%. This validates the current
speaker route, not headphones, capture or Bluetooth audio.

Matched warm-session observations used the same CM4/output/software, a 20-second
settle, then five samples two seconds apart with no compiler, foreground app or
lock. The scope includes the validated compositor wrapper parent and recursive
labwc descendants. Waybar PSS was 137.7 MiB across nine processes, CPU 0–0.486%
of one core; Noctalia was 187.1 MiB across five processes, CPU 0.482–1.928%.
Independent audio added about 19.3 MiB to each. These are short-window observations
of each delivered appearance, not FPS, resource limits or cold-start measurements.
Noctalia is now selectable with both validated native packages; Waybar remains
the profile default. ScreenCast, complete input coverage, suspend/resume,
cold-start behavior and full CM5 runtime validation remain unverified.
The optional image was exported as
`image/deploy/uconsole-cm4-ubuntu-noble-labwc-noctalia-labwc-noctalia-arm64.img`
(2,416,967,680 bytes), with matching cached native package hashes and the retained
`6.18.57-v8-uconsole` kernel. Initial inspection exposed an incorrect inspector
assumption: Noble enables greetd through its `display-manager.service` alias,
which `graphical.target` wants, rather than a direct wants-directory link.
The inspector now validates that canonical alias and its actual unit file. Final
read-only image inspection passes for package contents/licenses, desktop payload,
account, boot files, initramfs and partition references. The 474-package image
SHA-256 is `4000e46b883194c43935fbb22d3a5cca2d975225d9dfe915f2e8a0e1f4b7dcf0`.
All 305 host checks pass, including real dpkg documentation filtering and
`systemctl --root enable` alias regressions. No card was flashed and the exported
image has not been cold-booted.

The original configuration is backed up under
`/var/lib/uconsole-desktop/20261018/before-desktop/`. The Waybar deployment enabled
greetd and selected `graphical.target`; subsequent warm session switches support the
Noctalia comparison. A later independent boot is recorded separately below.
See the [desktop evidence and comparison boundary](../knowledge/desktop/labwc-shell-comparison.md).

### Subsequent CM4 boot observation

On 2026-10-21 SSH reached a new boot,
`00000000-0000-4000-8000-000000000016`, running the original Ubuntu Base installation
with the installed Noctalia session, not the newly exported desktop image.
systemd reports 3.302 s kernel plus 3.780 s userspace, totaling 7.083 s;
this excludes firmware and does not measure visible desktop readiness.
The compositor process was created at 7.63 s, its primary clients at 10.90 s,
and kanshi/Noctalia at 11.00 s on the kernel-relative process clock. Audio units
were active by 7.433 s. The largest observed interval is before primary-client
creation; no specific KMS/EGL bottleneck has been established.

At about 155 s uptime the screencopy contains the wallpaper and complete top bar;
DSI-1 reports transform 270 and scale 1.5. Failed units are empty, the CPU retains
ondemand/2200 MHz, and firmware protection flags are zero. The captured dmesg has
no DSI transfer timeout or channel-survey -52 message; this is one boot observation,
not a permanent-fix claim. FAT reports an unclean prior unmount; no mounted
filesystem repair was attempted. The user subsequently confirms normal landscape
desktop, trackball and top-menu behavior; the cold/warm boot type remains unknown.
Firmware timing is unavailable because vclog is absent.

The old live Wayland package lacked 15 license documents from before the scoped
dpkg include fix. Reinstalling the identical cached package with that include
restored them. Both desktop packages now have empty dpkg verification output;
the compositor and Noctalia PIDs/start ticks were unchanged. The live compositor
library-path setting remains null, while the exported image explicitly selects
the private stack path; live startup does not establish fresh-image equivalence.
CM5 package/ABI and deployment prerequisites are recorded in the
[package reuse note](../knowledge/build/userspace-package-reuse.md#板级职责单独适配).

Another independent boot, `00000000-0000-4000-8000-000000000012`, reports 7.092 s
systemd startup and a 3.27 s labwc-to-primary-client creation interval. A temporary
debug warm session shows a 0.43 s interval. A measured system reboot,
`00000000-0000-4000-8000-000000000010`, reproduces about 3.30 s. Its largest
pre-client log gap is 1.636 s from headless-backend creation to the first successful
EGL client-extension query, versus 0.077 s warm. Source bounds this before
eglInitialize, shader linking and first-frame modesetting; individual calls are
not timed, and mapped Mesa/Gallium/LLVM libraries do not prove the cause.
The original compositor configuration is restored byte-for-byte, temporary
profiling units/wrappers are removed, and failed units are empty. Detailed
[startup evidence](../knowledge/desktop/labwc-shell-comparison.md#后续启动的阶段边界)
remains separate from physical cold-start timing.

### First Mesa vendor-library load

On 2026-10-22 a small native LD_PRELOAD timing probe localized the earlier
pre-client gap without rebuilding labwc, wlroots or Mesa. Two measured system
reboots, `00000000-0000-4000-8000-000000000002` and
`00000000-0000-4000-8000-000000000023`, spent 1.636 and 1.620 s respectively
inside `dlopen("libEGL_mesa.so.0")` during the initial no-display EGL query.
Warm-session equivalents were 0.073–0.074 s. Subsequent `eglInitialize()` took
only 0.010–0.011 s on those first sessions.

The second first-session load used 0.213 s of process CPU time, incurred 403
major faults and accounted for 59,748,352 bytes (56.98 MiB) of input; its warm
control had zero major faults/input blocks. This supports first-load file I/O
as a material contributor, without attributing all non-CPU time to I/O waits.
ELF dependencies confirm `libEGL_mesa` → `libgallium` → `libLLVM`; per-library
fault attribution is not measured. Primary clients still started about 3.3 s
after labwc. No production startup improvement or physical cold-boot/first-frame
measurement is claimed. A future private Mesa experiment must match Noble's
ABI; the retained Trixie package is not a compatible replacement by assumption.

The original compositor configuration is restored byte-for-byte. Temporary
probe libraries, wrappers and rollback units are removed; the ordinary session
has no probe mapping or LD_PRELOAD, DSI-1 retains transform 270/scale 1.5,
Wi-Fi is connected, dpkg verification and failed-unit output are empty, and
firmware protection flags are zero. The user then requested uConsole shutdown
before host shutdown; the poweroff command closed its SSH connection. Retained
host evidence is `~/.cache/uconsole-desktop-20261018/egl-profile-*.json`;
[call timings and methodology](../knowledge/desktop/labwc-shell-comparison.md#首次-mesa-加载的调用计时)
record the observational limits. All 313 baseline host tests pass.

### Private Noble Mesa desktop trial

On 2026-10-22 a native Ubuntu 24.04 ARM64 build of Ubuntu Mesa
`25.2.8-0ubuntu0.24.04.4` produced the optional `uconsole-mesa-noble` package.
It keeps V3D/VC4 EGL/GBM/Gallium in a private prefix without LLVM, global GLVND
metadata or changes to image defaults. The fixed source archives, seven Ubuntu
patches, cached Meson/compiler inputs and runtime ELF closures are checked.
The final 3,780,576-byte package has SHA-256
`baf9e90d212a38346612063530f57b783aafe6966c63a89d04d8edaa86a7c337`
and is retained with its original receipt in Git LFS. Remote LFS publication
and download remain unverified; no push was performed.

Device observations: selecting private Mesa only for labwc advanced Noctalia's
start but transferred the system-library loading cost to the shell; its first
panel buffer remained about 13.49 s after kernel start. A subsequent A/B/A
comparison used the same probes and direct Noctalia ELF launch in all runs,
with both persistent shader caches populated:

| Mesa for labwc and Noctalia | First panel-buffer FBO | labwc Mesa `dlopen` |
| --- | ---: | ---: |
| System A | 13.268 s | 1.625 s |
| Private B | 11.656 s | 0.043 s |
| System A | 13.401 s | 1.618 s |

Derived conclusion: private libraries advance this software buffer boundary
by 1.61–1.74 s in the controlled sequence. This is one A/B/A sequence of
system-reboot first sessions, not a physical cold-start or visible-frame
measurement. First use of a new Mesa shader cache remains a separate boundary;
the earlier first-use warm session logged a 1494.2 ms queued render.

The live CM4 now explicitly selects private Mesa for both desktop processes.
The packaged cleanup helper removes the shell's loader overrides before
application code runs while glibc retains its initial lookup path. An actual
Noctalia launcher entry started a child with no graphics/loader overrides;
that child queried EGL successfully and mapped system Mesa/Gallium/LLVM.
The production compositor and shell mapped private Mesa without LLVM, and a
composed screenshot retained the wallpaper and top bar at transform 270/scale
1.5. On the private V3D renderer, all 241 existing rectangle-shader comparisons
remained byte-identical to their original shader oracle. All 336 host tests pass
(one optional test skipped); local LFS fsck passes.

The final uninstrumented reboot, boot ID
`00000000-0000-4000-8000-000000000003`, retained both private Mesa selections,
the cleanup helper and the composed desktop. Temporary profiling libraries,
wrappers and rollback units were removed. Package verification and failed-unit
output are empty, and firmware protection flags are zero. This final runtime
check does not add another instrumented timing sample.

CM5, other GPU families, physical interaction after this change and fresh-image
first-use behavior are not established. The exported SD image is unchanged.
See the [private Mesa methodology](../knowledge/desktop/private-mesa-startup.md)
for sample identities, isolation checks and measurement limits.

### Earlier CM4 desktop startup

On 2026-10-23, changing only firmware `initial_turbo` from 0 to 10 advanced
labwc's process start from 7.66 to 5.85 s after kernel entry in a controlled
reboot sequence. The systemd-reported kernel stage, which includes initramfs
here, fell from 3.339 to 1.842 s. The stable 2200 MHz/OV6 limits remain the same;
CPUFreq retains dynamic frequency control. Initcall tracing locates a large
early tracing-subsystem wait; blacklisting its first waiter merely moves the
wait to another initcall, so no kernel blacklist or tracing removal was kept.

The final uninstrumented boot `00000000-0000-4000-8000-000000000001` reports
landscape transform 270, scale 1.5, no failed units and zero protection flags
at uptime 21.22 s. SSH and ARP subsequently stopped responding, while the
host's gateway remained reachable. On 2026-10-24 the user identified an empty
battery as the cause. This is a user report; no persistent device journal is
available to verify that shutdown. The resumed boot
`00000000-0000-4000-8000-000000000007` retains the same setting: 1.865 s in
the systemd kernel stage, labwc at 5.99 s and Noctalia at 7.73 s. It remains
online beyond ten minutes while charging, with zero protection flags and no
failed units. These process timings do not measure a physical first frame.

A scoped passthrough wrapper separately measured 0.82 s in Ubuntu
NetworkManager's synchronous Netplan generation. On 2026-10-24 native matched
Ubuntu `1.46.0-1ubuntu2.8` builds isolate the full Netplan integration feature:
disabled/enabled/disabled service startup takes 0.667/1.428/0.668 s. Their
Noctalia process starts are 6.90/7.57/6.87 s, and labwc starts are
5.32/5.95/5.27 s. A following stock boot gives 7.67 s and 6.02 s respectively.
The paired builds use the same compiler and normalized options apart from
Netplan; all nine plugin text sections match. The enabled control also fixes
Ubuntu's missing Meson `WITH_NETPLAN` configuration-header placeholder.

Root-owned binaries and plugins were bound only into the service namespace;
executable hashes and all bindings were checked during sampling. Stock executable
and plugin hashes, network profiles and network/session ordering remain unchanged. Wi-Fi,
routing, composed screenshots and protection checks succeeded. All samples
shared the pre-existing DNS failure described below. A stale first sample
without the trial drop-in was excluded rather than attributed to the candidate.

Disabling Netplan also changes YAML save/delete integration, so these builds
remain experiments. The narrower source-only dispatch optimization below keeps
that integration. The stock daemon is restored; trial bindings, timers and root
staging directories are removed. Private build inputs and receipts are retained for reproduction.
The exported SD image is unchanged. See the
[early desktop investigation](../knowledge/boot/cm4-early-desktop.md).

The CM4 board configuration now retains `initial_turbo=10` for future builds.
It does not raise the image's frequency or voltage limits. This policy is
specific to the measured CM4 firmware path; CM5 and other modules need their
own measurements. The exported image is unchanged.

### Ubuntu Netplan invocation

On 2026-10-24 a source-only service PATH dispatcher removes unrelated Python
command imports from the exact plain `netplan generate` call. It keeps the
stock NetworkManager daemon and all nine modules, native generation, udev and
systemd reloads, YAML save/delete integration and network/session ordering.
Other arguments, custom environments and changed audited CLI sources delegate
to the original system CLI. The Ubuntu backend snapshots the helper and service
drop-in; image inspection checks their bytes and the helper's executable mode.
There is no custom daemon package or additional native build input.

Matched stock/fast/stock/fast reboots show NetworkManager service startup of
1.416/1.129/1.419/1.148 s. Labwc starts at 5.91/5.65/5.92/5.67 s and Noctalia
at 7.59/7.31/7.61/7.30 s after kernel entry. All four have identical network
configuration hashes, successful Wi-Fi/routing/DNS checks, composed screenshots,
no failed units and zero firmware protection flags while charging. Runtime
child observation independently confirms the NM-scoped entry executes. These
process observations do not establish physical first-frame or cold-boot times.

An inactive Ethernet fixture passes create/edit/reload/delete checks. A second
fixture loads an external YAML edit made while NM is stopped and retains the
new name and MTU across reboot `00000000-0000-4000-8000-000000000035`.
Deletion, generation and reload do not revive it. Original connection bytes
and all ten original ELF hashes match the pre-trial state. Temporary fixtures,
rollback units/timer and root staging are removed; the source-only optimization
is retained on the running device. Wi-Fi, DNS and protection checks remain clean.

All 346 host tests pass with one optional skip, and patch checks pass. The
exported SD image has not been regenerated. See the
[generation contract and evidence](../knowledge/network/ubuntu-netplan-generate.md)
for source guards, error semantics, profile lifecycle validation and sample IDs.

### Ubuntu Base DNS ownership

On 2026-10-24 the stock CM4 NetworkManager had working Wi-Fi, routing and SSH,
but DNS queries failed: Ubuntu's vendor configuration selected systemd-resolved,
which this base system does not install. Its generated resolv.conf pointed to
the absent local stub. The Ubuntu backend now installs a local `dns=default`
override while retaining the vendor file and existing runtime resolv.conf link.

The same override restores DHCP-provided DNS on the live stock daemon. Boot
`00000000-0000-4000-8000-000000000022` confirms persistent resolution after a
normal reboot, with the composed desktop, no failed units and zero protection
flags. All 337 host tests pass, with one optional skip. The exported image is
unchanged. See [DNS ownership](../knowledge/network/ubuntu-networkmanager-dns.md).

## Ubuntu Base 24.04.5 CM4 image

On 2026-10-18 the native `ubuntu-noble-base` backend exported
`image/deploy/uconsole-cm4-ubuntu-noble-base-none-arm64.img`. It uses the official
Ubuntu Base 24.04.5 ARM64 archive, pinned against Canonical's verified signed
checksum list, and signed Noble package repositories. The image has no desktop;
it contains a preset unprivileged account, SSH, NetworkManager, first-boot root
expansion and the CM4 early-display module list. Credentials are supplied through
the ignored Ubuntu-specific TOML file. The 1,611,661,312-byte image has FAT32 and
ext4 partitions; its adjacent receipt and package list record the exact inputs
and 213 installed packages.

This userspace trial explicitly retains the previously booted
`6.18.57-v8-uconsole` kernel through a pinned historical report. Its original
patch/configuration hashes remain recorded; they differ from current sources,
and its configuration predates the current `CONFIG_CHARGER_AXP20X` requirement.
The current strict artifact validator remains unchanged. The original driver
bytes are retained while the matching current initramfs integration hook is
installed before package configuration. No kernel rebuild was needed.

Offline observations: native export completed its filesystem checks, and native
read-only image inspection passed for the selected kernel, DTBs/overlays,
modules, initramfs, partition references, account and Ubuntu service contracts.
All 212 host tests and the Agent handoff command pass. A separate content check
confirms that all 74 compressed firmware paths, selecting 22 leaves, have plain
counterparts with matching decompressed bytes; the CM4 Wi-Fi aliases resolve
correctly within the target rootfs. Package audit and sudo configuration checks
are clean; host keys and machine-id are unset for first boot. The receipt's
builder/exporter hashes match the source used for that export. The image keeps
initramfs enabled and does not install pi-gen's direct-root policy.

After the user inserted the SD card for deployment on 2026-10-18, the verified
image was written to the 63,281,561,600-byte removable USB-reader card, identified
by reader serial `000000001536` and disk sequence 9. Full readback of all
1,611,661,312 written bytes matches the image SHA-256. Both read-only filesystem
checks and the shared kernel/initramfs/partition/account inspection pass on the
card itself. All mounts were removed and `udisksctl power-off` succeeded. The
local deployment receipt is
`image/deploy/2026-10-18-ubuntu-cm4-sd-flash.json`; write and check logs are in
`~/.cache/uconsole-ubuntu-cm4-{sd-flash,card-check}-20261018.log`.

The user subsequently reports successful Ubuntu startup with an upside-down
landscape console. SSH inspection of boot
`00000000-0000-4000-8000-000000000013` confirms Ubuntu 24.04.5 LTS on CM4 Rev 1.1,
`6.18.57-v8-uconsole`, Wi-Fi/SSH at `<HOST_05>` and ext4 root on
`/dev/mmcblk0p2`. Root filesystem size is 61,717,897,216 bytes; the expansion
marker exists and its service result is success. Machine-id and three SSH
public host keys have been generated. No failed systemd units are reported.
NTP is not synchronized in this capture; device wall-clock timestamps must not
be treated as a trusted time reference.

The booted command line and live console both used rotation 3. The live setting
was changed through `rotate_all` to 1, turning the console 180 degrees relative
to its previous orientation; a readback of `fbcon/rotate` confirms 1.
`/boot/firmware/cmdline.txt` now contains `fbcon=rotate:1`, with the original
file and runtime state backed up under `/var/backups/uconsole/console-rotation-*`.
The initial live adjustment needed no reboot. The user subsequently confirms
correct physical orientation; stock recovery boot
`00000000-0000-4000-8000-000000000029` also has `fbcon=rotate:1` in the booted
command line and live console readback 1. Future Ubuntu builds use rotation 1;
the previously exported image has not been regenerated. Other peripherals,
cold-start timing and CM5 Ubuntu build/hardware behavior remain unverified. See the Ubuntu build guide（工程源码：`image/ubuntu-base/README.md`）
and [rootfs integration knowledge](../knowledge/rootfs/ubuntu-base-integration.md).

## CM4 CPU overclock validation

On 2026-10-18 the Ubuntu CM4 completed finite CPU and memory trials at its
stock 1500 MHz, 2000 MHz and 2200 MHz. The two higher settings use
`over_voltage=6`, with `force_turbo=0` and `initial_turbo=0`; firmware thermal
protection remains enabled. Each stage includes verified CPU/VM workloads
and one complete memtester iteration with 256 MiB actually allocated and
locked. The 2200 MHz stage additionally completes 600 seconds of combined
four-worker CPU and 1 GiB VM load. Exit statuses and verification logs pass;
all sampled firmware protection flags are zero, and the captured kernel
logs contain no matched faults. The 2200 MHz stage reaches 61.835 degrees C.
Its single 20-second int64 result is 5966.24 bogo ops/s versus 4058.69 at
stock, a 47.0% increase for that method rather than a system-wide benchmark.

At the user's request, this card's normal `config.txt` now retains 2200 MHz
and OV6. Normal boot `00000000-0000-4000-8000-000000000031` confirms
`tryboot=0`, a 600-2200 MHz policy and the normal `ondemand` governor.
A subsequent 20-second verified four-worker int64 run passes, with observed
loaded clocks reaching approximately 2.20055 GHz and zero protection flags.
Wi-Fi/SSH and rotation 1 remain available; no failed systemd units are
reported. Temporary tryboot files and the experiment's runtime-watchdog
configuration have been removed; the original normal configuration remains
backed up. These are device-local settings, not generic image defaults.

The trial and final boot use the unchanged 2024-02-29 start firmware
`f4e2138c2adc8f3a92a3a65939e458f11d7298ba` and 2023-01-11 EEPROM
`8ba17717fbcedd4c3b6d4bce7e50c7af4155cba9`; no firmware upgrade was performed.
Finite workload success does not establish long-term stability or full
physical-memory coverage. Firmware changes require renewed validation.
The [overclock knowledge note](../knowledge/hardware/cm4-overclock-testing.md)
describes the oracles and the tested runtime-recovery boundary. Full device
records are retained in `~/.cache/uconsole-oc-20261018/device-evidence.tar.gz`
and `/var/lib/uconsole-overclock/20261018/` on the CM4.

## CM4 firmware update

On 2026-10-18, following explicit user authorization, the CM4's active
VideoCore firmware pair was updated to official release `1.20260915`,
repository commit `6f0881cba8bea8ec24956a5718714730e9936ec5`.
Boot `00000000-0000-4000-8000-000000000017` reports the running firmware as
2026-09-14 14:49:30, version
`a86983925695a7e63166327d7c002d64040ed31d`.

A separate official CM4 self-update then installed the BCM2711 `default`
EEPROM image dated 2026-09-23. Boot
`00000000-0000-4000-8000-000000000014` confirms version
`456428dcbedc313131e5dc8be59a80f1f55cd9ba`, timestamp `1790164934`.
The updater and images are fixed to rpi-eeprom commit
`7ed1cc2b210053b5d6537b2beb542663cab7e10b`. Its self-update branch was
explicitly selected for CM4; no SPI overlay or flashrom operation was used.
The complete live EEPROM configuration, including `BOOT_ORDER=0xf25641`,
is unchanged. The staged update image and signature were verified and then
removed after the running version was confirmed; no `recovery.bin` was
installed on the boot partition.

The project `6.18.57-v8-uconsole` kernel, selected DTBs, CM4 overlay,
initramfs, normal config and command line retain their recorded hashes.
Both boots retain Wi-Fi/SSH, `ondemand`, the 600-2200 MHz policy, console
rotation 1 and `vc4drmfb`, with zero protection flags and no failed units.
These software observations do not establish physical display output.
The exported Ubuntu image has not been regenerated with these firmware files.

Old startup files and EEPROM configuration are backed up on the device and
in `~/.cache/uconsole-fw-20261018/device-before.tar.gz`. The EEPROM fallback
image combines the official matching 2023 image with the saved live
configuration; it is a reconstructed image, not a raw SPI readback.
USB recovery has not been exercised. The
[firmware update note](../knowledge/boot/cm4-firmware-update.md) records inputs,
upstream changes, method and verification boundaries.

The new firmware combination also passes a separate 2200 MHz / OV6 regression:
four-core int64 for 20 s, CPU verification for 120 s, 1 GiB VM verification for
60 s, one fully locked 256 MiB memtester iteration, and 600 s of mixed CPU/VM
load. The complete 1014.136 s run records 982 telemetry samples, a peak of
62.809 °C and zero protection flags. Command status, verification logs and
memory coverage satisfy the documented oracles; no BUG, Oops, panic, OOM or
storage I/O errors were found. Boot-time Wi-Fi chanspec `-52` messages remain
in the captured log. The int64 observation is 5968.04 bogo ops/s versus
5966.24 on the old firmware at the same frequency; this single comparison
does not establish a performance improvement.

After removing the temporary runtime-watchdog configuration, ordinary boot
`00000000-0000-4000-8000-000000000005` confirms both firmware versions,
unchanged EEPROM configuration and project boot hashes, `tryboot=0`,
`ondemand`, 600-2200 MHz, console rotation 1, zero protection flags and no
failed units. PID 1 reports `RuntimeWatchdogUSec=0`, the hardware watchdog is
inactive, and update staging files are absent. The preset daily frequency
remains 2200 MHz / OV6. Complete upgrade, regression and final-boot evidence
is retained in `~/.cache/uconsole-fw-20261018/device-evidence.tar.gz` and
`/var/lib/uconsole-firmware/20261018/` on the CM4. These finite workloads do
not establish long-term stability or physical display output.

## Fresh CM4 SD desktop follow-up

On 2026-10-18 the user booted the newly flashed Raspberry Pi SD card in the
uConsole CM4, then reported a portrait desktop, an empty battery icon and
approximately two minutes from the power button to the desktop. Initial
inspection confirms `6.18.57-v8-uconsole`, Wi-Fi/SSH and a composed portrait
desktop. UPower reports a present battery at 62%; the stock `wfplug-batt` 1.11
backend instead derives its percentage from cumulative charge/energy and
ignores the AXP20x driver's valid `capacity` attribute. The focused adaptation
uses that percentage and preserves unknown/absent-battery semantics and
power-supply type/scope classification. Its 24 backend and multi-supply
selection checks use synthetic sysfs data.

Package `1.11+uconsole1` and landscape transform 270 survive the coordinated
warm reboot `00000000-0000-4000-8000-000000000026`. Reviewed screenshots show a
1280x720 desktop with wallpaper, panel, trash icon and a populated battery
indicator. The persistence capture reports battery `present=1`, `capacity=54`
and `status=Charging`; these are driver reports, not cell calibration or a
measurement of charging current. A matching `axp20x_ac_power` module loads
automatically and the AC input reports `present=1`, `online=1`, with no failed
systemd units in that capture. Future CM4/CM5 configurations enable
`CONFIG_CHARGER_AXP20X`; the USB-input option `CONFIG_AXP20X_POWER` was already
enabled. CM5 hardware validation of this integration remains outstanding.

The first boot's systemd startup total is 28.167 s; an unchanged warm reboot
reports 20.833 s. These exclude the power-button boundary and do not measure
a useful desktop frame. In that second boot, a successful PNG capture at
37.604 s after the reboot command is entirely black despite live desktop
processes; a later capture at approximately 100 s of device uptime contains
the desktop. The integration reboot reports a 20.328 s systemd total. Its
successive captures show an all-black frame ending at kernel uptime 19.63 s,
wallpaper only at 21.32 s, and the first reviewed complete desktop at 23.71 s
(41.206 s after the reboot command). Screencopy observes composed content;
physical cold-start visibility and the cause of the earlier black capture
remain unresolved. At that initial integration checkpoint no boot optimization
or speedup was established; initial resize/key-generation/cache work differs
from later steady-state startup.

The initial desktop session stopped after the battery persistence check. Its
[desktop, battery and startup record](../knowledge/desktop/cm4-postflash-desktop.md) remains
historical evidence; the subsequently resumed startup work is recorded below.

## Measured CM4 startup follow-up

The resumed 2026-10-18 work uses one warm control, two focused trials and a
final persistence boot on the same CM4 SD. Honoring the existing
`ENABLE_CLOUD_INIT=0` policy with the package's supported marker prevents
fallback cloud-init stages from delaying
sysinit/LightDM. Compared with control `fb0a0eef`, trial `74261a2d` reduces the
systemd total from 20.467 to 17.890 s and the first manually reviewed complete
desktop from kernel uptime 23.53 to 21.74 s. Wi-Fi/SSH, landscape and battery
reporting remain available, with no failed units in those captures.

Gzip trial `e122c687` keeps the decompressed kernel bytes and initramfs fixed.
It reduces the firmware-loaded file from 29,317,632 to 10,616,104 bytes and a
matched command-line-read-to-ARM-start interval by 1.613582 s, including firmware
decompression. Its systemd total is 18.140 s; a complete desktop is captured at
uptime 21.99 s, or 37.307 s after the reboot command. Those clock domains are
separate, and a single warm comparison is not a statistical or physical cold
startup measurement. The user's roughly two-minute delay remains unresolved.

The standard `uconsole/kernel8.img` gzip path and original boot configuration
pass persistence boot `00000000-0000-4000-8000-000000000032`. Its systemd total
is 18.060 s; the first reviewed complete desktop capture ends at uptime 20.34 s,
or host elapsed 37.288 s, after an all-black sample ending at 18.83 s. These
remain sampled composed-frame bounds. The marker, activated Wi-Fi, unchanged
network profile, landscape transform 270 and battery plugin persist; the
capture has zero failed units and `throttled=0x0`. The battery driver reports
34% and discharging. Retained raw kernel/config backups provide rollback.
Existing exported images are unchanged by the live trials, and CM5 gzip
hardware validation remains outstanding. See the
[boot-time record](../knowledge/boot/cm4-boot-time.md) for boot identities, retained
evidence and remaining limits.

## CM4 direct-root and NetworkManager follow-up

Later controlled trials on the same SD preserve the compressed kernel and
cloud-init policy while removing initramfs, then changing only the
NetworkManager package family. Direct root moves firmware ARM entry from
11.909034 s to 7.921233 s and reaches a complete desktop at kernel uptime
19.50 s. Debian's `1.52.1-1` build removes the Raspberry Pi `+rpt4` build's
synchronous Netplan/systemd reload: NetworkManager falls from 3.414 to 0.841 s,
systemd total from 17.329 to 14.624 s, and a reviewed complete desktop is
captured at uptime 17.43 s, host reboot elapsed 28.640 s. The Wi-Fi keyfile is
unchanged. Signed offline package inputs and a timed rollback protected the
trial; no broad service removal was used.

The project now keeps first-boot initramfs in a CM4 managed firmware include.
A 60-second timer checks the known native MMC/ext4 layout and project kernel
before enabling direct root for subsequent boots. Resize/first-run arguments
and unsupported layouts retain initramfs; updates restore it and block
same-boot reapproval. Image inspection rejects a missing, old, wrong-release or
non-executable update hook. Source/isolated failure checks total 104 passing tests.
The live include, update/reset path and later delayed reapproval were verified.
A following default boot `00000000-0000-4000-8000-000000000024` has no initrd,
successful root fsck, firmware ARM entry 7.964964 s, systemd total 14.915 s and
complete-desktop capture at uptime 18.76 s, host elapsed 29.976 s. It retains
Wi-Fi, landscape and battery reporting, no failed units and `throttled=0x0`.
Trial files/timers have been removed; normal defaults now select the verified
path. Retained initrd and original package/configuration backups permit rollback.

These remain warm reboot observations, including sampling and shutdown in
host elapsed. The user's five-second CM5 reference has no equivalent recorded
desktop boundary; saved CM5 logs do show direct-root HS400 eMMC, whereas this
CM4 uses SD. Fresh exported-image resize/PARTUUID/policy validation and physical
cold-start timing remain outstanding. Existing image artifacts are unchanged.
Mesa/V3D is hardware accelerated. The subsequent compositor, early-display and
PCIe trials are recorded below; the [boot-time note](../knowledge/boot/cm4-boot-time.md)
retains the individual controls and measurement boundaries.

## CM4 early display and compositor follow-up

A controlled static-module trial loads the unchanged display chain in early
root userspace: HDMI DDC (`i2c-brcmstb`), OCP8178, CWU50 and VC4. Raw `fb0`
registration moves from 7.918 to 5.082 s after kernel entry, while systemd total
remains approximately 15 s. This establishes earlier framebuffer readiness;
it does not establish an earlier visible image or desktop. The CM4 image stage
now installs the exact load list independently of its direct-root policy.
Neither a minimal display initramfs nor a built-in display kernel was produced.
See [early-display evidence](../knowledge/display/cm4-early-display.md) for the dependency
closure and the separate initramfs/built-in candidates.

A native private V3D/VC4 Mesa trial removes LLVM from labwc's dependency chain
while preserving distribution libraries for ordinary clients and Xwayland.
Its controlled pair reduces sampled labwc reads from 87,969,792 to 35,299,328
bytes and the first reviewed complete-desktop capture from uptime 18.93 to
17.62 s; systemd total is unchanged. One pair does not establish repeatability.
The optional `uconsole-mesa-compositor` package is now installed and configured
under `/usr/lib/uconsole/mesa-compositor`, and LightDM selects its independent
session. A runtime check confirms private compositor mappings, V3D render
jobs, system-only client/Xwayland libraries and an actual X11/GLX 1.4 query.
The native offline builder, fixed inputs and receipt validation are project
owned; normal image builds neither require nor install this package.

The official CM4 adapter leaves PCIe unconnected. The uConsole CM4 boot config
now uses `dtparam=pcie=off`, removing the approximately 345 ms dead-link wait
while preserving USB enumeration; the generic carrier config remains separate.
An independent initcall diagnostic attributes an early approximately 0.59 s
pause to `init_kprobe_trace`. Asynchronous tracing mutex contention is a source
hypothesis, not a proven CPU/self-test cost; no tracing option was disabled.
See [initcall evidence](../knowledge/boot/cm4-initcall-profile.md).

Normal boot `00000000-0000-4000-8000-000000000034` verifies the combined retained
configuration: direct SD/ext4 root, gzip kernel, Debian NetworkManager, the
exact display module list, disabled unused PCIe and the stable-prefix private
Mesa session. Raw panel probe is at 4.337 s and `fb0` at 4.727 s; systemd total
is 15.000 s. Labwc starts at 10.19 s, panel/file-manager processes at
12.73/12.79 s, and the first reviewed complete landscape desktop spans uptime
16.98–17.33 s (host reboot elapsed 27.786–28.801 s). The firmware ARM-start
counter anchor is 7.978505 s; its clock is not calibrated to the power button.
These combined results are a persistence observation, not an isolated speed
comparison or a five-second desktop measurement.

Wi-Fi/SSH and its unchanged network keyfile, transform 270, populated battery
indicator and composite USB enumeration are retained. No failed systemd units,
throttling, captured DSI timeout or kernel panic/Oops are reported. The battery
driver reports present, 99% and Charging; this does not measure cell calibration
or charging current. Package integrity and `dpkg --audit` are clean. Diagnostic
command-line options, experiment rollback timers and obsolete cache-session
launchers have been removed with backups; the production direct-root timer
remains enabled. The full recovery initramfs and original package/configuration
backups remain available. All 119 host tests and four patch applications pass.
A later builder-provenance repack has identical runtime payload; the boot
observations remain tied to the already installed package. Existing exported
images have not been rebuilt or flashed; physical cold-start timing, fresh-image
policy validation and CM5 integration remain outstanding.

## Existing-device observations

The initial read-only inspection of the CM5 community reference image found:

- Debian 12, kernel `6.12.94-v8-16k+`, CM5 Rev 1.0, installed `akrex-kernel` package.
- `/dev/mmcblk0` is 29.1 GiB eMMC, with hardware boot/RPMB partitions; `/` is mmcblk0p2 and `/boot/firmware` is mmcblk0p1. mmc1 is the Wi-Fi SDIO function.
- The inspection record reports the CWU50 driver selecting its old-panel branch, OCP8178 backlight and AXP223 PMIC. The exact physical panel part number and a successful DCS identification read were not established; `cw,cwu50` is a shared compatible string.
- Keyboard/consumer/joystick/mouse are standard composite USB HID interfaces, VID:PID `1eaf:0024`.
- Existing audio services read GPIO10; low enables the speaker through GPIO11 high. Shutdown drives GPIO11 low.
- The active CM5 boot section enables UART0. SPI settings occur in inactive Pi3/Pi4 sections; the project does not copy them into its CM5 section.

No device reboot, boot configuration change or eMMC write was performed.

The user confirms that both the official uConsole image and the ak-rex
community image display correctly on the uConsole screen. Treat those boots
as the known-good display baseline. The earlier black-screen results belong
to the project CM4 trials before the recovery and fresh-SD follow-up above.

## Separate CM4 carrier observations

Read-only inspection of the user-provided CM4 on a Waveshare carrier found CM4 Rev 1.1 (`c03141`), 4 GB RAM, a native 29.1 GiB SD card, Debian 13.4 and running kernel `6.12.75+rpt-rpi-v8` with 4 KB pages. Ethernet is up; both HDMI connectors are disconnected. Existing boot settings include SoC XHCI host mode and a 2 GHz CPU setting. The exact Waveshare carrier model is unknown. No boot settings, packages or EEPROM were changed, and no reboot was performed. See [CM4 carrier baseline](../knowledge/hardware/cm4-waveshare-baseline.md).

This records the original distribution system. The project 6.18 kernel has since booted from the separately prepared carrier test SD, as recorded below. The uConsole CM4 image's display/PMIC overlay and audio GPIO service must not be applied to this unrelated carrier.

## Source findings and derived decisions

The official CM5 datasheet §2.7 says the connector's SD interface exists only on CM5Lite; eMMC variants do not expose those signals. This explains why inserting a microSD into this eMMC unit produces no card-detect log. Formatting or a Linux rescan cannot restore absent wiring. See [storage note](../knowledge/storage/cm5-storage.md).

The comparison kernel source is `ak-rex/rpi-linux` commit `874351b52621220e99c9a2d3849219fcce39c37f`. Its relationship to the installed binary is not proven. The port retains panel initialization tables, backlight protocol/levels, display wiring and audio GPIO polarity. Standard baseline AXP223 MFD/regulator/ADC/battery/AC support is reused. Copied battery capacity/current settings and extra PMIC register writes are deferred until their necessity and cell specification are known. See [port evidence](../knowledge/kernel/cm5-kernel-port.md).

The published ClockworkPi CM4 kernel package is built from source commit
`03436f4117533693d608b10eb4b3965987230b68`. Its old-panel initialization
table matches the project table exactly. Its new-panel function executes 189
writes, including TE at the end; the project's 188-entry table matches those
configuration writes and handles TE separately. Twenty-three additional
candidate writes in the official source are comments, not active commands.
The project now preserves the official new-panel power/display command order
and delays before its shared final transition. Earlier forced new-panel trials
did not exercise that complete sequence.
See [CM4 display investigation](../knowledge/boot/cm4-uconsole-boot-debug.md).

## Verification record

- 61 host unit tests passed (including CM4/CM5 gzip payload packaging and hashes, desktop battery package validation/export ordering, preset-account requirements, locked accounts, SSH/wizard checks, interactive configuration persistence/defaults/cancellation, prerequisite failure handling, build ordering, OS/desktop compatibility, CM4/CM5 artifact isolation, a synthetic non-Pi Rockchip target, the official CM4 panel sequence and single-module target/configuration-cache validation): manifest/submodule invariants, cumulative patch application with unchanged source/index, invalid checkout detection, artifact freshness/path confinement/checksum validation.
- Kernel build container: Ubuntu 24.04, GCC 13.3 aarch64 cross compiler; built successfully.
- Image build container: Debian Trixie; built successfully. ARM64 execution inside an empty chroot passed via host binfmt/QEMU.
- CWU50/OCP8178 objects and all device trees compiled. The generated board overlay, vc4-kms-v3d-pi5, audremap-pi5 and dwc2 applied successfully to both compiled CM5 eMMC base DTBs (CM5IO and CM4IO). This uses fdtoverlay without firmware parameter processing. audremap maps to audremap-pi5 on BCM2712 through overlay_map.dtb, which is included in the package.
- Both upstream checkouts match their pins and remain clean. All four catalog patches apply cumulatively to the complete Linux source. CM4 overlay/display/audio/USB application also passed on the BCM2711 CM4 base DTB (using the firmware-mapped vc4-kms-v3d-pi4 overlay). Full Image/modules/DTB compilation and ARM64 package creation passed for CM5 (`6.18.57-v8-16k-uconsole`) and CM4 (`6.18.57-v8-uconsole`). Both new drivers have matching module vermagic on each target. Both current-artifact JSON validators pass. Desktop image export and read-only artifact inspection passed for both targets.
- The CWU50 panel object compiled for the current CM4 patch. The resumed all-modules CM4 package build was stopped during unrelated module compilation to keep panel iteration focused; no new kernel package was produced. The recorded CM4 package below predates the current panel patch and its artifact validator now rejects it as stale. Use `make kernel-container-module` for this driver iteration and run the full build before preparing another deployable package.

These checks do not establish hardware functionality.

## CM5 kernel artifact

- Package: `kernel/build/artifacts/882870bea1e8b2f8/uconsole-kernel_6.18.57-v8-16k-uconsole_arm64.deb` (about 31 MiB).
- Package SHA-256: `4989bbdd3486e8091c5f599ff482ccc02ca656bcddd7316797ed1bd2e1255b63`.
- Staged Image SHA-256: `e0b8a1729418ee28b64eeec9b2ea9c797c712c17fbbf0bada2035b36db207462`.
- Full source/configuration/patch metadata: `kernel/build/current.json`; validate through `make -f Makefile.agent kernel-artifact`.

## CM4 kernel artifact

- Package: `kernel/build/artifacts/uconsole-cm4/17e83bbc8a6a21cc/uconsole-kernel_6.18.57-v8-uconsole_arm64.deb` (about 31 MiB).
- Package SHA-256: `fe7c01d43b047f0ac0f98e7a22739a3b01b499d93b9cbe8394270fa940cac022`.
- Staged Image SHA-256: `9bff54d79b1b2277d2ebe14067101174daec7bae1c936ff9d0d11b557b8b9875`.
- Full source/configuration/patch/board metadata: `kernel/build/current-uconsole-cm4.json`; validate through `make -f Makefile.agent kernel-artifact BOARD=uconsole-cm4`.

## CM5 desktop image artifact

- Exported 2026-10-17 from official pi-gen stages 0–4 plus the project stage. The completed stage4 rootfs was reused after checking identical upstream stage sources and matching base/account/locale settings; only the project stage and final export were run in the final work directory.
- Compressed image: `image/deploy/image_2026-10-17-uconsole-cm5-raspios-trixie-desktop-rpi-desktop-arm64-uconsole.img.xz` (1,714,910,104 bytes).
- Compressed SHA-256: `7909c1fedc4f2589967b0c70e8baa6387afd6d91578cfdb834512c215f0ba116`; adjacent `.sha256` file supplied.
- Raw image: `image/work/b0e75b4f9c33c324/export-image/2026-10-17-uconsole-cm5-raspios-trixie-desktop-rpi-desktop-arm64-uconsole.img` (6,819,938,304 bytes).
- Raw SHA-256: `422608dd5583de60c65fe963ed05dd2ecea586bd738e455f72ce307ce8912966`.
- `make image-check IMAGE=<raw-image>` passed: ten selected kernel/config/DTB/overlay files, both new modules, final initramfs copy and release, audio payload/enabled service, overlay prefix marker, actual root PARTUUID and absence of the build apt proxy file. This uses read-only loop/mounts, ext4 without journal replay, and no container network. Streaming decompression of the final xz artifact produced exactly the inspected raw-image SHA-256.
- The firmware config uses `[cm5]`, `os_prefix=uconsole/` and `cmdline=/cmdline.txt`. The inspector checks the shared command line against the root PARTUUID finalized by pi-gen; prefix-local overlays include their required README and overlay map. A leading slash alone does not establish prefix bypass; the later CM4 lookup/fallback evidence is recorded in the [boot-time note](../knowledge/boot/cm4-boot-time.md).
- The temporary apt proxy initially could not tunnel HTTPS changelog downloads; some apt-listchanges changelog cache entries could not be populated. Package installation and the final export completed successfully; package archives were signature checked. The distribution firmware hook intentionally skips the custom kernel release, whose private initramfs is synchronized by the project hook and verified above.

## CM4 desktop image artifact

- Exported 2026-10-17 with the CM4 package, board configuration and shared Pi audio payload. The completed official stage4 rootfs was reused after checking identical upstream stage sources and matching base/account/locale settings, followed by the project stage and final export.
- Compressed image: `image/deploy/image_2026-10-17-uconsole-cm4-raspios-trixie-desktop-rpi-desktop-arm64-uconsole.img.xz` (1,714,751,392 bytes).
- Compressed SHA-256: `26e7c446ce9dfa2aaeab1c46cc7c73825b457b8a355a46b52d9080b5a28044f5`; adjacent `.sha256` file supplied.
- Raw image: `image/work/4e84cf98f8836c97/export-image/2026-10-17-uconsole-cm4-raspios-trixie-desktop-rpi-desktop-arm64-uconsole.img` (6,819,938,304 bytes).
- Raw SHA-256: `243d8558eccbcad12d0f9663b53712870cc35ffad16d5b91e9e79254296a3eae`.
- `make image-check BOARD=uconsole-cm4 IMAGE=<raw-image>` passed: nine selected input files, both new modules, final initramfs copy/release, audio payload/enabled service, overlay marker, root PARTUUID and absence of the build apt proxy file. The board config selects `[cm4]` and `kernel8.img`; firmware uses the included overlay map for BCM2711 display selection.
- Streaming decompression of the final xz artifact produced exactly the inspected raw-image SHA-256.
- These are uConsole CM4 image checks. They do not establish behavior on a different CM4 carrier.

## Headless account policy

New builds preconfigure the initial `pi` account, enable SSH and disable the
first-boot account wizard. A password must be supplied through ignored local
configuration for uConsole targets; image preparation fails before replacing its snapshot if the
password is missing. The historical CM4/CM5 uConsole image files above retain
their original wizard behavior. The separate CM4 carrier test image below has
an account configured offline and needs no display for setup. The independent
CM5 IO Board development target uses the requested public `pi` / `1` default,
with optional overrides in its own ignored local TOML.

## CM4 carrier SD deployment

On 2026-10-17 the user explicitly authorized writing the newly connected SD
card, then booting it on their CM4. The local USB disk was identified by serial
`000000001536`, disk sequence 13 and capacity 128,177,930,240 bytes, separately
from both NVMe disks. The previous card contents were replaced. No remote
system storage or EEPROM was changed and no remote reboot was performed.

- Separate test image: `image/deploy/2026-10-17-cm4-carrier-trixie-desktop-6.18.57.img`, 6,819,938,304 bytes.
- Image and complete SD readback SHA-256: `f52f98365dd6000c6cfcada54ba10e63d90b08df54a92cfd353c7e016075874e`.
- Derived from the preserved CM4 desktop raw image above. Uses the same `6.18.57-v8-uconsole` kernel/package/modules/initramfs, generic CM4 DTB, HDMI KMS and native XHCI. The uConsole overlay is not selected; its audio GPIO service and executable are removed. Existing carrier overclock settings are not copied.
- A preset `pi` account, enabled SSH and hostname `uconsole-cm4-test` were configured offline. The account wizard and its temporary user were removed. Persistent journal logging was requested, but the later boot inspection found that a vendor configuration overrides it, as recorded below. Passwords remain outside Git.
- `make image-check BOARD=uconsole-cm4 IMAGE_VARIANT=cm4-carrier IMAGE_USER=pi IMAGE=<test-image>` passed. Offline `sshd -T` confirmed password authentication and PAM. The installed password hash matches the locally supplied test password.
- The writer held the identified disk exclusively, synchronized writes, invalidated the block cache and verified all written bytes by SHA-256. Read-only FAT/ext4 filesystem checks and the same kernel/account/boot-file inspector passed on the card itself. FAT boot is 512 MiB; initial root is about 5.8 GiB. The distribution's first-boot root resize and SSH host-key generation services were enabled at deployment and completed on the user's boot below.
- The card was unmounted and the USB reader powered off successfully, ready for removal. See [carrier SD test record](../knowledge/storage/cm4-carrier-sd-test.md).

The user subsequently booted this card. The read-only inspection below verifies
the running system separately from the original image/card integrity checks.

## CM4 carrier first boot observations

After the user reported a successful boot on 2026-10-17, SSH inspection of
`pi@<HOST_04>` confirmed `6.18.57-v8-uconsole`, aarch64 and 4096-byte pages
on the same CM4 Rev 1.1. All five checked kernel Image, base DTB, configuration
and initramfs hashes match the prepared artifacts. The system identifies as
Debian 13.7 Trixie with Raspberry Pi packages, hostname `uconsole-cm4-test`.

- systemd reports `running`, with zero failed units. Password SSH works; first-boot host-key generation and root resizing completed successfully. LightDM and the labwc desktop process are running, with both HDMI connectors disconnected.
- The 128,177,930,240-byte SD is the root device. Its root partition grew to 127,632,670,720 bytes and is mounted read/write. Root has about 113 GB available at inspection. Current FAT/root PARTUUIDs, persisted `cmdline.txt` and `fstab` agree after first-boot identity changes.
- Ethernet retains DHCP address `<HOST_04>/24`, with a 1000 Mb/s full-duplex link and zero RX/TX errors in the captured counters. This is link/SSH validation, not a throughput benchmark.
- Native XHCI enumerates the Terminus `1a40:0101` USB hub at 480 Mb/s. No external USB data-transfer test or PCIe endpoint validation was performed.
- Wi-Fi and Bluetooth firmware initialized and `wlan0`/`hci0` exist. Both radios are software blocked, without a hardware block; Wi-Fi association and Bluetooth pairing remain untested.
- Temperature was 40.9 °C, firmware `get_throttled=0x0`, and configured CPU frequency 1500 MHz. These are idle observations, not load/stability tests. Deferred-probe list is empty; the captured kernel log contains no panic, Oops or SD/ext4 I/O fault.
- Logs contain userspace integration warnings (ALSA restore/rule labels, missing RealtimeKit/UPower services, a Debian-specific sysctl absent from this kernel and netplan file permissions). PCIe link-down/dummy-regulator and staging-driver notices are also present. These are recorded for follow-up; zero failed units does not mean a warning-free system.
- The intended persistent journal setting is ineffective: vendor `40-rpi-volatile-storage.conf` sorts after the test image's `10-boot-test.conf`. Effective storage is `volatile`, with the active journal under `/run/log/journal`. A later-sorting project override is needed; no journal configuration was changed during this inspection.

No remote configuration changes, package installation, radio enablement or
reboot were performed during inspection. See the [carrier boot record](../knowledge/storage/cm4-carrier-sd-test.md)
for evidence and the boundary between this boot and uConsole peripheral tests.

## CM4 Wi-Fi and uConsole next-boot preparation

The user subsequently powered the CM4 back on and authorized temporary Wi-Fi
provisioning for its move into the uConsole. A saved NetworkManager connection
acquired `<HOST_05>/24`. A fresh SSH session succeeded with Ethernet
temporarily disconnected and the route to the client confirmed through
`wlan0`; Ethernet was then restored. Wi-Fi association and standalone SSH are
now observed on the Waveshare carrier. Reconnection after migration remains
untested.

The SD was then switched to the production uConsole CM4 boot configuration.
Its audio GPIO service was enabled for the next boot and left stopped on
the Waveshare carrier. The actual boot command line preserved the resized
SD's root identity and persisted the wireless country request. Journal
persistence was corrected and verified through the effective setting and
actual journal file. No reboot or image flash was performed during preparation.

A separate private image,
`image/deploy/2026-10-17-uconsole-cm4-trixie-desktop-6.18.57-wifi.img`, has the
same temporary Wi-Fi configuration, preset account, uConsole payload and
persistent logging. Its SHA-256 is
`1c1b41806c2238896f361bc2e3eac3a991e6bfb230ee48b75d332f2f89a044bc`.
The standard image/account inspector and additional read-only Wi-Fi checks
passed. Credentials remain outside Git; normal build defaults are unchanged.
See the [Wi-Fi preparation record](../knowledge/network/cm4-uconsole-wifi.md).

## First uConsole CM4 attempt

The user reported a black screen and no router entry after moving the CM4 and
test SD into uConsole; the carrier power indicator stayed on. The returned
SD's first inspection was read-only. Kernel/boot files, root PARTUUID and saved
Wi-Fi profile/radio state match the prepared inputs. FAT/ext4 consistency
checks passed. Only the previous Waveshare session and its normal shutdown
appear in the persistent journal; there is no new journal from the uConsole
attempt. The failure stage and cause remain unknown.

The SD now uses the existing generic CM4 carrier fixture for a diagnostic
comparison, with Wi-Fi/SSH/persistent logging preserved and a verbose text
boot. The uConsole overlay and audio GPIO payload are absent, and antenna
selection is the CM4 default. The shared card inspector passed; the card was
safely ejected for a new uConsole trial. This diagnostic configuration does
not test the built-in display. See the [boot investigation](../knowledge/boot/cm4-uconsole-boot-debug.md).

## uConsole CM4 diagnostic boot observations

SSH subsequently succeeded at the user-supplied `<HOST_05>`, with the
previously pinned host key, project `6.18.57-v8-uconsole` kernel and expected
SD root identity. The generic carrier configuration boots with Wi-Fi and
Ethernet down. Replacing native XHCI with DWC2 also boots. Adding the project
CM4 board overlay then binds the AXP221 and enumerates the uConsole
`1eaf:0024` keyboard/consumer/joystick/mouse interfaces. All three snapshots
have zero failed units and `get_throttled=0x0`.

The user confirms the same adapter previously used with CM5, USB power and no
batteries. The PMIC reports battery `PRESENT=0`; its charging/capacity readings
are not evidence of an installed battery. HID enumeration works in this
condition; physical input events remain untested.

The screen remains black by user observation. DSI-1 is reported connected and
enabled at `720x1280`, and the backlight reports 5/9, but the initial log has
DSI turnaround/transfer timeouts. A later framebuffer blank/unblank diagnostic
also produced a `__vc4_hvs_stop_channel` warning. LightDM was started for
diagnosis; a running service does not validate visible output. A trace during
delayed panel-module loading shows the optional DCS ID read `0x04` returning
`-110`, followed by the first initialization write `0xe1` returning `-110`.
The GPIO strap selects a driver branch; it is not an independent panel part
identification. Omitting only the read permits all 212 traced two-byte writes
to complete, but the user still observes a black screen. A separate trial
adding the working CM5 overlay's always-on 1.8 V DCDC3 and VCI connection
still times out on the original ID read. Combining that supply configuration
with the no-read module also completes the writes and remains black by user
observation. The supply addition has been reverted on the SD; no production
panel or supply change is justified by these results. The official schematics
also connect display 1.8 V to the CM4 module output, so a missing DCDC3
consumer does not establish an unpowered display. Actual rail voltages remain
unmeasured. Low-power commands, reading single-byte power mode only after
initialization, and extending reset-release delay from 5 to 120 ms also leave
read timeouts; these trials do not establish a panel identity or display fix.
Selecting the newer reference table without an ID read completes all 189
traced two-byte writes and starts the desktop without captured DSI timeouts,
but the user confirms only backlight, with no visible content. That private
module has also been deselected by rebooting into the original installed
module. Published CM4 adapter wiring agrees with DSI1/four lanes; actual
connector rail voltages, reset timing and signal continuity remain unverified.

Temporary, backed-up configuration trials preserve `ant1`, Wi-Fi/SSH and the
verbose multi-user command line. The earlier trials retain the project
kernel. Archived `6.12.94-v8+` comparisons reproduce the ID/read-write
failure with both the project board overlay and the packaged CM4 overlay,
including its native PMIC I2C bus. Network access or SSH command execution
later stalls during panel loading; its cause and visible output are unknown.
The first comparison needed a user cold power cycle; the second retained
rollback/watchdog guards and returned to the saved 6.18 configuration without
another physical power cycle. No EEPROM or eMMC change was made. The
production `ant2`/audio/full display settings were not validated by those
earlier trials. Later fresh production-SD boots and desktop integration are
recorded separately above. See the [boot investigation](../knowledge/boot/cm4-uconsole-boot-debug.md)
for the earlier boot identities, evidence and recovery limits.

## Not established

- Exact carrier-board revision and whether every retained boot option is necessary.
- Battery cell specification and the need for the comparison fork's PMIC programming changes.
- Repeatable cold CM4 power-button-to-visible-desktop startup, the cause of the earlier composed-black capture, and the cause of earlier panel failures. Full keyboard coverage, battery percentage accuracy and charging behavior, headphone/capture routes, Bluetooth discovery/pairing/audio, suspend and power-off behavior with this 6.18 adaptation and the full CM5 project kernel/image remain unvalidated; the Ubuntu labwc session now has user-confirmed trackball and terminal-shortcut input.
- A tested deployment/recovery procedure for the original CM5 eMMC unit; the
  current CM4 test system uses an SD card.

## Next work

Boot optimization is paused at the user's request. The user confirmed that the
previous CM4 loss of connectivity followed battery exhaustion. CM4 is now
reachable on the same SD installation; the earlier unreachable state is not an
outstanding network-recovery task. Retain the existing boot configuration and
measurements without starting further timing experiments.

1. Complete remaining desktop and peripheral functions. Bluetooth discovery,
   pairing and audio need an appropriate peer; ScreenCast browser/OBS clients
   remain separate from the completed portal/PipeWire pixel test. Preserve
   working Wi-Fi and distinguish functional results from service-presence checks.
2. Rebuild the Noctalia Ubuntu image to include the current screenshot tools,
   lockscreen transition policy, browser/media applications and Amberol layout.
   When the designated SD reader is available, boot that inspected image and
   verify first-boot resize, partition references and physical desktop/input
   behavior. The live installation remains a separate reference; image inspection
   does not establish fresh-card boot behavior.
3. Plan physical checks for full keyboard coverage, headphone/capture paths, USB
   transfer, battery accuracy/charging, suspend and power-off. Do not infer hardware
   success from telemetry or command completion alone.
4. Keep generic-carrier CM4, uConsole CM4 and CM5 validation separate. Preserve the
   recovered display baseline and original CM5 deployment; future non-Pi boards
   require their own controller/firmware/DT validation and recovery procedure.
