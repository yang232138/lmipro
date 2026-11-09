# Current-source volatile Data audit, 2026-11-09

Recovery prerequisite: build 42550859729 passed and its embedded kernel was
independently checked for a strong lmipro_publish_boot_identity definition,
the load_init_program BL call, exact source hash, capture markers and firmware.
boot.img SHA256 5bd632f218ebaa9cee9cf54ff1813b73978fa28cf895b00145cda7232391003d.
This is build verification; handset mount/tzinit recovery is not yet observed.

## Confirmed build dependency fix

The prior volatile-data run 42481233268 failed in archive-10 with pinned
kernelcache missing. build_archives precedes kernel.prelink; the latter is
where a cold ordinary build downloads KernelCache. The RAM bindings builder
consumes that cache earlier. Fetch KernelCache immediately after device fetch,
before either reference or declared kernel build, and retain the hash check.
Adding KernelCache to the later trust-cache step alone cannot fix this order.

## Audited transport and mount

Catalog RamBlock 25c57e0e9b68bf452dd1986914525edc9d2320bc is unchanged.
NativeRamBlock::start reads the loader-owned PackedRAMDisk mapping, validates
physical bounds, imports nonzero pages, computes logical seed SHA256, sets
online/ready and registerService. Logical capacity is 33554432 bytes / 512 =
65536 sectors, last valid sector 65535; packed input is 942160 bytes and imported
resident pages occupy 942080 bytes. Packed file size is not device capacity.
The raw seed starts with an APFS NXSB container at byte zero, without a GPT
prefix. UUID-selected IOMedia must have NativeRamBlock ancestry and one unique
match, not a guessed disk number or md0. The iterator and temporary properties
are released; the registered provider and imported pages persist for service
lifetime. Original boot mapping is loader-owned and not freed by this driver.

Seed logical SHA256 8e9ff98f62a4dfc5f8e82ef30f435cfe035ab9830fac59210f8f6a67a5ef097c,
packed SHA256 2434ed1fdfc09f518fd6b18e6ef77ea94aff5090ed98a8d29018dc013cb42c13,
volume UUID 32EC6E37-C75B-4305-AA27-C2FEA4E67F30, revalidated from fixed artifact.
The original seed-generation macOS mount/write probe is recorded, but no new
host APFS fsck or handset APFS mount is claimed in this Windows audit.

SystemOS cache/union prepares first, then boot identity, then board Data at
/private/var, then exec of launchd. The Data helper refuses wrong seed size/hash,
missing typed media, failed copyout/mount, wrong actual device, read-only mount
or non-APFS filesystem. It preserves errno and stage; there is no success-on-error.
The UFS driver source, GPT/window parser and SystemOS implementation are unchanged.
Only overlapping already-qualified absolute IOStorageFamily aliases are emitted
once by the RAM and UFS binding recipes, with equality checks before deduplication.
Build 42552959529 reached final kernel link after archive-10 succeeded, then
failed on six native_ufs_store_* undefined references. Darwin static linking
requires these weak_import symbols despite RamBlock documenting them as optional.
The board now supplies LmiproVolatileStore.c: mode zero selects the existing
volatile branch; open/read/write/sync all refuse with result zero (this ABI uses
one for success), GPT returns 0xff/not checked. No physical I/O implementation
is supplied. This resolves linkage without ignoring undefined symbols or masking
RAM/APFS errors. Host tests check refusal and unchanged caller buffers; final
kernel verification checks actual mode/read/write/sync bodies and source hash.
Run 42554550015 then exposed a board source-path mistake in the new recipe:
tree/package is the shared package; tree.py maps the board package to
kits/lmipro. Correct only the recipe provider path to kits/lmipro, matching
the existing UFS and handoff recipes. The macOS host refusal test passed before
this missing-input error; there was no boot image from this failed run.

## Host checks and limitations

The actual C++ packed importer and SeedDisk identity both reproduce the Python
logical hash. 4000 sparse differential operations pass using a host-only test
boundary update. The ORIGINAL sparse test failure is retained: it still expects
rejection above 2 GiB while the pinned parser intentionally permits 1 TiB and
includes a large-disk backend. Only the copied host test boundary was adjusted;
the pinned driver/parser were not modified. Big-disk component test also passes.
Nine board mount refusal cases pass in the existing host harness.

Actual RamBlock registration, APFS discovery, mount ordering, device lifetime
under handset scheduling, timezone syscall and later launchd services remain
hardware checkpoints. In particular, a one-shot IOMedia lookup returning ENOENT
would require readiness evidence before adding a wait/retry. A 32 MiB volatile
seed is a bounded bring-up input, not a production Data volume or full UI claim.
Do not flash, write userdata, or count compilation as hardware success.


## Actual packed input audit after kernel build

Run 42555657558 compiled the reference and PMGR kernel and packed a boot image.
Its final verifier incorrectly required an optional boot_progress_stage string:
CONFIG_XNU_BOOT_PROGRESS is disabled and the macro compiles to ((void)0).
The failure panic and stage variables are present. Correct the verifier using
the compiled config and verify real BL calls, not a disabled progress string.
Actual load_init_program calls: identity 0xfffffff0076ec79c, RAM seed
0xfffffff0076ec7e8, Data mount 0xfffffff0076ec97c, then launchd exec wrapper.

Inspecting the embedded ADT exposed an independent real Data input error: the
DBR seed was labeled RAMDisk. NativeRamBlock::start selects its packed importer
only for PackedRAMDisk; the 942160-byte DBR input is not a page-aligned raw disk,
and the ordinary path would fail its byte-length check. Set board
payload.adt.packed_data_ramdisk=true, matching the actual seed format.

The already compiled kernel is reused. Re-serialize the actual ADT with the
pinned framework's node() encoder after checking a complete byte-exact round
trip; change only /chosen/memory-map/RAMDisk to PackedRAMDisk, retaining the
16-byte loader placeholder. All other ADT bytes and payload file contents are
compared unchanged. Pack with the original pack_bootimg.py, pinned Mu-Silicium
mkbootimg.py and same BootShim source; verify shim/FD/firmware DTB unchanged.
No kernel rebuild was necessary for this packaging-only source change.

Result boot SHA256 880a4d296d099581850a9849d54e945f4a9781aca4abbcacabaa9ad5f9ea35b1.
Raw kernel e5e92445c294720e27885b779b9bcf969022fac41c85d65a441a071411fffe31,
payload kernel a564724022a29572bad88a1aedfd89941d12684a357d3b7a3302e29f9888e691.
Independent verification checks actual embedded files, 11 storage aliases,
strong identity, real identity/seed/mount/exec calls, refusal bodies, config,
seed/patch/provider source hashes, and confines probe changes to original entry
and panic areas. The cloud run remains failed at its original verifier; the
repack and corrected verification are local results, not a green cloud run.
Handset mount, tzinit exit 30 resolution, Data writes and launchd startup have
not been observed. No device command, flash or userdata write was performed.
