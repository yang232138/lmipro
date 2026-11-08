# Confirmed mount failure and board boot identity

Photo 1794109724284.jpg from boot 9b6f51b08dc89f6cb6a80ad0674dde78867a4803beccf89873181fc54de0cc5d:
mount signal 0, exit 66, bytes 216. It reports failed to get filesystem info
twice, Data missing but not required in env 0, and failed to get
boot-manifest-hash / No such file or directory.

The fix derives SHA384(domain including NUL || actual 4096-byte SHNGROOT
header) after verifying the header SHA256 checksum and the exact two window
names, offsets, sizes, recorded hashes and total size. Pins come from the
previous full local image verification, not untrusted on-disk values alone.
Publish only after owner SystemOS mounting succeeds and both window services
exist; refuse any existing identity. Read back the published bytes.

This is a board boot namespace identifier, explicitly tagged
Lmipro.SHNGROOT.ImageSet.v1. It is NOT an Apple boot manifest or a signed
VerifiedRoot policy. Header checks and pinned recorded hashes do NOT verify
every installed data block. Content authentication and persistent writable
Data remain unqualified. No existing authorization checks are removed by
this change, and no disk write command is added.
