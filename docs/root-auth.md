# Root authentication: scoped bring-up

User transcribed `rootvp not authenticated after mounting @bsd_init.c:1098`
after actual temporary boot613d6869f71a48df2a1d6807d6e7a2274abbd9e4b8315c25c13b99352e576e17.
The exact XNU source reaches this panic only after vfs_mountroot returns zero
and VFS_ROOT obtains the root vnode. This is a post-mount milestone, not root
authentication or userland success. Filesystem name and real RAM I/O counters
have not yet been captured, so they must be reported by the next diagnostic.

Official userdata/security/UFS documentation and fixed original-APFS checkpoint
source were read. Even the unchanged restore OS volume reaches this same
authentication refusal in upstream checkpoints. Unsigned APFS or merely adding
root-hash IM4P files is insufficient. The original root-hash payloads require
personalization; manifest digest agreement is not signature authentication.
The documented owner path requires a genuinely signed policy, authenticated
block reads, readonly exact mounted-media identity and an appropriate transport.
The pinned build currently gates owner acceptance to verified VirtioMmioBlock;
RamBlock does not implement that authenticated path. Do not enable owner trust
or fictitious VirtIO hardware just to continue past this panic.

Stage now selected: add canonical VerifiedRoot and the existing official
eight-control source-kernel crypto probe. Independently qualify its real C
parser/RSA3072-SHA256 checks and negative controls, sign the exact complete
previously tested RAM seed, and verify all65526 real blocks on the host.
The kernel uses the unmodified official XNU crypto bridge/probe with this
measured policy/key/samples. The private disposable test key is never uploaded.
Public anchor/manifest hashes are hard-pinned in the board archive recipe.

Policy run41033006023 passed seven host check groups, ordinary ARM64/iOS17
cross-compile, actual signature verification and every block of the exact
268394496-byte test image. Artifact12967120609 SHA256:
531bd7a92dfea1c936dd2792fc36c6669ca3b6b220cc43a217854dccb6476b0f.
This is host evidence only. Native XNU crypto execution is pending.

At the existing autherr failure branch, the new diagnostic prints actual
filesystem name, RAM readiness/read/failure counters, crypto status and all
eight genuine crypto result values. The official boot argument
offline-owner-root-check=1 invokes the probe before autoconfiguration.
The original FSIOC_KERNEL_ROOTAUTH and failure condition stay in place;
OWNER_ROOT_AUDIT and OWNER_ROOT_TRUST are disabled. No root is accepted by this
diagnostic. The expected next screen may therefore still be a panic.
crypto=1 qualifies only the eight checks, never a working authenticated root.

Remaining work after evidence: implement authenticated RAM transport and exact
readonly mounted-device policy binding before accepting an owner root. A full
system needs real OS/SystemOS images and a suitable SM8250 storage path; the
tiny blank test seed is not a system. Do not reuse SM8550 register constants,
change physical windows, overwrite userdata or claim complete iPadOS boot.
