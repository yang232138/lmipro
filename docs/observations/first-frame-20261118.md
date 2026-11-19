# LMIPRO first real lock-screen frame

The only current milestone is an actual iOS/iPadOS 17.7.11 lock screen produced by the original user processes on the physical panel. Full Adreno 650/Metal, camera, audio and performance work is deferred. No partition flashing is authorized.

## Qualified baseline

Device commit 7583e5fbce4fb722174eb680d040099549153992, workflow run 44043639093, boot SHA256 4c44073f6498dca9ae9f9f79b45810123b513432e5002d2eefd6ec487ce21633. A hardware photo shows the 600-second checkpoint, SpringBoard 27, backboardd 58, Legacy registration and one successful backboardd open. The checkpoint deliberately panics to print diagnostics: this is evidence of reaching 600 seconds, not proof of indefinite or fully healthy operation.

No selector 4/5, validation or transfer was observed. Surface-frame count only increments after successful transfer and does not count IOSurface allocation.

## ABI and remaining proof

The selected donor is iPad7,1 / 21H461, ordinary ARM64, not an arbitrary iOS 17 ABI. Actual IOMobileFramebuffer code uses scalar selector 4 for SwapBegin and struct selector 5 with 0x508-byte input for SwapEnd. Original QuartzCore has a software-renderer branch. Its presence does not establish that backboardd reaches it or that all lock-screen effects work there. Unsupported selectors 57 (GetColorRemapMode), 27 (GetGammaTable), 50 (SetBrightnessCorrection) are observed; no necessary failure has yet been established by caller control flow.

The pinned GOP backend already has a bounded single-layer BGRA CPU copy and original retirement path. It requires an actual completed, CPU-readable surface. It is not a layer-tree compositor or an implementation of GPU command buffers. Do not return success for unimplemented rendering, fences or unsupported formats.

## Instrumentation

Gfx/v1 observes actual dispatch entry/return and final or early IOServiceOpen outcomes for IOSurface/IOCoreSurface and IOGPU/AGX/IOAccel class prefixes. S/G identify surface/GPU; suffix 0 backboardd, 1 SpringBoard, 2 other processes. It retains first error, last reply, actual successes, input shape, completed opens/errors and contention drops. Open errors use selector UINT32_MAX. Zero dispatch calls alone does not prove no GPU client was created. Matching failure before IOServiceOpen and per-task implicit close are outside this probe.

The target IOSurface creation path calls IOConnectCallMethod selector 0 at 0x1a85f6988; s0=entry/return/success counters distinguish actual allocation requests from unrelated activity. Successful allocation alone is not proof of rendered pixels.

All API replies and dispatch behavior remain unchanged. Gfx/v1 reads no structure replies. Reports use bounded nonwaiting snapshots, with process groups separately accounted. First errors are candidates until their caller branch and downstream rendering gate are verified.

## Physical Gfx/v1 checkpoint and next discriminator

Run 44190686547 / device 538849b7b84639c176ed45746f4c42cc0cf02997 / boot f30e8f78a1a2779b5ad956bd38aec0d3dc5d49eff5198d30ecb5c516c37f094e reached the intentional 600-second checkpoint. The photo shows S0 open 1/0, dispatch 1/1, success 1, last selector 13/status 0, and no selector 0. S1 is empty. S2 has 12/12 successful dispatches including 11/11/11 selector-0 events. All GPU rows and graphics drops are zero. No validate, transfer, copy, retirement or surface-frame event occurred. The other-process identity and exact user-client class were absent in v1; these events are not evidence of completed lock-screen pixels.

Target IOSurface 0x1a85fa1d8 selects method 13; 0x1a85fa1f4 calls IOConnectCallMethod with 40-byte output. It checks uint16 at output+0x22 against 1 at 0x1a85fa1fc..204. A bad reply can close and zero the connection at 0x1a85fa21c..224. Successful dispatch alone does not qualify this branch.

