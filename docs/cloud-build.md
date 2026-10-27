# Cloud build

The [macOS build](https://github.com/UniXNU/UniXNU/blob/main/.github/workflows/macos.yml)
workflow runs the kernel step on a GitHub macOS runner, so no local Mac is needed.
It is a build check: it publishes redacted text logs and uploads no image.

## 1. Publish this repository

Create an empty **public** repository `yang232138/UniXNU-lmipro` on GitHub, then push
the local history:

```sh
git remote add origin https://github.com/yang232138/UniXNU-lmipro.git
git push -u origin main
```

A public repository is the simplest input: the runner clones it without a token. A
private one needs the `UNIXNU_DEVICE_TOKEN` secret in the fork (step 3).

## 2. Fork UniXNU

Fork <https://github.com/UniXNU/UniXNU> to your account and open the **Actions** tab
once to enable workflows on the fork. The `macOS build` workflow lives there, not in
the device repository.

## 3. Optional secret

The kernel step does not need it. Add `UNIXNU_UNIQUE_CHIP_ID` (hex) only when the
payload step is selected, because `board.toml` marks the platform seed as required.

## 4. Run the workflow

In the fork: **Actions → macOS build → Run workflow**, with:

| Input | Value |
| --- | --- |
| `device_repository` | `yang232138/UniXNU-lmipro` |
| `device_ref` | `main` |
| `defconfig` | `lmipro_defconfig` |
| `steps` | `kernel` |
| `xcode` | `26.6` |
| `ipsw_url` | empty (uses the OS profile) |

`steps: kernel` compiles XNU, applies the 61-patch series (including the two SM8250
SMP patches), runs the component recipes and prelinks against the donor kernelcache.

## What a green run proves

- The device repository resolves: arch, SoC, OS and device layers, the pinned SM8250
  commit, and the component set.
- XNU applies this port's patch order and links with `ArmDTPlatform`, `ArmCpu`,
  `SoftwareAES`, `XnuPrelink`.
- The prelink step reports a kernel hash.

## What it does not prove

- No firmware: `uefi-lmipro` does not exist, and the workflow runs no `build firmware`.
- No boot image: the device tree still carries a zero DRAM size, so the ADT would
  describe no memory; `build payload` and `bootimg` need `docs/inputs.md` values first.
- No hardware behaviour of any kind.

## If it fails

Read the job summary first, then the `*.log` artifact. The usual failure classes for a
fresh device repository are: a missing component in `[kernel] archives`, a defconfig
value the kernel does not know, a patch that no longer applies, or a source the pinned
commit no longer provides. Record the failing step in `porting.toml` before changing
code, so the next run answers one question.
