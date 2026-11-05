#!/usr/bin/env python3
"""Build libVerifiedRootProbe.a for the owner-root kernel.

build-xnu-4k-baseline.sh requires build/verified-root-xnu/libVerifiedRootProbe.a
whenever XNU_VERIFIED_ROOT_PROBE is set. The archive carries the catalog
VerifiedRoot kit (verified_root.c), the XNU crypto adapter that supplies SHA-256
and RSA-3072 through the kernel's own crypto provider, the probe entry that
bsd_init.c calls, and the probe inputs.

This board has no signed SystemOS policy: the installed SHNGROOT image has not
been qualified through kits/VerifiedRoot/tools/manifest.py, so the manifest the
probe would authenticate is empty. vr_open() therefore reports VR_FORMAT and
native_verified_root_status stays 0 - the probe never claims a verification it
did not perform. The owner root and the owner SystemOS mount are authorized by
the read-only UFS block driver (package/LmiproUfsBlock.cpp), not by this
instrument.

The archive also carries weak owner-root hooks. The flag-off reference candidate
is built with the same owner-root options but without the PMGR archive that
carries the UFS block driver, so those hooks would not resolve there; the board
driver's strong definitions override the weak ones in the linked kernel.
"""
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path

if os.environ.get('XNU_VERIFIED_ROOT_PROBE', '0') != '1':
    print('VerifiedRoot probe archive not built: the owner-root probe is off')
    raise SystemExit(0)

root = Path(__file__).resolve().parents[1]
out = root / 'build/verified-root-xnu'
out.mkdir(parents=True, exist_ok=True)
record_path = root / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT/bsd_init.o.json'
if not record_path.is_file():
    raise SystemExit(f'the kernel compile record is missing: {record_path}')
record = json.loads(record_path.read_text().strip().rstrip(','))

fallback = out / 'owner-root-fallback.c'
fallback.write_text('''/* Weak owner-root hooks so the flag-off reference build links without the PMGR archive. */
#include <stddef.h>
#include <sys/types.h>

int native_verified_block_root_authorized(dev_t device);
int native_verified_block_volume_device(const char *uuid, dev_t *device, char *path, size_t size);

__attribute__((weak)) int
native_verified_block_root_authorized(dev_t device)
{
	(void)device;
	return 0;
}

__attribute__((weak)) int
native_verified_block_volume_device(const char *uuid, dev_t *device, char *path, size_t size)
{
	(void)uuid;
	if (device) *device = (dev_t)-1;
	if (path && size) path[0] = 0;
	return 0;
}
''')

sources = [
    root / 'kits/VerifiedRoot/src/verified_root.c',
    root / 'kits/VerifiedRoot/adapters/xnu_crypto.c',
    root / 'scripts/qemu-entry/verified-root-probe.c',
    fallback,
]
for source in sources:
    if not source.is_file():
        raise SystemExit(f'VerifiedRoot source missing: {source}; run "unixnu fetch" first')

objects = []


def compile_source(source, destination):
    command, index = [], 0
    arguments = record['arguments']
    while index < len(arguments):
        value = arguments[index]
        if value == record['file']:
            command.append(str(source))
        elif value == '-o':
            command += ['-o', str(destination)]
            index += 1
        elif value in ('-MF', '-MJ'):
            index += 1
        elif value in ('-MD', '-MMD', '-MP'):
            pass
        else:
            command.append(value)
        index += 1
    command += ['-I' + str(root / 'kits/VerifiedRoot/include'), '-Wall', '-Werror']
    result = subprocess.run(command, cwd=record['directory'], capture_output=True, text=True)
    (out / (source.stem + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr[-4000:] or result.stdout[-4000:])
    return destination


for source in sources:
    objects.append(compile_source(source, out / (source.stem + '.o')))

# Empty instrument inputs: a 160-byte header plus its 384-byte signature, an empty
# anchor, and the four sample descriptions the probe walks. vr_open() rejects the
# header magic and reports VR_FORMAT, which is the honest result for a board with
# no qualified owner policy.
assembly = out / 'verified-root-inputs.S'
assembly.write_text('''// Empty owner-verification inputs: no signed SystemOS policy exists for this board.
.section __DATA_CONST,__const
.balign 8
.globl _verified_root_manifest, _verified_root_manifest_bytes
_verified_root_manifest:
.zero 544
_verified_root_manifest_bytes:
.quad . - _verified_root_manifest
.balign 8
.globl _verified_root_key
_verified_root_key:
.zero 395
.balign 8
.globl _verified_root_sample_indices
_verified_root_sample_indices:
.quad 0, 0, 0, 0
.globl _verified_root_sample_lengths
_verified_root_sample_lengths:
.quad 0, 0, 0, 0
.balign 8
.globl _verified_root_samples
_verified_root_samples:
.zero 16384
''')
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-c', str(assembly),
                '-o', str(out / 'verified-root-inputs.o')], check=True, timeout=120)
objects.append(out / 'verified-root-inputs.o')

for obj in objects:
    data = obj.read_bytes()
    magic, cputype = struct.unpack_from('<2I', data)
    if (magic, cputype) != (0xfeedfacf, 0x100000c):
        raise SystemExit(f'{obj.name} is not an ordinary ARM64 object')

archive = out / 'libVerifiedRootProbe.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), *objects], check=True, timeout=120)

report = dict(
    archive='build/verified-root-xnu/libVerifiedRootProbe.a',
    archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
    sources={str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources},
    compile_record='build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT/bsd_init.o.json',
    probe_inputs='empty manifest (544 zero bytes), empty anchor and zero samples',
    probe_manifest_bytes=544,
    real_signed_policy=False,
    owner_root_authorized_by='package/LmiproUfsBlock.cpp',
    weak_owner_root_fallbacks=['native_verified_block_root_authorized',
                               'native_verified_block_volume_device'],
)
(out / 'build-report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
print('PASS: libVerifiedRootProbe.a built from the catalog kit with the probe inputs absent')
