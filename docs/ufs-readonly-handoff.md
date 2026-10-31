# lmipro read-only UFS handoff observation

The continued PMGR build reached root mount failure. This observer first records the actual
SM8250 UFS clock/power state, conditionally reads UFSHCI registers, then records SMMUv2 global
stream matching for 0x0e0 and 0x4e0. It submits no commands, performs no register writes, changes
no clock, reset, SMMU or request list, and publishes no storage device.

The firmware DT captured from this handset identifies controller 0x1d84000/0x3000 and PHY
0x1d87000/0xe00. Linux v6.12 sm8250.dtsi and gcc-sm8250.c supply the SoC register facts.
SM8550/ShengUfs clock offsets and stream 0x60 are deliberately not reused on this board.

Sources: https://github.com/torvalds/linux/blob/v6.12/drivers/clk/qcom/gcc-sm8250.c
https://github.com/torvalds/linux/blob/v6.12/arch/arm64/boot/dts/qcom/sm8250.dtsi
Mapping and observation reference: https://github.com/UniXNU/ShengUfs/blob/e2b32cc429e8cf1501161c1d8c517f8994ceb9b2/NativeUfs.cpp
Procedure: https://unixnu.com/zh/porting/ufs

The caller follows the already-qualified PMGR call in bsd_init. The helper object is appended
by an explicit board recipe to the existing board archive, whose changed SHA256 is then
tracked by the standard PMGR link dependency mechanism; the original provider and 340-slot
ABI checks remain intact. The helper has no class constructor and is off unless
lmipro-ufs-handoff=1. For that mode, the final diagnostic panic is the intentional observation
page, with the official no-reset renderer. It is not a system boot or root mount PASS.

The supplied userdata image manifest and full window digests were verified without writing
the file. The OS APFS System-role volume UUID is 04769D26-2CA0-4825-B443-E2E5F88300F2; it is
recorded for root matching. This does not qualify the source kernel to access storage or
authorize an OS root volume. No userdata flashing is performed by this workflow.
