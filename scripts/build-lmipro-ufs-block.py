#!/usr/bin/env python3
"""Build the read-only UFS block driver and bind its IOStorageFamily imports.

The driver subclasses IOBlockStorageDevice, which lives in the kernelcache's
IOStorageFamily kext. The public headers come from the storage ABI directory;
the undefined class symbols are bound to the kernelcache addresses recorded by
scripts/audit-storage-interface.py, exactly as build-virtio-mmio-bindings.py does
for the virtio/ram block drivers.
"""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

if __import__('os').environ.get('XNU_SHENG_PMGR', '0') != '1':
    print('UFS block driver not linked in the flag-off reference')
    raise SystemExit(0)

root = Path(__file__).resolve().parents[1]
out = root / 'build/lmipro-ufs-block'
out.mkdir(parents=True, exist_ok=True)
source = root / 'kits/lmipro/LmiproUfsBlock.cpp'
obj = out / 'LmiproUfsBlock.o'
record = json.loads((root / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json').read_text().strip().rstrip(','))

command = []
i = 0
arguments = record['arguments']
while i < len(arguments):
    v = arguments[i]
    if v == record['file']:
        command.append(str(source))
    elif v == '-o':
        command += ['-o', str(obj)]
        i += 1
    elif v == '-Xclang' and arguments[i + 1].startswith('-W'):
        i += 1
    elif not v.startswith('-W'):
        command.append(v)
    i += 1
command += ['-I', str(root / 'build/storage-abi/include'),
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
            '-Wno-c99-designator', '-Wno-reorder-init-list',
            '-Wno-deprecated-enum-enum-conversion', '-Wno-unnecessary-virtual-specifier']
subprocess.run(command, cwd=record['directory'], check=True, timeout=180)

symbols = subprocess.check_output(['llvm-nm', '-u', str(obj)], text=True)
undefined = [line.split()[-1] for line in symbols.splitlines() if 'IOBlockStorageDevice' in line]
if not undefined:
    raise SystemExit('the driver no longer imports the storage base class')

audit_path = root / 'build/storage-abi/interface-audit.json'
if not audit_path.exists():
    subprocess.run([sys.executable, str(root / 'scripts/audit-storage-interface.py')], check=True, timeout=900)
audit = json.loads(audit_path.read_text())

possible = {
    '__ZTV20IOBlockStorageDevice': int(audit['vtable'], 16),
    '__ZN20IOBlockStorageDeviceC2EPK11OSMetaClass': int(audit['original_protected_constructor'], 16),
    '__ZN20IOBlockStorageDeviceD2Ev': int(audit['original_base_destructor'], 16),
    '__ZN20IOBlockStorageDevice10gMetaClassE': int(audit['original_metaclass'], 16),
}
missing = [name for name in undefined if name not in possible]
if missing:
    raise SystemExit(f'unqualified storage import: {missing}')

assembly = out / 'original-bindings.S'
assembly.write_text(''.join(f'.globl {name}\n.set {name}, {possible[name]:#x}\n' for name in sorted(set(undefined))))
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-Werror',
                '-Wno-incompatible-sysroot', '-c', str(assembly), '-o', str(out / 'original-bindings.o')],
               check=True, timeout=120)

archive = root / 'build/sheng-platform-power/libShengPlatformPower.a'
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), str(obj), str(out / 'original-bindings.o')],
               check=True, timeout=120)

proof = dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
             object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
             bindings={name: hex(value) for name, value in sorted(possible.items())},
             undefined=[name for name in sorted(set(undefined))],
             audit_sha256=hashlib.sha256(audit_path.read_bytes()).hexdigest(),
             reads_only=True, write_commands=0)
(out / 'build-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps({k: v for k, v in proof.items() if k != 'bindings'}, indent=2))
print(f'PASS: {len(set(undefined))} IOStorageFamily imports bound from the audited original class')
