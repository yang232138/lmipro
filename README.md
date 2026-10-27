# UniXNU-lmipro

Device repository for the Xiaomi Redmi K30 Pro / K30 Pro Zoom Edition
(`lmi` / `lmipro`, model `M2001J11C`, Qualcomm SM8250 / kona).

| Repository | Contents |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | Build tools, component catalog, ARM64 and OS profiles |
| [UniXNU-sm8250-common](https://github.com/FFT-infra/UniXNU-sm8250-common) | SM8250 resources, DTSI, config fragment, SMP patches |
| `uefi-lmipro` (to create) | Mu-Silicium UEFI port |

## State

Structure only. `board.toml`, the SoC pin, the board device tree and the defconfig
are in place; no hardware input has been read from the handset, no component beyond
the shared kernel path is selected, and no image has been built. `porting.toml`
carries the per-area record, limits and next action.

## Inputs

| Item | Value |
| --- | --- |
| SoC | SM8250 (kona), 4x Cortex-A55 + 4x Cortex-A77 |
| Donor | `iPad7,1`, iPadOS `17.7.11`, build `21H461`, `j120ap` |
| Display | 6.67-inch 1080x2400 AMOLED, 60 Hz; framebuffer at `0x9c000000` |
| Touch | FocalTech FT3518 over QUPv3 GENI SPI |
| Wi-Fi | QCA6390 |
| Userdata | REQUIRED: first and last LBA of the `userdata` GPT entry |
| Firmware config / DSC | `lmipro` / `Platforms/Xiaomi/lmiproPkg/lmipro.dsc` |
| Firmware volume | `Build/lmiproPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd` |

## Build

Run from the UniXNU checkout:

```sh
./unixnu defconfig lmipro_defconfig -D ../UniXNU-lmipro
./unixnu fetch -D ../UniXNU-lmipro
./unixnu status --json -D ../UniXNU-lmipro
./unixnu tree -D ../UniXNU-lmipro
```

The porting record check runs here as well:

```sh
python3 scripts/porting-record.py check --device ../UniXNU-lmipro
```

Read [porting](https://unixnu.com/porting/) and the
[bring-up order](https://unixnu.com/porting/bring-up) before the first build.

## Required before the first build

1. Fill both `memory` ranges in `dts/lmipro.dts` from the handset's live device tree.
2. Fill `[userdata] first_lba` / `last_lba` from the handset GPT.
3. Create `uefi-lmipro`, add `UniXNURamDiskDxe`, DiskIoDxe and FAT support, and
   confirm the firmware volume path.
4. Confirm the `boot_a`/`boot_b` size before choosing `[payload] size_mib`.
5. Record the firmware files and their SHA-256 values in `proprietary-files.txt`.

## Layout

| Path | Contents |
| --- | --- |
| `board.toml` | Board selection, userdata geometry, firmware, payload and ADT inputs |
| `feeds.toml` | SoC source pin |
| `dts/lmipro.dts` | Board memory, reserved regions, cluster types and enabled peripherals |
| `configs/lmipro_defconfig` | Device kernel configuration on top of the arch, SoC and OS fragments |
| `patches/xnu/series` | Board XNU patches in application order |
| `porting.toml` | Porting record rendered by the documentation site |
| `proprietary-files.txt` | Firmware inputs and hashes |
| `package/` | Board packaging, appears as `kits/lmipro` |
