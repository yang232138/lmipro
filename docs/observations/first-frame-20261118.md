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

Next: bind the new checkpoint to the boot SHA, symbolicate the captured original callers, and correlate actual IOSurface creation/rendering with SwapBegin/End. Implement only the first proven required missing interface, then verify pixels and original swap retirement. A software-route rejection requires a concrete call/dependency trace; absence of swap calls is insufficient.
