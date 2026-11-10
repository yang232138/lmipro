# Display handoff qualification after genuine userspace service startup

Handset photo 1794279588945.jpg on run 42742941670 records launchd plus logd,
UserEventAgent, runningboardd, configd, mediaserverd, powerd and backboardd.
Volatile Data completed 679 writes and 1944 reads with zero transport failures.
The 600-second stop was an explicitly requested diagnostic panic. These facts
qualify userspace process creation and volatile I/O, not graphical startup.

The existing board selects no GopFramebuffer archive. Its inherited original
IOMobileGraphicsFamily and IOSurface classes are present in the OS profile, but
published display instances have not been observed. The reserved physical
framebuffer range is not proof of current GOP geometry/format/ownership.

Reviewed GopFramebuffer cf3b42979a9c44219e7bb149543e6fc3388b58ed and sheng recipe
516a780473a2ac59c8b438e132bb60204dda9e22. The recipe bundles SM8550 board power,
touch, audio and GPU probes. It must not be selected unchanged for SM8250.
NativeLegacyDisplay requires an audited original 21H461 private ABI plus board
hooks, IOSurface descriptors, publication and scanout preparation. NativeGOPPresenter
alone is not the original iOS compositor's display service.

Add an opt-in read-only observer to the existing watch. With
lmipro-data-watch=600 and lmipro-display-watch=1 it samples PE_current_console
and currently published IOMobileFramebuffer, IOMobileFramebufferLegacy,
IOSurfaceRoot and NativeGOPPresenter instances, then explicitly stops for a photo.
No service waits, service creation, mappings, display writes, DPU/DSI registers,
framebuffer ownership transfer, storage changes or error substitutions occur.
All original watch details remain available when lmipro-display-watch is absent.

PE_Video and IOService API checked against xnu-10063.141.1 pexpert/pexpert/pexpert.h,
pexpert/arm/pe_init.c and iokit/IOKit/IOService.h. The arithmetic layout status
does not qualify actual memory mapping or pixel format. Registry -1 is allocation
failure, -2 is an invalidated sample, and 33 is a capped count, never absence.

Next: capture actual geometry and published services from this new artifact,
then implement only the board display bridge justified by those measurements.
Source/host checks and a successful build do not establish handset display output.

## Handset result and census correction

Photo 1794286897556.jpg of run 42752208211 confirms base 0x9c000000,
1080x2400, depth 32, stride 4320, offset/length 0, scan span 0x9e3400,
rotation 0, scale 1, display 1, and PE pixel format B8G8R8.
IOSurfaceRoot has one matched instance; launchd and backboardd still exist.
MobileFB, LegacyFB and GOP returned -1. These are ambiguous, not allocation proof.

Pinned xnu-10063.141.1 iokit/Kernel/IOService.cpp getMatchingServices calls
copyExistingServices. Its ctx.result starts NULL and remains NULL on no match;
it constructs a collection iterator only when current is non-NULL. The previous
observer incorrectly labeled every null iterator as allocation failure. Its host
fixture always returned an empty iterator on no matches and missed this contract.

Correct the observer to query all matched IOService instances, then filter with
the public metaCast API. The observed IOSurfaceRoot proves this population is
nonempty on this baseline. An available complete scan may now return genuine zero.
Keep null query results as -1 query unavailable; preserve invalidation as -2;
report scan truncation as -3 (4096 inspected) and count saturation as 33.
Do not infer absence from an incomplete or unavailable scan. No retry or wait.
Host tests now include nonempty populations containing zero target instances,
the exact 4096 boundary, overflow past the scan cap, and reference cleanup.
New census remains hardware untested until the new hash-qualified image is booted.

## Corrected census on handset

Photo 1794289338194.jpg of run 42761115816 records complete counts:
MobileFB=0, LegacyFB=0, IOSurface=1, GOP=0. Console geometry is unchanged.
Launchd and backboardd remain present. This qualifies absence of matched
framebuffer instances at this sample, not absence of class code or all UI jobs.
The current board selects no GOP archive; connect the pinned original Legacy
backend with a board-owned startup wrapper. First qualify init/start/publication
and retained framebuffer mapping. Transfer and GPU remain disabled until their
ownership and producer-completion requirements are qualified on this board.

## Board-only original Legacy initialization candidate

The pinned GopFramebuffer implementation is used unchanged. The board recipe
audits the actual current-job flag-off reference: full original graphics text,
358 slots, abstract leaves, original constructor/destructor chain, class metadata
arguments and IOSurface getter instructions. It compiles the owned backend and
board wrapper only. No sheng peripheral package is linked. Runtime additionally
checks the loaded original class extent before creating the owned subclass.

The existing post-IOKitBSDInit hook invokes startup with lmipro-display-init=1.
Original family and IOSurface load statuses are preserved. The provider owns a
single GOP resource whose geometry and RGB spelling must match the handset.
Original probe/start/registerService run with the original generic user client.
Every failure reports its first stage; cleanup releases failed resources. Active
services and their mapping remain owned until this test reboots. A 600-second
checkpoint retains the census and adds actual init/client/mapping counters.

This first test enables only GOP mapping and publication. Frame validation,
transfer, retirement, GPU offer and timer-vsync are disabled. The board's
prepare-scanout hook refuses transfer until producer and console ownership are
qualified. This is not a claimed graphical startup.

## Published Legacy framebuffer observed on handset

