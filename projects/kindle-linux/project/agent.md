# Agent interface

Read README, architecture, status and flashing notes, then run:

```sh
make -f Makefile.agent handoff
make check
```

## Handoff JSON

`make -f Makefile.agent handoff` prints one JSON object to stdout and does not
connect to a device or download anything. Schema version 1 contains:

| Field | Meaning |
| --- | --- |
| `schema_version`, `project` | Interface identity |
| `revision` | Git branch, commit (`unborn` when absent), cleanliness |
| `target` | Board identity and device policy from TOML |
| `userspace` | Pinned Alpine archive, checksum and signer |
| `stage` | Implemented engineering stage |
| `offline_restore_verified` | Whether a tested restoration path exists |
| `alpine_installed` | Whether a local installation receipt exists |
| `deployment_evidence` | Local recorded installation receipt, or null |
| `usb_autostart_evidence` | Recorded USB configuration and verification scope, or null |
| `handoff_documents`, `next_actions` | Required context and next work |

The stage is usb-autostart-pending when a recorded enabled configuration lacks reboot verification.
The current baseline implements a manual chroot. Deployment status comes from
ignored `work/chroot-installation.json`, validated for architecture and recorded
vendor boot-state verification. A new checkout without this local receipt reports
`alpine_installed=false`; this is recorded evidence, not a live device query.

The underlying Python command exits 0 on valid metadata and 2 on invalid or
missing metadata. Errors are JSON objects with `schema_version`, `project`,
`error`. GNU make maps recipe failures to its own exit status, normally 2.
A dirty or unborn repository is reported and does not itself fail validation.

## Other commands

`tools/rootfs.py fetch|check` returns 0 after checksum and pinned signature
verification, 2 on invalid input, failed download or verification error. It only
writes ignored host caches and a temporary GPG home.

`tools/device.py probe` explicitly contacts the configured device using SSH,
public-key authentication and strict known-host checking. Its schema version 1
contains an observation timestamp and `device` fields: kernel, architecture,
machine, bootargs, eMMC sector count, total memory, root mount and screen modes.
It exits 0 for a complete matching-board observation and 2 for configuration,
transport or response errors. It does not label recovery or arbitrary bootability
as passing. Save raw outputs under ignored `work/`; they can contain local details.

## Explicit chroot operations

`tools/chroot.py install` verifies the upstream input, probes the board, stages
assets and runs the regular-file image installer. It refuses an existing image
and checks vendor boot parameters/root mount before writing its local receipt.
`tools/chroot.py run -- COMMAND ...` runs one foreground command; without COMMAND
it opens a shell and allocates a TTY when invoked from an interactive terminal.
Command exit codes propagate; setup/transport errors return 2, cleanup failure 125.

`tools/chroot_check.py` or `make alpine-check` explicitly connects to the board.
Success returns 0 and writes `work/chroot-check.json`, with observed release,
kernel, architecture, UID and APK version plus booleans for persistence, exit
status, mount/loop cleanup, exclusive session locking and unchanged vendor state. Failure returns 2.
Alpine remains manual; these chroot commands do not install boot daemons.

`tools/chroot.py bootstrap` uses the manifest profile and `image/bootstrap.sh` to
configure the image repository, install the declared packages and verify native
Python plus an HTTPS response body. It records `work/chroot-development.json` and
checks vendor boot state again. Package revisions are not immutable input pins.

## Display and plugin operations

`tools/display.py probe|capture|demo|plugin-install` explicitly contacts Kindle.
Probe/capture only read display state; demo updates its user-store files and
writes one page with the existing host FBInk. Plugin installation adds the new
project plugin and checks Lua syntax without restarting KOReader.
Success returns 0 with JSON; setup, device and render errors return 2.
`ui_tested=false` in plugin installation is intentional until real interaction is
verified. `work/display-check.json` records display geometry, a framebuffer PNG
path and vendor-state/chroot-cleanup checks, not optical panel measurements.

## KOReader development entrypoints

