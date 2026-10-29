# lmipro device-tree panic region

The current source kernel uses the BCM2837 4 KiB machine configuration. Its
PE_create_console begins by mapping the fixed PLATFORM_PANIC_LOG_PADDR
(0x3c0fc000, 0x4000 bytes), then reading and possibly clearing the region.
That address has not been supplied as an lmipro panic reservation.

With the same verified firmware, serial_init, early startup initialization,
the first kprintf, and the mapping inside PE_create_console returned (three
entry colors, then a six-second reset). PE_create_console itself did not
reach the corresponding post-call reset. These observations narrow the
failure; they do not by themselves prove the fixed-region access is the cause.

The pinned UniXNU already applies native-dt-panic-region.patch. Enable its
existing XNU_DT_PANIC_REGION option in the device defconfig. The upstream
code reads pram/reg and /chosen/embedded-panic-log-size with size, overflow
and mapping checks. Missing input returns without accessing a guessed region.
No panic region is invented for this board.

Build using UniXNU ee472385743ebd16941ab14892dcb9d02e219d15, then verify
both the applied patch and the compiled DT branch. Keep the current UEFI
FD and test a six-second reset after PE_create_console before proceeding.
