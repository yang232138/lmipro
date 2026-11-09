# mount PID 3 regression audit, 2026-11-09

## Confirmed source selection regression

The latest successful overlay 677e454125effd5d970c61e9689b6d9b6bd86aff pins
device 9508f9ef07c9f894aaaab546cb8d7e8c62cfeb1c and framework
bbde4a06d695cb5191735a14b1eaad0aca4883a3. Device 9508 and the earlier
boot-identity device a7f46d28cdf673cf1d75aa1c9225cb22ab9550a8 diverge at
26a16e43f0a708e90c555a3bbbbfd0d27db413a7. GitHub compare's file list is
against that merge base, not a direct diff of the two tips.

The current branch never integrated the boot-identity and mount-output commits.
Its patch series has only PMGR and skip-root-auth. Its actual raw kernel lacks
both the boot identity symbol and MOUNT LOG diagnostic. The current config
retains SystemOS startup and union, but does not select RamBlock or Data mount.
The successful build therefore did not test the separate volatile Data branch.
Overlay 3cf87593ca2dc9273a147274dae3b5619ba15ebd selected the other Data
attempt based on device 26a16e4, rather than advancing the boot-identity branch.
Restoring union in 9508 did not restore boot identity.

## First fatal operation and ordering

mount[3] is a process name and PID, not a kernel mount stage number. The earlier
instrumented boot 9b6f51b08dc89f6cb6a80ad0674dde78867a4803beccf89873181fc54de0cc5d
captured mount stderr: filesystem info warnings twice; Data missing but not
required in env 0; get boot-manifest-hash fails ENOENT; mount exits 66, signal 0.
The new uninstrumented screenshot alone cannot prove its first syscall, but the
same required identity producer is demonstrably absent from source and binary.

bsd/kern/kern_exec.c load_init_program mounts owner SystemOS via
native_owner_cache_prepare before exec of /sbin/launchd. The repaired hook
publishes the checksum-checked SHNGROOT namespace immediately after successful
SystemOS mounting. launchd subsequently starts /sbin/mount (observed PID 3),
which reads this boot namespace. kern_exit reports a critical-process exit;
the restored sys_generic writev capture preserves its real stderr and code.

The identity is SHA384(domain including NUL || checksum-checked SHNGROOT header)
with fixed local layout/hash pins; it is NOT Apple authentication or verification
of all installed blocks. Existing authorization behavior is unchanged.

## Minimal repair

Restore only identity validation/publication, its weak reference-build fallback,
the two board patches and patch series. Preserve current framework, union,
Owner Cache, UFS transport, GPT, APFS, firmware and boot arguments. No Data
mount is enabled in this recovery candidate. Do not suppress exit statuses.
Source application against the current framework is checked before publishing.
Host/build verification is not handset validation; no device commands are run.
