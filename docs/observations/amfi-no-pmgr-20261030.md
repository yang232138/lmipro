# lmipro AMFI checkpoint, 2026-10-30

The user supplied photograph `1793311356118.jpg` after temporarily booting
`boot.img` SHA-256
`5f5cbdfb0350d8a345ce9fc8dc87e70ddeb38acb41642243935ffca0c908b35e`.
The screen shows `AMFI: No PMGR?` at `ConfigurationSettings.cpp:235`.

Device: Redmi K30 Pro Zoom Edition, Android identity lmipro, Fastboot product
lmi. The unchanged firmware FD SHA-256 is
`720481eb15e6cb3a6552c717ee6e912649a9c81eef18c95ab00dff7627530ee6`.
Payload run: https://github.com/yang232138/uefi-lmi/actions/runs/40961830558
Source: `856eac9bad80ebea9ae5a1ba7968b011983e0697`.
The raw source kernel is
`fa73fcb3a811ba2133ce2753e3292bc0757166b5903f791e3ecd32a319aa9026`,
built in run 40953560214 with `CONFIG_XNU_DT_PANIC_REGION=y`.

The previous post-PE_create_console checkpoint showed entry colors or text
followed by an approximately six-second reset. The no-reset payload then
reached this real AMFI panic. No system/root-volume boot is established.

Read-only checks of the current source kernel match the reference AMFI
read/OR/write sequence, ApplePMGRNub type name, provider dispatch methods,
and original provider read/write slots. Full executable hashes, imports,
object layout and all 340 vtable entries remain build-gated before testing
the opt-in lifecycle path on the handset.
