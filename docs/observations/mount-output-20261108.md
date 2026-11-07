# Current mount failure, 2026-11-08

User-supplied Desktop/union1-boot.img is byte-identical to uefi-lmi run
42333251284 (overlay dd3d69e2f6700ff07018b6cb697889ff33708246).
SHA256: c1a30dc17b86594d4cf974d7cf9fd8628401acde4e0bf3879edda9bcc52e81a4.
Framework: 2440fb161e5caf8a27dcfe72c9ce9aaf01e57871.
Device: 26a16e43f0a708e90c555a3bbbbfd0d27db413a7.
Photo 1794022351093.jpg shows mount[3] exited, signal 0, exit status 66.

The exact kernel enables the owner SystemOS union composition and lacks
XNU_OWNER_BOOT_MANIFEST. The ADT has no boot-manifest-hash. Reference HVF
checkpoints reproduce this status when the hash is absent, but this handset
has not yet supplied mount stderr, so that cause remains a hypothesis.

This device patch retains at most 640 bytes of mount PID 3 stdout/stderr
attempts, from at most 16 iovecs and 640 bytes per syscall. It neither consumes
the uio nor changes the write return value. copyin occurs outside the bounded,
nonwaiting append guard. The original critical-process panic path prints the
captured tail and real signal/status, instead of hiding stderr.
All root/union, UFS, firmware, trust-cache and boot arguments remain those of
the exact baseline. No fabricated boot-manifest property is supplied.

Next hardware observation must show MOUNT LOG v1 and its body. This is an
evidence build, not a qualified repair or complete OS boot.
