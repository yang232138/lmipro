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

All API replies and dispatch behavior remain unchanged. No user buffers, surface pixels or structure replies are read. Reports use bounded nonwaiting snapshots, with process groups separately accounted. The existing display caller capture remains available. First errors are candidates until their caller branch and downstream rendering gate are verified.

Next: bind the new checkpoint to the boot SHA, symbolicate the captured original callers, and correlate actual IOSurface creation/rendering with SwapBegin/End. Implement only the first proven required missing interface, then verify pixels and original swap retirement. A software-route rejection requires a concrete call/dependency trace; absence of swap calls is insufficient.