Gfx/v2 records bounded PID/name/class identity, explicit closes, reply byte count and this version field only for successful exact IOSurfaceRootUserClient method 13 with a 40-byte in-band kernel reply and no output descriptor or variable data. XNU xnu-10063.141.1 iokit/Kernel/IOUserClient.cpp is_io_connect_method supplies MIG inband_output as args.structureOutput and represents OOL output separately. No user pointers, descriptors, surface contents, original return values or renderer decisions are changed. Missing or malformed output remains explicitly unavailable. Reports put graphics and compact user-entry stacks first because v1's repeated counters pushed the stacks off the physical screen. These stacks remain entry samples, not proof of a blocked task or a successful software-renderer invocation.

## Physical Gfx/v2 checkpoint

2026-11-18 07:59 CST. Run 44200694360, boot SHA256 afa6b6fe266feb7dbb149ab58cb04d525cf56c6d781df9afa5a86d955f4c2b33. Backboardd PID 58 returned 40 in-band bytes with version 1 from IOSurfaceRootUserClient selector 13. It opened once, never explicitly closed, and issued zero selector-0 requests in this observer. SpringBoard has no matching calls. The other group has 11 successful selector-0 requests, with mediaserverd PID 29 as the last caller. GPU open/dispatch counts and graphics drops are zero. The photo shows diagnostic bars and text. The submission and copy counters were below the visible panel.

Three pinned wrapper return PCs agree on a cache slide of 0x14aac000. In the selector 50 stack, QuartzCore returns to update_framebuffer_locked at 0x1895987e8, is_ready at 0x1895108a4, set_enabled at 0x189510828 and render_for_time at 0x189521028. The four preceding instructions are the expected direct/indirect call sites. Reaching update_framebuffer_locked from is_ready requires a successful return from the preceding SwapWait call. The post-selector-50 branch tests configuration bits and does not use w0. TID 577 sampled CA::Render::Server::server_thread's Mach receive loop, whose sampled call returned success. The sample does not identify the contents or sender of that message. The other samples cover dispatch, XPC, logging and thread control. The eight slots recorded 36,909 capacity skips. A missing stack is not reliable negative evidence.

## CA receive and swap observation

Target QuartzCore server_thread compares Mach header msgh_id at 0x189444cf4..d04 with the 40000..40199 range. The new opt-in hook copies only the received kernel message ID and audit sender PID before copyout releases the message. It records the real receive result after copyout to backboardd, bounded to four ID/sender pairs plus totals and drop counters. The observer does not inspect payloads, change results, or claim that a delivered message was accepted as a CA transaction. The checkpoint places this observation and selectors 4/5, validate, copy and retirement on screen before optional stacks.

## Physical RenderIPC checkpoint

Run 44222323502 reached 600 seconds. Both swap selectors had zero entries and returns, with zero drops. Legacy registered and backboardd opened its client. Validation, transfer, copy, retirement and surface counters were zero. Backboardd PID 58 was the sender for all 27 successful receives in the selected range: 40001 x23 and 40002 x4. There were zero other rows, drops or failed copyouts, and no observed receipt from SpringBoard PID 27. This is evidence for that ID range and receiver, not proof that SpringBoard made no send attempt or that other IPC failed.

The follow-up adds SpringBoard-only counts at the entry and every actual return of XNU's mach_msg_trap_send, after the existing trap has built its kernel-local header. It retains only scalars and a first real error, and shows the count next to the original receive results. The numerical destination is a name in the sender's IPC namespace. Reported success means the selected Mach send returned success. Earlier trap argument failures are outside the hook.

An independent 15-second-limited SpringBoard stack slot uses XNU's THREAD_TAG_MAINTHREAD, set on the fork-created main thread in bsd/kern/kern_fork.c. It takes a current-thread syscall-entry sample and separately observes a matching actual return. The build retains wait limits and the existing real-frame termination check. The new observations aim to identify the point before the missing SpringBoard receipt without changing userspace, graphics or Mach return values.

## Physical SpringBoard checkpoint

Run 44227322929, device revision `221948a93b40811105ca2e76aae98bb82a8a3a35`, boot SHA256 `098d6991b83dd8fc4612eb56107daa12f83a0209eb791453f50514e68c18f65d`. The physical screen was photographed at the 600-second checkpoint on November 18, 2026 at 12:27 UTC+08:00. It showed diagnostics. Both swap selectors had zero entries and returns. Validate, transfer, copy, retirement and surface counters were zero. backboardd completed IOSurface selector 13 (40 bytes, inline, version 1); neither compositor process used selector 0 in this trace.