`make app-check-local` runs the Lua behavior specification without a device.
`make check` includes it when a host Lua executable or liblua5.3/liblua5.4 exists;
a missing optional runtime is an explicit skipped unittest, not passing evidence.
`tools/plugin.py install|check|preview|status` is the focused plugin interface.
Install stages/verifies Lua and SHA256 before directory publication and preserves
the previous directory outside the active plugin tree. Check runs the same Lua
spec on the device with mocked UI/storage. Preview starts the existing reader
wrapper only when no reader/update is active; it uses a unique readiness token,
records a capture and never labels simulated checks as real touch validation.

Status is offline JSON schema 1: plugin identity, source_sha256,
recorded_install_matches_source and recorded_preview_matches_source. Both
booleans compare source hashes against local receipts and do not query hardware.
Other operations explicitly use SSH. Errors exit 2; successes exit 0.
The install receipt has per-file SHA256, syntax/vendor-state results and a backup
path. Preview evidence includes per-file source hashes and real_touch_tested=false.
A fresh checkout has no local installation or preview receipts; consult recorded source hashes and explicit device checks.

## USB entrypoints

`tools/plugin.py install-usb --mount PATH` updates only a verified mounted Kindle
VFAT user volume. It records transport=usb-mass-storage, usb_readback_verified,
host_lua_behavior_verified and device_lua_checked=false. This is an explicit
filesystem/device operation, not part of offline checks. `app-install-usb` uses
the local default mount path; deployment and UI readiness remain separate.

`tools/usb.py probe|remember|configure` returns schema-1 JSON. Probe is read-only;
remember stores ignored host-local identity. Configure uses NetworkManager only
when a unique Kindle USB interface exists. It does not install device software,
verify SSH, or change the project SSH configuration. Errors exit 2; success 0.
The configured profile has a project stable-id, so unrelated same-name profiles
are not changed. See docs/usb.md for current verified transport and remaining lifetime limits.

Plugin payload discovery includes owned .lua/.sh files. SSH installation validates
Lua with the native interpreter and shell with sh -n; USB installation checks host
Lua behavior plus shell syntax before publishing. The 0.2.0 USB launcher is manual
and session-only, with preflight and normal-reader-exit ordering. Device-side USB
and dedicated port 2223 have completed a real run. Diagnosis/launch logs
remain in the user store and are not committed to Git.

The local default connection now uses the verified USB peer on port 2223.
work/usb-check.json records the actual SSH endpoint pair, UID, trusted-key match,
route and retained boot flags; device-usb-probe.json records the full probe.
app-preview has matching-source rendering evidence, and device Lua / chroot
checks were executed over USB. A fresh checkout still requires local connection
configuration and host-key verification; these receipts are not live status.

## Explicit USB autostart

`tools/usb.py autostart-enable|autostart-disable|autostart-status` operates over SSH.
It reuses native ENABLE_USBNET and the existing KMC user-store script hook.
Enable validates device policy, hook availability, ownership, staging hashes and
shell syntax, publishes complete code before the mode marker, then exercises the
worker without resetting the live link. Disable removes only managed hook/marker
files and leaves the current connection running. It takes effect on the next boot.
The ignored installation receipt records worker verification separately from
reboot verification. The first boot log established a user-store unmount race, now covered by a regression.
The revised worker runs from /var/tmp and explicitly requests native networking.
The second reboot restored the USB interface but TCP 2223 refused connections;
read the volatile worker log after recovering manual SSH before changing daemon startup.
No failed boot check can establish automatic startup success.

Autostart enable is now blocked before device contact following the drive-mode
hang. The user confirmed renaming ENABLE_USBNET and rebooting to normal UI.
The recorded stage usb-autostart-disabled-cleanup-pending distinguishes that
user-confirmed disable from live verification/removal of the residual hook.
Do not resume startup experiments as part of this cleanup.

The latest recorded stage is usb-autostart-rolled-back after verified USB storage
cleanup. work/usb-autostart-rollback.json records absent active marker/hook and
successful host unmount. This does not claim optical UI behavior after unplug.
Enable remains blocked; ordinary offline checks never reconnect or redeploy it.
