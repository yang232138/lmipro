# SMMU RO1: pre-DMA ownership audit

Opt-in lmipro-ufs-handoff=24, default off. All MMIO accesses are 32-bit reads.
No UIC/UTP submissions, no HCE/reset/PHY/clock/power/SMMU changes, no disk I/O.

Primary sources (Linux v6.12):
- arch/arm64/boot/dts/qcom/sm8250.dtsi: apps-SMMU 0x15000000+0x100000; UFS SIDs 0xe0,0x4e0.
- drivers/iommu/arm/arm-smmu/arm-smmu.h: SCR0, ID0/1/2, GFSR 0x48, SMR/S2CR and bank registers.
- drivers/iommu/arm/arm-smmu/arm-smmu.c: ID1 page shift and NUMPAGENDXB geometry.

Unsupported stream indexing or EXID configuration stops before tables. Tables bounded
by ID0 NUMSMRG (max255). Classic VALID/15-bit and EXIDVALID/16-bit matching are separate.
Only a unique TRANS match with a valid bank and a geometry exactly consistent with the
documented 1 MiB instance permits bank reads. Duplicate matches, BYPASS, FAULT or unknown
geometry do not dereference a bank. The audit never walks page tables or claims DMA proof.

Expected screen LMIPRO SMMU RO1 displays raw CR0/IDs, global faults, stream matches and
matched context state. A controlled diagnostic panic preserves the screen, no timed reboot.
Capture all lines. Real block reads and root mount remain unqualified.
