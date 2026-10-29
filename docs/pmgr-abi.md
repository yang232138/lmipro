# Opt-in original PMGR ABI integration

The current iPadOS 17.7.11 / 21H461 AMFI requires a genuine ApplePMGRNub.
The selected source kernel has no board PMGR integration entry, so adding
`sheng-pmgr=2` alone does not supply this service.

This integration reuses `ShengPlatformPower.cpp` and its guarded builder from
UniXNU/UniXNU-sheng commit `516a780473a2ac59c8b438e132bb60204dda9e22`.
Runtime class names, exported integration symbols and original private ABI
identities are retained. The source has its original SPDX identifier; the
reference repository license is retained in `package/PMGR-REFERENCE-LICENSE`.

Only the board source path and opt-in build gate are adapted in the builder.
A lmipro wrapper connects the existing integration to bsd_init and emits a
constant diagnostic panic after actual mode-1 checks. No PMIC, clock, MMIO,
sleep, reset or secondary-CPU success is implemented by this provider.
Unimplemented original virtual operations stop with an unsupported panic.

The pinned upstream audit checks unchanged AMFI and ApplePMGR executable
hashes, rebound IOKit imports, the original nub/provider dispatch, and the
actual target AppleT8011PMGR constant-read/no-op-write behavior. The builder
uses native XNU compile records and verifies exact storage, ARM64/iOS17 ABI,
all 340 vtable entries and every bound original import. Late class registration
does not modify instructions.

`CONFIG_XNU_SHENG_PMGR=y` selects the guarded archive and pre-AMFI hook.
The runtime default remains off. `sheng-pmgr=1` allocates, calls and releases
unpublished objects; the lmipro wrapper reports LIFECYCLE PASS only after
checking original class sizes, provider binding, actual read values and
released instance counts. `sheng-pmgr=2` is reserved for the subsequent genuine
service-publication test after the lifecycle observation succeeds.

Build a fresh reference candidate and its JSON using the same pinned sources
with this option temporarily off. The CI preparation step copies that exact
candidate, report and verified original kernelcache into the reference paths
required by the pinned audit. It then restores the declared device config,
builds the archive through `[kernel] archives`, and rebuilds the affected
kernel using the official driver. No obsolete local candidate is accepted.

The firmware, device hardware tree, userdata geometry and AMFI executable
remain unchanged. The first image is a mode-1 lifecycle diagnostic, with the
official no-reset panic screen. It does not claim a working power backend or
an OS root volume.