All 28 receives in range were backboardd self-sends: 40001 x24 and 40002 x4. There were zero failed, unlisted or dropped observations. SpringBoard had zero entries at the selected Mach send hook. That hook requires a parsed header with ID 40000–40199, so absence leaves other IPC and earlier encoding/dispatch paths open.

At second 309, the main thread of SpringBoard (PID 27, TID 295) sampled `bootstrap_look_up` through `_bootstrap_look_up3`, libxpc, and `mach_msg2_trap`. Three pinned return-address anchors agree on slide `0x4d04000`. The seven caller frames land after actual call instructions in the 21H461 cache. The sampled Mach call returned `0`. The original libxpc code separately reads the XPC reply's `error` field, validates successful replies, and extracts `port` in the bootstrap wrapper. The photo therefore proves a transport return and a lookup call path. It does not identify the requested service, its lookup status, a returned port, continued waiting or an upstream failure.

Next: correlate the exact request service name, reply status and port outcome with its caller above `bootstrap_look_up` before deciding which interface needs modification. Retain the original SwapBegin/End and real frame counters. A software-route rejection requires a concrete call/dependency trace. The zero frame counters do not exercise the CPU copy implementation.


## One-shot bootstrap diagnostic

The v1 `lmipro-bootstrap-at=300` probe arms on the first eligible SpringBoard main-thread syscall at or after second 300, then observes the next original `_bootstrap_look_up` invocation. This is a fresh boot and may see a different call from the old second-309 sample. Both cache UUID and live instructions must match before arming. Existing debug state and processes that disallow attach cause refusal. The breakpoint state is restricted to the selected thread and is removed at its matching return or a pairing mismatch. The board does not write EL0 code or registers. The selected lookup report replaces the general stack sections only on this diagnostic boot.

At the entry, record LR, a validated frame-chain hint, the task's actual executable Mach-O base, mapped cache/libxpc bases, time and bounded `copyinstr` of the service name. At the original return instruction, read the real `w0` and only read the output name if the original store site was reached and `w0 == 0`. The output must still match the store value and refer to a live send right in the task's IPC space. `phase=3` pairs the wrapper entry and return. `phase=2` means return was not observed; a `mach=1/0` plus scheduler state 3 or 4 is evidence of an outstanding observed Mach call with the thread waiting at the checkpoint. A missing return alone does not prove waiting. `phase=1` is armed with no observed entry, `phase=4` is refused, and `phase=5` is invalid pairing. The original swap/CA counters remain visible.

### Actual v1 checkpoint and exact in-flight fallback

Run 44249472662 reached the 600-second physical checkpoint. It reported `phase1 diag0 p27 t294 lost0`: the kernel accepted the thread's breakpoint state and recorded no entry hit. Service name, wrapper return, port and caller were unobserved. This does not distinguish late arming, a non-reproduced call and a missing hardware hit. The selected CA range had 32 backboardd self-sends and no SpringBoard send; swap and copy counts were zero.

The `SB lookup/v2` report retains a single lookup. At the exact `mach_msg2` trap PC a bounded saved-FP walk checks seven pinned return addresses, two frame sizes and the live register-save instructions. The interface frame supplies saved `x19` (name) and `x23` (output address), and the wrapper frame supplies caller LR and original SP. It then pairs a subsequent store and actual wrapper return if those breakpoints hit. `via=2` means the timestamp is sampled Mach entry; `via=1` means direct wrapper entry. `arm` records the actual arm time, and `chains` counts checked candidates. No output port is read until store and successful wrapper return are observed.


### Actual v2 checkpoint and earlier exact-chain capture

Run 44254364267 reached 600 seconds on the handset (photo 1794986322150.jpg, SHA-256 `d5c19c8f8f59d8fa084b37ec764e91984c1128df74d1f25a3f65f2a2cc7505f2`). It recorded `phase1 diag0 p27 t294 lost0` and `arm=432 via=0 chains=3`. The three candidates passed the trap-PC filter but did not yield a validated lookup. The first eligible arm in this run was later than the earlier boot's second-309 sample. The absence of a match supplies no name, status, port, caller, or evidence of a blocked lookup. Swap and frame counters stayed zero, and all 27 observed selected-range messages came from backboardd (40001 x23, 40002 x4).

