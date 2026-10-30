# Volatile root-device bring-up

The qualified PMGR publication image reached `Failed to mount root device` at
IOKitBSDInit.cpp:984. Photograph 1793314717812.jpg shows this actual observation.
Image SHA256: fec97f0894a44d6633fa1def3b821508604a5953fb645eee6e271e607d6e0dc6.
This passes the previous AMFI No PMGR stop; it does not qualify a complete boot.

Select unmodified RamBlock 25c57e0e9b68bf452dd1986914525edc9d2320bc and
CONFIG_XNU_NATIVE_RAM_BLOCK. Preserve the complete inherited component/archive
arrays, DT panic-region fix, PMGR source ABI and original security components.
No native UFS, persistent callbacks or phone userdata geometry is selected.

The board binding recipe compiles public IOStorageFamily source at
e45fbe30a7879e0b16e26dddb78538f1b00e0bff as an annotation object, never linked.
Against the qualified 49334c98 candidate/prelink plan, it requires exact 190-slot
dispatch, compiled class size 0x90, original metaclass registration with real
IOService parent and original constructor/destructor thunks. Every concrete
RamBlock override and undefined original-class import is checked before aliases
are emitted. Original IOStorageFamily executable bytes remain unchanged.
This is STATIC ABI evidence, not the upstream audit's live 4K metadata result.
Disk I/O and mount success still require a handset observation.

Build a fresh APFS container inside a disposable GitHub macOS virtual disk.
Extract only its APFS partition into a page-aligned raw image; never use a
physical-device path. Pack it with DarwinBoot 3325e352d7df68a9f9ad74832ed94a6acbbd7390
Test/packed_ramdisk.py. Verify the complete logical roundtrip hash, APFS NXSB,
actual volume UUID and encoded size <=8 MiB before composing the 64 MiB FAT.
Use --data-ramdisk and payload.adt.packed_data_ramdisk with the measured
root_match_volume_uuid. DarwinBoot owns the seed memory and RamBlock publishes
real volatile block storage through the original IOStorageFamily.

The test volume contains only an explanatory marker, no complete OS, launchd,
trust cache or authenticated System/Cryptex root. Mount policy or missing userland
may therefore be the next genuine failure. Do not weaken root/AMFI policy or
claim system boot. Keep sheng-pmgr=2 and the no-reset panic screen for evidence.
Use only fastboot boot on the checksum-verified image. No phone partitions,
internal storage addresses, DMA windows, registers or vendor binaries change.
