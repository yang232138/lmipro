# Opt-in Data stall checkpoint, 2026-11-09

Handset results now distinguish three observations:
- Restored userdata: the existing UFS diagnostic accepted the SHNGROOT checksum
  and pinned metadata and started both OS/SystemOS window devices.
- Current recovery kernel with Data/RamBlock disabled reaches tzinit[17],
  signal 0, exit status 30 (user transcription).
- Current Data/RamBlock kernel remains on the early color bands, without a
  captured Data errno. That does not locate the stall inside APFS mount.

Add an opt-in lmipro-data-watch=N (5..120 seconds, default absent/off) snapshot.
Start its separate kernel thread after both window devices start, immediately
before IOService::startDeferredMatches. No UFS commands or metadata parser
changes are made. rootphase=1 means inside/awaiting that deferred pass;
rootphase=2 means it returned. completeWindows and pubState are existing
completion fields, assigned only AFTER the deferred pass, not counts of the
already started devices when rootphase=1.

BSD dataStage=-2 means load_init_program has not started the tracked work;
10 is SystemOS preparation; 11 is identity publication; 0 is typed RAM seed
lookup; 1 is checked seed identity; 2 is mount; 3 is post-mount validation;
4 means the existing writable APFS mount/device checks all passed. Keep the
original real errno and panic paths. The timer reports stage, errno, readiness,
capacity and RAM read/write/failure counters, then intentionally stops with a
clearly labeled diagnostic panic. A stalled operation is not falsely assigned
an I/O error. Stage 4 makes the diagnostic thread terminate without a panic.

Do not enable the global BootProgress build option: the pinned framework guards
it behind NativeUfs/Virtio dependencies absent on this board. The board snapshot
does not enable those dependencies, change the root windows or add persistence.
thread_terminate_self is declared in the pinned xnu-10063.141.1 kern/thread.h.

Host checks exercise the actual header: default-off/zero, invalid delays,
thread creation failure, all tracked stages and success cleanup; stage and
errno remain unchanged. The board patch applies against the current framework
plus recovery patches. Removing the diagnostic additions reproduces the exact
previous UFS transport/GPT/APFS implementation. CI checks the new linked watch
thread, stage initializer -2, selected runtime argument and header source hash.
No handset checkpoint result is claimed before observing the timed snapshot.


Build 42617864877 completed the reference kernel, then the board compilation
reported thread_terminate_self undeclared in its sanitized IOKit export header.
The pinned original header declares extern void thread_terminate_self(void),
and the already linked current kernel contains _thread_terminate_self at
0xfffffff0072dc848. Add that exact C-linkage declaration for this statically
linked board code, with no stub or replacement implementation. Move the host
mock definition after the actual header include, so the host test also checks
that the header supplies this private API declaration rather than hiding it.
