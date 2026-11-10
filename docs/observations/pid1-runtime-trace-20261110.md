# PID 1 runtime evidence after the 120-second snapshot

Photo 1794274094090.jpg from the identical kernel with watch=120 still shows
Data stage4/status0/readonly0, reads57/writes0/failures0, initStage22,
execError0, /sbin/launchd and only PID1 in the bounded name sample. This
qualifies the original exec call's return, not original launchd LC_MAIN.
Do not alter the already successful mount or display a logo as OS proof.

Add opt-in PID1-only SVC entry/user-return, non-SVC user exception and EL0
interrupt PC samples. The original SVC dispatch, return-register preparation,
fault handling and interrupt dispatch remain unchanged. The event helper
does not allocate, read user memory, send signals or modify saved registers.
It uses the pinned original proc_selfpid API. A try-append guard drops on
contention and counts drops; snapshot copying also never waits for that guard.
The helper is inactive unless the existing diagnostic argument enables it.

Record raw call numbers, PCs, arguments, user return x0/CPSR and exception
ESR/FAR. The first BSD carry-flag error is retained without assigning it as
the boot failure: normal discovery can return expected errors. Entries and
returns are not paired transactions across threads or nonreturning traps;
different totals alone do not prove a blocked call. User page faults are
expected VM events, not automatically I/O failures. EL0 interrupt samples
give real user PCs, which still need mapping before claiming LC_MAIN.

Only sleh.c and systemcalls.c receive LMIPRO_USER_TRACE=$SHENG_PMGR; reference
mode0 has no callbacks, final mode1 includes them. The existing mode stamp
invalidates these two objects when switching modes, preventing stale flag0
objects. CI must retain both actual compile records with flag1, source hashes,
new header hash, callback sites and original exec-forwarding/result proof.
The unchanged framework revision has this explicit local build-script
adaptation recorded; it is not represented as a clean upstream checkout.

Strict C++17 tests cover runtime disabled/PID filtering, exact values, first
error preservation, busy-path drops and bounded formatting. Removing the
hook additions reproduces the exact previous syscall/exception sources.
No UFS, GPT, APFS, SystemOS, Owner Cache, seed or storage binding changes.
