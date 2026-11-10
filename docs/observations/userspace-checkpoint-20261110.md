# Post-Data userspace checkpoint, 2026-11-10

Handset photo 1794270772239.jpg from the all-stages candidate shows:
rootphase=2, pubState=1, completeWindows=2; dataStage=4, status=0,
readonly=0; ramReady=1, bytes=33554432; reads=57, writes=0, failures=0.
pubState=1 is publication success, not Data errno. The original APFS mount,
typed device ownership and writable mount flag checks passed. Zero writes
does not qualify actual RAM writes or persistence. The deliberate timer
stop is not a spontaneous I/O failure. No launchd success is inferred.

The Data helper returns after publishing status=0. Its argument copyouts
end at scratch+0x350, below the owner's environment area at +0x400 onward;
the selected native union mode sets init_envp=USER_ADDR_NULL. No evidence
supports changing that already successful path.

Add a separate initStage and execError, preserving Data's stage=4:
10 means the Data helper returned; 11 is the existing memorystatus snapshot;
12 means that operation passed or was not configured; 20 is original init
argument construction; 21 is entry to the original execve; 22 means it
actually returned, with the unchanged result in execError. The actual path
is recorded, including existing launchdsuffix/fallback selection. No exec
arguments, search order, security token handling or returned error changes.

The same opt-in timer samples process names for PID 1..64 into 384 bytes,
via the exact proc_name(int,char*,int) debugging API in the pinned original
xnu-10063.141.1 proc.h/kern_proc.c. That API owns/releases proc_find's
reference. There are no raw proc pointers, transition waits, signals or
process mutations. A bounded name sample is not a full process census or
proof that the named process completed startup. It is sampled only after
the opt-in timer, never when the diagnostic argument is absent/zero.

Host checks cover absent/zero argument, thread errors, all Data/init states,
the actual success tuple, PID sample bounds, process names and NUL-safe
truncation. The XNU patch applies after the current device series; removing
only the instrumentation exactly reproduces the previous source, including
the Data and Owner Cache function bodies. A real kernel build must verify
the new globals, original execve call and linked proc_name sample before
temporary handset boot. No userdata or persistent device changes are made.
