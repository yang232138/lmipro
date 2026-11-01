# SMMU RO2: visible routing table census

This board diagnostic is opt-in mode 25; default remains off. It preserves the
qualified FD and kernel security contracts, performs no register or disk writes,
submits no UIC/UTP/DMA/reset commands, and deliberately stops on a result screen.
It does not read context banks or page tables. It cannot qualify DMA or mount root.

## Prior hardware evidence

RO1 run 41311297798, device a7dea0874d5087c7b46b311c18cfe60a2e5478f1,
overlay 90c51393f8f0530afa2e9ac11077c9c59590d5a4, boot SHA256
a72e194c61775fcd2042715e308956b3e73bcd71a891e6655a1d77435200c961.
Photo 1793570865069.jpg: stage 3, CR0 00260406, ID0 4c017e57, ID1
6000004a, ID2 00005111, 87 groups, no valid match for candidate e0/4e0.
Unread context and match-detail zeroes were placeholders. GFSR=0 before DMA
does not prove DMA works. CLIENTPD=0 means clients enabled; USFCFG=1 selects
faulting unmatched streams if the actual UFS path uses this instance.

## Routing uncertainty

The handset live FDT and stock base UFS nodes do not declare `iommus`.
Their apps-SMMU is qcom,qsmmu-v500 at 0x15000000+0x100000, with
qcom,skip-init. Linux v6.12 sm8250.dtsi supplies candidates e0 and 4e0.
This difference is not proof that either candidate is wrong or the SMMU unused.
Do not create bypass mappings from the current evidence.

## Measurement and interpretation

Read GR0 global identification/fault state and SMR/S2CR only. Report declared
groups separately from `scan=min(groups,128)`: Linux v6.12 qcom_smmu_cfg_probe
explicitly caps nonarchitected extra groups. Classic and EXID validity differ.

- `valid`: visible valid entries; `nz`: any nonzero SMR/S2CR pair.
- T/B/F/R: raw TYPE counts for valid TRANS/BYPASS/FAULT/reserved entries.
  These do not prove effective routing, particularly on trapped Qualcomm views.
- Four samples are the first nonzero pairs; only sample-count rows are meaningful.
- Compare two complete snapshots with exact per-entry equality. Hash fields
  are 32-bit FNV word fingerprints for screen reference, not security hashes.
- `changed`: unequal pairs; `gchg`: changed CR0/ID0/ID1/ID2/GFSR bits 0..4.
  Zero means equal sampled values, not atomic state or absence of hidden changes.
- All zero or all-ones views need visibility/firmware investigation. Valid entries
  for unrelated streams distinguish that case from only missing UFS candidates.

Linux arm-smmu-qcom.c describes firmware trapping/substituting S2CR writes.
Its active quirk detection is deliberately NOT executed here. No context bank,
stream route or device ownership is changed.

## Validation and next gate

Host tests exercise zero, unrelated, mixed types, invalid and extended entries,
all-ones pairs, exact table/global changes, unsupported identification, 128 cap,
and reject any context-bank/out-of-range read through the fake MMIO interface.
Cloud workflow checks original AMFI/PMGR executables, probe linkage, source hashes,
qualified FD hash, no write/command API and boot-image input round-trip.

Hardware pending. Capture the RO2 screen after a checked `fastboot boot`;
no flash/erase/repartition. Resolve visibility and actual route before bounded
known-block DMA; root remains unqualified.

## Primary references

- https://unixnu.com/zh/porting/ufs
- https://github.com/torvalds/linux/blob/v6.12/arch/arm64/boot/dts/qcom/sm8250.dtsi
- https://github.com/torvalds/linux/blob/v6.12/drivers/iommu/arm/arm-smmu/arm-smmu.h
- https://github.com/torvalds/linux/blob/v6.12/drivers/iommu/arm/arm-smmu/arm-smmu-qcom.c
