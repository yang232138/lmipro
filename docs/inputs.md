# Board inputs to collect

`dts/lmipro.dts` and `board.toml` keep zero or `REQUIRED` values for everything that
has not been read from the handset. This page lists each value, the command that
reads it, and where it goes. Run the commands in a rooted Android shell on the
device (`adb shell`, then `su`).

| Value | How to read it | Goes to |
| --- | --- | --- |
| DRAM base and first-bank size | `od -An -tx1 -v /proc/device-tree/memory@80000000/reg` | `dts/lmipro.dts` `memory@80000000` `reg` |
| Reserved regions | `ls /proc/device-tree/reserved-memory/`, then read each `reg` | `dts/lmipro.dts` `reserved-memory` |
| Framebuffer geometry | `cat /sys/class/graphics/fb0/virtual_size` and `.../stride` | `dts/lmipro.dts` framebuffer size, display area |
| `userdata` GPT entry | `sgdisk -p /dev/block/sda` | `board.toml` `[userdata]` `first_lba` / `last_lba` |
| Boot partition size | `blockdev --getsize64 /dev/block/by-name/boot_a` | `board.toml` `[payload] size_mib` |
| Vendor firmware | `ls -l /vendor/firmware`, then `sha256sum` each file | `proprietary-files.txt` |
| Touch node | `find /proc/device-tree -iname '*ft*'` and inspect `compatible`, `reg`, `interrupts` | `porting.toml` input area, board touch kit |
| Panel timing | `dumpsys SurfaceFlinger` (resolution and refresh rate) | display area |
| CPU topology | `cat /proc/cpuinfo` | cpu area |
| Wi-Fi firmware | `ls /vendor/firmware/wlan` | network area |

## Reading a device-tree property

Values under `/proc/device-tree` are big-endian property blobs, not text:

```sh
od -An -tx1 -v /proc/device-tree/memory@80000000/reg
```

Record the raw hex, the date and the command next to the matching `porting.toml`
finding. A value read from one handset is an observation, not a platform constant:
the porting guide requires it to be taken from the target device, and RAM size differs
between the 6 GB, 8 GB and 12 GB variants.

## After the firmware exists

Once `uefi-lmipro` builds and boots into the UniXNU menu, the firmware menu and its
log exporter become the second source: they report the GOP geometry, the memory map
and the FD load address without Android.