The v3 diagnostic sets `lmipro-bootstrap-at=0` in its temporary image, qualifying the cache on the SpringBoard main thread's first eligible exact trap PC. It scans only this PC and the seven original ARM64 frames, and configures two per-thread breakpoints only after recovering one matching request. `ready` is the qualification time; `checks` counts PC-matched candidates; `depth` is the maximum number of successive matching saved return addresses. `phase1` now means scanning without a captured request and without a configured entry breakpoint. `at` is a sampling time inside the original lookup. A request captured on a fresh boot is a candidate for the prior sample, and must be correlated through time, caller and service. The actual wrapper return and original port store checks are unchanged. The observer does not change IPC or graphics behavior.


### v3 result: lookup complete; follow the screen-info RPC

Run 44259212767 reached the 600-second checkpoint (photo 1794988674678.jpg). SpringBoard PID 27/TID 295 at second 176: `com.apple.backboard.display.services`, `ret1/0 store1 port2/3b03`. The original wrapper returned success and the task had a live send right. The earlier second-309 record did not retain the service or outer caller, so this resolves the observed v3 call without assigning the older call an identity. 21H461 cache slide `0x15b68000` resolves the caller to `__BKSServerPortHelper`, and the outer frame to `__BKSDisplayServicesGetMainScreenInfo`. The matching launchd declaration belongs to `com.apple.backboardd`. LegacyFB remained registered; swap and copy counts stayed zero, and 27 observed CA messages were backboardd self-sends.

The next synchronous operation is `__BKSDisplayGetMainScreenInfo`, Mach request 6001011 (24-byte request, 6001111 60-byte success reply). The matching backboardd Mach-O contains the routine at unslid `0x100007d20`. v4 follows exactly one matched client call after the real lookup, and records entry, the selected Mach trap, `mach_msg` return, MIG return and the four unmodified 32-bit floating-point words and two flags on success. The BKS screen report retains the upper caller, cache and executable bases for later symbolication. Its state is 1 awaiting exact entry, 2 observed entry, 3 observed MIG return, or 4 an invalid probe match or setup. A live right does not prove this RPC was delivered or returned real display parameters. No IPC behavior or display response is changed.


### Physical screen-info success; next boundary is UIKit startup

Run 44265528905 reached 600 seconds (photo 1795000139814.jpg). The original screen-info RPC entered at second 161, and the matched Mach trap took 43,688 ms. The trap, mach_msg, and MIG statuses were each 0. The real returned words decode to 1080 by 2400, scale 1, rotation 0, flags 0/0. This transaction completed; the reason for the measured delay is unknown. The recovered upper PC `0x18faf8884` with slide `0x49c000` is the first return site of `BKSDisplayServicesStart` calling the screen info wrapper. LegacyFB was registered, but SwapBegin/End, validation, transfers, copies, and surfaces remained zero. All 27 observed CA messages in the selected ID range came from backboardd itself.

The version-matched UIKitCore image calls `BKSDisplayServicesStart` from `__UIApplicationMainPreparations` at unslid `0x18a3ba7e0`. On success the BKS wrapper calls GraphicsServices `GSSetMainScreenInfo` and validates positive screen dimensions before its success return. The v5 diagnostic verifies the actual two saved frames and eight live instruction words, then uses one owned per-thread breakpoint at a time for the same UIKit frame. `UIKit init/v1` step 1 is the BKS Start return, steps 2 through 4 are preload initialization/display/device returns, step 5 is UIApplicationInitialize return, steps 6 and 7 bracket window server start, and step 8 is UIApplicationInstantiateSingleton return. These are ordered checkpoints, not proof of a successful render context. A nonzero diag means the probe itself could not finish its setup or pairing. No service replies or user registers are changed.
### 20:42 handset checkpoint

Run 44293549792 reached the 600-second checkpoint. LegacyFB registered. No backboardd framebuffer open, selected CA receipt, swap, or copied surface was recorded. SpringBoard PID 27, TID 294 qualified at second 139. After 38 candidates the longest observed prefix was six frames. The observer never recovered a complete lookup. The wrapper result, screen-info RPC and UIKit progress are unknown for this boot.

