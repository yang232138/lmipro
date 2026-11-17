# UniXNU-lmipro

Board repository for Redmi K30 Pro / K30 Pro Zoom Edition (`lmi` / `lmipro`,
Qualcomm SM8250), using the pinned iPadOS 17.7.11 / 21H461 source/ABI profile.

The current sole milestone is a real lock screen from the original iOS/iPadOS
17.7.11 user processes on the physical panel. Full GPU acceleration, camera,
audio and performance work is deferred. The qualified hardware baseline reaches
the 600-second diagnostic checkpoint with SpringBoard/backboardd activity and a
successful Legacy framebuffer open; no real UI frame has been demonstrated.
See [the current evidence and next action](docs/observations/first-frame-20261118.md).

From the pinned UniXNU checkout:

```sh
./unixnu defconfig lmipro_defconfig -D ../lmipro
./unixnu fetch -D ../lmipro
./unixnu status --json -D ../lmipro
python3 scripts/porting-record.py check --device ../lmipro
```

The board package appears as `kits/lmipro` in the resolved tree. The PMGR
archive recipe consumes a freshly produced reference candidate, its JSON and
the verified original kernelcache; the CI workflow prepares these inputs.
Keep the runtime PMGR mode off until running its explicit lifecycle test.

UEFI and kernel tests use `fastboot boot` only. Userdata geometry, functional
storage/root, peripheral drivers and a complete system boot remain unfinished.
