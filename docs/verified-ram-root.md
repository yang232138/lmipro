# Authenticated read-only RAM root: binding audit

The prior device result is recorded separately in observations/apfs-crypto-20261030.md.
Now adapt canonical RamBlock25c57e0e9b68bf452dd1986914525edc9d2320bc in an owned
build copy. Its original checkout stays unchanged. A versioned patch, original
and adapted hashes, exact compile commands and 190-slot concrete ABI audit
identify the final object. The existing mode0 persistent-store definitions remain.

The actual pinned VerifiedRoot RSA3072 policy/key are copied into an immutable
VM allocation. Signature, logical capacity and full seed SHA256 must agree before
publication. An isolated scratch mutation must fail genuine XNU block validation.
Writes return NotWritable; the transport reports write protection. Persistent
overlays never attach to this signed root.

LmiproVerifiedRamIo.c authenticates each complete signed4096-byte block in a
private snapshot BEFORE copying the requested subrange to its consumer. Source,
hash or output failures report only the verified prefix and take the transport
offline. Root binding fails after I/O/integrity errors. Wrong image/signature/
capacity refuses startup. Policy/key mappings are independently protected readonly.

The same actual C read primitive passed four host groups with the actual signed
268394496-byte test image and real OpenSSL provider: aligned/unaligned/tail reads;
first/later block mutation with no unverified-byte publication; source/short-copy
failures; outside/overflow/cross-end bounds. This is host evidence, not XNU or
hardware qualification. Official seven crypto groups and all65526 signed image
blocks had separately passed in run41033006023.

The original XNU owner hook supplies the ACTUAL mounted vnode's BSD device.
The RAM implementation requires exactly one corresponding IOMedia, the real
NativeRamBlock in its provider ancestry, exact signed UUID, a healthy verified
transport and successful wrong-UUID refusal. The original hook independently
requires MNT_RDONLY. No fake VirtIO controller, root-name override or UFS access.

CONFIG_XNU_OWNER_ROOT_AUDIT=y; OWNER_ROOT_TRUST stays OFF. Offline mockfs is
explicitly disabled and its archive removed. Actual APFS must continue to mount.
The existing authentication refusal prints fs/crypto, actual owner decision,
readonly flag, verified-block count, scratch-tamper and wrong-UUID results,
I/O and hash failures. Expected audit success is apfs,crypto=1,owner=1,ro=1,
positive verified blocks,tamper=1,uuid=1,io=0,hash=0. It still refuses root until
the readonly/mounted-device path has real device evidence. This audit does not
enable the trust flag or claim full OS/userland boot.

No physical memory ranges, hardware registers, firmware, partitions or persistent
storage change. A complete OS still requires authentic OS/SystemOS inputs and
an appropriate real storage path. The current blank marker seed is only a test.