Photo 1794298617391.jpg of run 42771882999 records MobileFB=1, LegacyFB=1,
IOSurface=1, GOP=0; Legacy initStage=8 status=0 registered=1 guard=0,
mapping=0x9c000000 bytes=10368000 active=1. Four connection calls were counted,
with the last type=0/status=0; this does not prove all four returned success.
Validate/transfers remain zero in the explicitly publication-only test.
Launchd, backboardd and other services remain present. No GUI is observed.

Next observe genuine IOUserClient callExternalMethod entry/return, filtered to
the original IOMobileFramebufferUserClient class. Preserve its original ipcEnter,
externalMethod dispatch, ipcExit and return value. Count bounded selector/status
and scalar-input snapshots without replacing the original client vptr or methods.
Do not infer the first nonzero IOReturn is the root cause; some are expected.

## SpringBoard observed and original display query qualified

Photo 1794314831783.jpg of the explicitly repeated run 42789076216 shows
SpringBoard PID 26 and backboardd PID 58, but no GUI. Original display method
entries/returns are 2/2 with zero drops: selector 8 succeeds, selector 28
returns e00002f0. Its caller PID 94 is outside the <=64 process sample.
The genuine table at 0xfffffff005f354c0 is indexed with selector*24 in three
original externalMethod paths. Selector 8 calls the dimensions getter (slot
249); selector 28 calls getDotPitch (slot 188), with one scalar output. The
unchanged pinned backend returns kIOReturnNotFound when the actual provider
lacks dot-pitch. This board provides no measured pitch property. Do not invent
one or replace the failure with success: causality for the UI is not established.

The next opt-in lmipro-ui-watch=1 observer copies only the exact current process
name and scalar execution events for SpringBoard/backboardd using the existing
entry/return/exception hook. proc_selfname in pinned XNU copies current_proc's
name without proc_find, allocation or transition waits. Record thread IDs to
avoid treating aggregate latest events from different threads as paired waits.
Two bounded actor records use one CAS attempt with explicit contention drops;
a new PID resets that actor's old events and increments its generation.
The existing PID1 path, original syscall results and all kernel patches remain
unchanged. Display calls now retain a bounded copied caller name as well.
The 600-second observer still intentionally stops. Its output qualifies activity,
not a thread backtrace or GUI. All presentation gates remain disabled until
real frame submission/producer ownership can be qualified. Storage source,
SystemOS, Owner Cache, Data seed and RAM block bindings are unchanged.

## Original mode validation failure on handset, 2026-11-11

Photo 1794343572693.jpg (run 42822768304) shows real backboardd calls:
17 entered/17 returned, zero drops, 15 distinct selectors; only four sorted
rows were requested. Last selector 50 returns e00002c7; the first error remains
selector 28 dot-pitch e00002f0. These are not established as fatal UI failures.
SpringBoard PID 26 and backboardd PID 58 each remain observation generation 1.
SpringBoard's captured real EL0 BRK PC is 0x1bd7bc744, ESR class 0x3c.

Copied only the SystemOS APFS window from the existing local SHNGROOT image,
source opened read-only. Its complete SHA256 matches the OS-profile input:
b4908cde41d23e6bc05e6af51a2c9eeb00cf0f863a6a18b1600c96a3fd70414f.
Extracted unchanged main cache and selected code subcaches using official
portable 7-Zip 26.06; cache UUID 590d839a82d230f1a17d0cc053420f73 matches
the profile. Local symbols use unsupported ZBM resource compression; no claim
that those symbols were decoded. Exact original instructions suffice here.
The photographed kevent_id return PC 0x1e9b6c8c0 matches the unique original
375/SVC wrapper return 0x1cb7808c0, giving page-aligned slide 0x1e3ec000.
The actual BRK then maps to 0x19f3d0744, inside the original FBSDisplayMode
initializer at 0x19f3d0500. All 584 bytes match the already-qualified SHA256
ca4857e6617670f2369a0a6eb68be09e29135ce5dfda19ac1ea829229f0cc0ec.
Disassembly follows invalid-mode validation to error policy 1 and BRK, not
the separate no-main-CADisplay assertion. The exact invalid input was not in
the previous scalar-only photo: do not claim scale alone is proven causal.

The current exact payload ADT lacks /chosen display-scale and display-rotation;
GOP handoff reports scale 1, rotate 0 and real 1080x2400 scanout. Introduce
opt-in lmipro-display-mode=1: publish only missing words matching that actual
handoff before original driver registration. Never replace conflicting/malformed
existing values. Verify reads after publication and remove only newly created
words on failure. This is an explicit 1:1 logical mode policy, not physical
density, a fake panel mode, refresh rate or an Apple hardware identity.
Original userspace and its mode validator remain byte-for-byte unchanged.

Add one read-only source hook at an actual EL0 BRK. The existing exact-name
SpringBoard observer records the first PC and x19..x28 from the kernel-owned
saved register array. No user-pointer dereference, user-code change, exception
suppression, allocation, transition wait or invented return. At the audited
mode failure, x24/x25/x26 retain original preferred-scale/width/height under
the real AAPCS preserved-register ABI; their raw values will diagnose a repeat.
The hook is removed exactly to reproduce the prior complete sleh.c fixture.
Existing PID1, Data, UFS/GPT/APFS, Owner Cache, authentication and presentation
gates are preserved. First qualify the mode result; actual frame transfer and
graphical startup remain separate, unverified milestones.
