# Actual lmipro APFS and XNU crypto result

User transcribed: `lmipro root fs=apfs crypto=1 ram=1 reads=53 failures=0`.
This followed verified temporary boot2a3792560e331f8f9fa19e7e5efd61905964c8f62b6ec01ca73127d80147fa48
from run41038124033, raw kernel5c0df80482f58c2b257b2fd06fb8a60c05e2d32708b89878b5bf3e839d3ea157.
Source device00ed83e2e11848d143b75fcb8f8482e9d217bde2, workflow91c8e6658a3e98ed14ecdf131174424eddf8861d.
The exact public policy/key/samples, unchanged original Apple executables,
same FD/DTB/shim and official entry/panic changes were independently checked.

This confirms actual APFS root mounting, RAM readiness,53 completed read calls
and no reported read failures. crypto=1 is set by the unmodified official XNU
probe only after its eight real positive/negative controls and signature-call
count pass. Individual result values were not transcribed separately.
It is a user-reported screen observation, not a complete captured kernel log.
Owner root acceptance was disabled and the current small seed has no complete
OS/userland. Those milestones remain unqualified.
