# PID 1 file signature progress investigation

Handset photo 1794276240162.jpg on build 42738148128 shows initStage=22, execError=0,
PID 1 launchd, SVC entries 378 and returns 377. Last entry syscall 92 (fcntl),
x0=0x34, x1=0x61 (F_ADDFILESIGS_RETURN). Last return syscall 153 (pread), x0=0x4000.
First observed BSD carry error was syscall 294 (shared_region_check_np), errno 12;
later calls executed, so this earlier error is not established as fatal.

Root phase 2; Data stage 4 status 0 readonly 0; typed RAM seed ready 33554432 bytes,
reads 57 writes 0 failures 0. This does not qualify actual write completion.
User exception EC 0x24 is a user data abort and is not by itself a fatal exception.

The pinned native-4k-signature-hash-cache implementation reads and verifies the
whole signed range before deriving native hashes. This diagnostic preserves that
implementation, all authentication and error returns, and UFS/GPT/APFS operations.
New PID 1 markers surround the original fcntl signature operations. An opt-in
timer samples existing global signature counters at startup, 30 and 600 seconds.
Counters are cumulative, not a per-file pathname identification. A marker alone
does not prove a deadlock; compare actual counter changes and return paths.
No device writes or full OS boot are claimed by source/host/build verification.
