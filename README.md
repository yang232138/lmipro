# UniXNU-lmipro

Board repository for Redmi K30 Pro / K30 Pro Zoom Edition (`lmi` / `lmipro`,
Qualcomm SM8250), using the pinned iPadOS 17.7.11 / 21H461 source/ABI profile.

Temporary boot reaches XNU and its real panic screen. Enabling the upstream
DT panic-region option passed the console-creation checkpoint. The latest
observed failure is `AMFI: No PMGR?` at `ConfigurationSettings.cpp:235`.
See [the observation](docs/observations/amfi-no-pmgr-20261030.md) and
the authoritative [porting record](porting.toml).

The next integration is an opt-in, ABI-gated original PMGR object lifecycle
test. See [its source, build inputs and limits](docs/pmgr-abi.md). Real clock,
regulator, CPU-power and suspend/resume backends are not implemented here.

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
