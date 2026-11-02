# Architecture

## Execution model

The vendor bootloader authenticates its boot image and starts the existing
`4.1.15-lab126` kernel. Kindle init and services continue to own hardware,
networking and power management. The manual Alpine chroot provides ARMv7
userspace while sharing that kernel. It is not a VM or a replacement boot image.

The user-store filesystem is VFAT underneath a vendor FUSE view. Linux ownership,
symlinks and executable files therefore belong in an independent ext filesystem
image, not a directory extracted directly onto VFAT. Deployment must distinguish
`/mnt/us` from its underlying `/mnt/base-us` and validate the actual loop backing
path before selecting either.

## Ownership and lifetime

Human commands live in `Makefile`. Offline status uses `Makefile.agent`.
`tools/project.py` owns manifest and policy validation. `tools/rootfs.py` verifies
fixed upstream bytes against both the SHA256 pin and the pinned PGP identity.
`tools/device.py` owns the explicit read-only SSH probe.

Installation and execution are explicit operations, separate from input preparation.
Mounts belong to a private namespace, and an invocation must own and release its
loop association. Shared host mounts, vendor loop devices, PID 1, system boot
configuration and raw storage are outside the installation scope. The device's
old mount utility lacks `--make-rprivate`; the tested namespace currently has
private propagation. The launcher rejects shared propagation instead of silently dropping isolation.

## Trust and evidence

Alpine identity lives in `manifest/alpine.toml`; the public signing key is tracked.
Existing nonempty caches are verified and are never silently accepted or replaced
after a mismatch. The Amazon GPL source receipt is research provenance, not a
reproducible firmware build or a recovery image.

Device observations and source-level conclusions live in `docs/status.md` and
`docs/flashing.md`. A root shell, a readable disk or a detected Fastboot interface
does not establish an offline restoration method. No implemented command writes
to physical eMMC block devices or changes vendor boot variables or firmware.

## Implemented image runtime

`tools/chroot.py` stages the verified archive and `image/alpine.sh` under the user
store. The installer creates a new 512 MiB regular image file, disables ext4
features inappropriate for the old kernel, extracts Alpine, verifies a native
ARMv7 executable and publishes an installation receipt. It refuses an existing
image. A host receipt records that vendor boot arguments and the root mount
matched before and after installation; it is local deployment evidence, not a
live query or a reproducible application package snapshot.

Every run locks the runtime, enters a private mount namespace and uses only an
unassociated spare loop node from 13..16. `LOOP_CLR_FD` after mounting requests
kernel autoclear when the final reference closes. Cleanup unmounts owned mounts
in reverse order and reports failures instead of forcing a busy unmount. The
inherited lock and an existing-image association check prevent duplicate mounts.
A private minimal dev filesystem exposes no eMMC block nodes. Sysfs is read-only;
proc and a bind of the host devpts support foreground programs. This is not a
security boundary for untrusted root code. It has no USB-export interlock.

`tools/chroot_check.py` checks the runtime with explicit hardware contracts,
including failed-command cleanup. It is never called by offline checks.

The minimal development bootstrap is explicit and confined to the image. Its
repository and package list live in `manifest/alpine.toml`. APK verifies package
signatures. The base tarball is pinned; repository package revisions are rolling
inputs, with locally recorded runtime observations rather than a package lock.

## Display bridge and application examples

`apps/hello/main.py` generates UTF-8 in Alpine. The host `image/display.sh` feeds
its completed output to the existing glibc FBInk and CJK font. The chroot exits
before drawing; it retains its minimal device set. FBInk owns the vendor display
ABI and EPDC refresh. This one-shot demo leaves the vendor UI running and does
not claim exclusive screen ownership or input handling.

`tools/display.py` explicitly installs/runs the demo, probes FBInk without eval,
and captures an 8-bit grayscale framebuffer with its actual stride and offsets.
The companion KOReader plugin runs inside KOReader, independently of Alpine;
installation validates syntax but does not restart the reader or claim UI tests.

## KOReader app starter

The plugin's main.lua owns widgets and dispatch; kindledevstate.lua owns pure
application state. LuaSettings stores changed counters on close or reader flush,
not on every tap. A read-only device-information popup has no Alpine dependency.
The ordinary initialization only adds a menu/action. An explicit preview launch
environment opens the panel without adding persistent startup configuration.

Plugin updates validate Lua syntax and source hashes in a staging directory,
preserve the old plugin directory, and attempt rollback if publication fails.
The saved counter is outside the replaceable code directory. Pure Lua behavior
checks use mocked KOReader APIs; real UI/touch evidence remains a separate layer.

## USB transport

Plugin code can be installed directly into the mounted Kindle VFAT user store.
The USB installer validates the volume identity, fsyncs and verifies staging bytes,
runs the pure host Lua specification and publishes with the existing backup /
rollback policy. It never executes device code during mass-storage mode.
The receipt labels its transport and does not claim device-Lua or UI validation.

`tools/usb.py` identifies the Amazon USB device and remembers its physical port.
It excludes unrelated USB gadgets. The explicit configure operation only acts on
a unique matching network interface, using a tagged NetworkManager profile with
no default-route or DNS takeover. Device-side USB mode and SSH readiness remain
separate observations; no persistent Kindle startup service is installed.

The 0.2.0 plugin adds a read-only USB diagnostic operation and an explicit
session-only helper. Preflight checks the retained-root policy, device identity,
keys, cable state and inactive Alpine image. Only then does the UI start a detached
helper and send KOReader's normal Exit event. The helper waits for reader exit
and volumd responsiveness before requesting the documented USB-network property.
It uses the existing host identity/public keys for a dedicated USB-bound Dropbear,
with password authentication disabled. No raw gadget/module manipulation, vendor
script replacement, boot service or automatic reboot is part of this path.
The manual device execution and dedicated USB root SSH have now been verified.

The 0.2.1 runtime has now been validated on the device: USB enumeration, a
host-only NetworkManager link, the previously trusted host identity and strict-key
root SSH all succeeded. The default local device configuration selects USB.
Device Lua behavior checks, Alpine lifecycle checks and real KOReader rendering
completed over that link; SSH remained available while KOReader ran. No device
boot service or USB Internet sharing was introduced. Local receipts retain the
scope of the observations rather than asserting persistent connectivity.

## USB autostart in the user store

The explicit host USB installer owns /mnt/us/kindle-usb, a marked emergency.sh
and a marked ENABLE_USBNET. It refuses unrelated files and symlinks before
staging. The existing KMC framework_ready hook copies its code into /var/tmp,
changes directory there and launches a detached worker. No vendor-root Upstart
file is added. The worker first requests networking via the native volumd
property and HAL events, waits for usb0 and the remounted user store, then
starts the dedicated SSH runtime. Existing USB interfaces and SSH listeners
survive repeated hook calls. Locks are closed before daemonization.

The first wait-only worker raced USB storage export: the native daemon unmounted
the user store before usb0 appeared, removing the flag that the worker polled.
Native ENABLE_USBNET references alone did not establish working boot activation.
A regression fixture now removes the user store before worker startup and
requires a mode request, remount, SSH readiness and persisted diagnostics.
The second reboot produced a network interface, but SSH refused connections;
complete automatic startup remains unverified. Volatile worker logs and a
changed boot ID are required before recording success. Alpine remains manual.

USB autostart is now rolled back following the drive-mode hang. A verified
Kindle VFAT session confirmed the active marker absent and removed the exact
managed emergency.sh after checking its deployment hash. The volume was synced
and unmounted. Runtime files may remain inert in the user store; there is no
active marker or hook entrypoint. The host enable operation remains blocked.
