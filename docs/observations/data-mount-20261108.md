# tzinit exit 30 and volatile Data candidate

Handset photo 1794113271295.jpg shows tzinit[17] signal 0, exit 30.
This matches the framework's reference symlink EROFS checkpoint. The new
candidate retains the current UFS/union/boot-identity path and introduces an
independent, 32 MiB mutable APFS seed from the pinned original OS private/var.
RamBlock publishes it; the board mount checks the generated volume UUID,
typed RAM ancestry, exact seed SHA256 and capacity, and verifies the mounted
APFS device and read-write flag. Missing/wrong seed or failed mount stops boot.

This is board bring-up using namespace metadata, not signed owner-manifest
authentication. The standard authenticated owner Data guard is not changed.
The seed and rendered board patch are built together and shipped as evidence.
No physical write callback is supplied; Data is lost on reboot. Persistent
Data, keybags, UI boot and hardware write qualification remain unverified.
