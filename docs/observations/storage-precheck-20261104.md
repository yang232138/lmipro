# Powered controller observation

Device: LMIPRO, SM8250. Exact boot SHA256: 8e0b2d26f4a0827715379a84acc8dcbb25be35ed5adcc1b9f9cf1025eed8afc5.
Device source f820343ceffd47f568607166c7acbf049ef5a6cc; overlay 9bcffd0f8ef5500424cc8c8f556f1df39336463f; run 41767832591.
Temporary boot returned OKAY; photo 1793764936745.jpg shows STORAGE PRECHECK v1.
HCE=1, HCS=0x10f, CAP=0x1587031f, VER=0x300, DB=0, run=1. SMMU ID0=0x4c017e57, ID1=0x6000004a, ID2=0x5111. 87 scanned groups, 5 valid translations, unchanged snapshots, neither e0 nor 4e0 matched. No DMA or disk read has been demonstrated.

Next candidate follows https://unixnu.com/zh/porting/ufs and Linux v6.12 sm8250.dtsi lines 2495-2509. It tests the source-backed stream hypothesis without claiming it as hardware-proven. Only previously unclaimed slots/banks are assigned after queue quiescence. Existing mappings are refused. Timeout has no retry/cancel/free. The allocation stays live until reset. No disk read/write, root publication, clock/PHY reset or persistent firmware update is implemented by this test.