When the lookup report is active and backboardd has no recorded render receipt, the checkpoint now prints existing backboardd activity and committed-exit PIDs and the existing SpringBoard main-thread sample. A zero PID records no observed event. A negative PID indicates lock contention. Different activity and exit PIDs must be interpreted separately. A main-thread sample describes its recorded time. A new phone observation is required to establish which case occurred.

### 05:17 handset checkpoint and BKS shell caller

Run 44391057707 reached the 600-second checkpoint. LegacyFB registered, backboardd opened it once successfully, and all 27 selected CA messages were backboardd self-sends. Swap and surface counters remained zero. SpringBoard PID 27, TID 294 issued request 6001011 at second 177. Its Mach trap took 282,829 ms, the trap, mach_msg and MIG each returned zero, and the original result was 1080 by 2400 at scale 1 and rotation 0. The bootstrap lookup returned zero with port `0x3f03`.

The observer captured caller `0x19042abb8` with the shared cache mapped at `0x180d98000`. In the 21H461 donor cache, the unslid return site `0x18f692bb8` belongs to `-[BKSSystemShellService _activateServerConnectionWithIdleSleepInterval:]` and follows the call to `BKSDisplayServicesStart`. The UIKit-only caller check rejected it with `ENOEXEC` before arming. The follow-up pairs the two exact shell return sites with their stack pointers and checks the activation's saved LR at its return. In `BKS shell/v1`, step 1 reports the original BKS Start return value and step 2 confirms the enclosing activation returned.

### 06:00 handset checkpoint

Run 44401347424 observed `BKS shell/v1 arm1 step2 diag0 bks1 at246`, with a successful screen-info RPC and trap duration of 63,934 ms. LegacyFB open succeeded and all 27 selected render messages were backboardd self-sends. There was no observed swap or copied surface by 600 seconds.

The matched return address `0x1a2bf5ef4` resolves to `-[BKSSystemShellService initWithConfigurator:] + 0x118` with slide `0x13564000`. The remaining normal path releases an ARC object, checks the stack canary and returns the receiver. With the verified initializer frame and return location, step 3 records the paired initializer return. `object1` records a nonnull result at that breakpoint. Read `next` as the captured saved caller PC and compare the time and identity on the existing `SB main` sample with the BKS record. The checkpoint displays that sample when render receipts are present as well.

### 06:45 handset checkpoint

Run 44408703079 observed the nonnull BKS shell initializer return at second 204, to `-[FBSystemShell _initWithOptions:] + 0x2fc` in 21H461. The SpringBoard main thread ran later and the sample at second 334 resolved to `mach_port_mod_refs` inside `-[NSXPCConnection initWithListenerEndpoint:]`, called by `_LSCopyServerStore`. The trap returned zero. The XPC object, store response and database creation remained unobserved, and no swap or copied frame was observed by 600 seconds.

The next checkpoint follows one live call for the same main thread after a successful shell initializer. It matches the observed Mach trap stack and verifies the three paired return sites. In `LS store/v1`, stage 1 records the matched reference-count operation, stage 2 the XPC constructor result, stage 3 the boolean store return, and stage 4 the database object. Interpret the result fields only once their corresponding stage has been reported.

### FrontBoard-to-UIKit handoff

Run 44416270360 confirmed the BKS shell initializer returned a nonnull object into `-[FBSystemShell _initWithOptions:]`. The LaunchServices observer never matched. The same-second main-thread sample was a bootstrap Mach trap with a successful transport return. It retained no service or wrapper result.

The 21H461 SpringBoard executable branches into `_SBSystemAppMain`. Its call at `0x1cfa38d5c` targets `_FBSystemShellInitialize` and returns to `0x1cfa38d60`. Its call at `0x1cfa38df0` targets `UIApplicationMain` and returns to `0x1cfa38df4`. The former returns an Objective-C object. The latter takes argc, argv and two class names and returns an integer.

The boot argument `lmipro-frontboard-boundaries=1` selects the paired handoff observer. `SB handoff/v1` reports stages 0 through 5 for no confirmed arm, arm, FrontBoard entry, FrontBoard return, UIKit entry and UIKit return. The two pairs carry observed caller addresses and millisecond timestamps. A stable pending syscall accompanied by state 3 or 4 records the sampled user-side wait location, with up to three saved callers. A dropped or raced snapshot suppresses the pending result. An arm without an observed entry is inconclusive.

Run 44439589463 captured the main thread waiting at the ARMv8.1 libdispatch
receive call at `0x18f589534` before FrontBoard returned. The saved stack ends
at libdispatch. The separate LaunchServices sample belongs to a port-reference
operation and does not identify the outstanding request.

The opt-in `lmipro-frontboard-boundaries=2` mode adds one `SB ipc/v1` record to
that existing checkpoint. It requires the matching 21H461 cache, verified
instructions and exact wait stack while the FrontBoard boundary remains open.
The `caller` values continue the live frame chain above libdispatch. They are
runtime return addresses. Subtract the cache slide and four bytes to find each
call instruction.

`reply` is the Mach receive name. `sync` is the destination name supplied for
synchronous priority inheritance, when present. `pair1` establishes an observed
send-only trap on the same call frame and dispatch message object, with the
matching message ID and reply port.
Its `id`, `remote` and `local` are the arguments submitted to Mach.
`sret1/<status>` records the inline send attempt's kernel result. For an
unpaired send, `hdr1` means those three fields are a user-side header snapshot
with a matching reply port. A deferred send may have run on another thread.
Its transport result, receiver identity and delivery remain unknown. A
successful inline send alone does not prove server receipt or reply delivery.

No functional fix or new boot image has been made for this request.

Run 44452864617 reached the 600-second checkpoint on the handset. The
checkpoint exhausted the kernel stack while formatting the first Swap line.
The faulting store at `0xfffffff0072a98c8` is in `__doprnt`. Its saved SP plus
`0x90` equals the fault address. The handset photo contains no IPC snapshot.
Both checkpoint formats already print at most 210 display-detail bytes. The
source now allocates 211 bytes for that buffer, including the terminator,
instead of 2560. The transaction observer and report formats are unchanged.

### LaunchServices blocking store server

Run 44457519648 captured the paired SpringBoard send and pending receive. The
request reaches `_LSCopyServerStore` and calls
`getServerStoreBlockingWithCompletionHandler:` through an anonymous listener
endpoint. The Mach request ID is `0x40000000`. The client namespace names
`0x580f` for the destination and `0x1e03` for the reply. The paired send
returned zero. The checkpoint does not identify a server PID or record server
receipt, wait completion, a store result, or a reply.

In the 21H461 cache, `LSDatabaseBlockingFetchServer` exports an
`LSDatabaseBlockingFetchClient` on accepted connections. The handler at
`0x187d7c9c8` selects an unbounded database seeding or manual-rebuild group
wait. It calls `syncRead:` at `0x187d7ca84` after that wait returns. The
read callback calls `__LSServer_GetServerStoreForConnectionWithCompletionHandler`.
This routine can call the XPC completion block with a store, node and error.
These control-flow facts are from the pinned cache and do not describe the
unobserved server execution in run 44457519648.

`lmipro-ls-server=1` selects one request handled by a matching-cache `lsd`
thread whose local NSXPC connection names a SpringBoard peer. The report at
the existing checkpoint contains three bounded lines.

- `LS server/v1` reports arming, rejected handler entries, setup status, the
  actual receiving PID and TID, the peer PID, and a diagnostic error.
- `seed in/out` reports entry and matched return for the selected group wait.
  `ret` is valid with `out=1`. `manual1` identifies the manual-rebuild
  branch. `read in/out` reports entry to `syncRead:` and an observed database
  copy return. `db1` means that copy returned a nonnull object.
  `cb1` means the original reply block ran from a verified store call site.
  `args` bits 0, 1 and 2 represent nonnull store, node and error arguments.
- `reply xpc1` means Foundation handed a reply to libxpc for the selected
  connection. `port` is an extracted local server-side send-once name.
  `mach in/out` and `ret` report a matching send and its kernel result.
  The ports in the client and server prints are in different namespaces.
  A successful server send does not establish that SpringBoard consumed
  the message.

The observer follows the selected server thread. Unobserved or cross-thread
work remains unknown. A host test checks admission, wait pairing, database
result, callback identity, and send result pairing. The validation goal for
a dedicated boot is to locate the first server milestone that remains open.
Data volume and database investigation requires evidence that the database
wait or read actually blocks.
