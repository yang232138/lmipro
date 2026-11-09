#!/usr/bin/env python3
"""Bind unchanged catalog RamBlock imports to the same pinned storage ABI as UFS."""
from pathlib import Path
import json, hashlib, subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/ram-block'
obj=out/'NativeRamBlock.o'
symbols=subprocess.check_output(['llvm-nm','-u',str(obj)],text=True)
undefined=[line.split()[-1] for line in symbols.splitlines() if 'IOBlockStorageDevice' in line]
assert undefined and not any('IOBlockStorageDeviceD0Ev' in n or 'IOBlockStorageDeviceD1Ev' in n for n in undefined)
audit_path = root / 'build/storage-abi/interface-audit.json'
if audit_path.exists():
    audit = json.loads(audit_path.read_text())
    possible = {
        '__ZTV20IOBlockStorageDevice': int(audit['vtable'], 16),
        '__ZN20IOBlockStorageDeviceC2EPK11OSMetaClass': int(audit['original_protected_constructor'], 16),
        '__ZN20IOBlockStorageDeviceD2Ev': int(audit['original_base_destructor'], 16),
        '__ZN20IOBlockStorageDevice10gMetaClassE': int(audit['original_metaclass'], 16),
    }
    for row in audit.get('methods', []):
        if row.get('reference_name') and row.get('candidate_target'):
            possible[row['reference_name']] = int(row['candidate_target'], 16)
    origin = 'interface-audit.json'
else:
    bindings_path = root / 'scripts/data/storage-bindings.json'
    if not bindings_path.exists():
        raise SystemExit('no interface-audit.json and no scripts/data/storage-bindings.json')
    bindings = json.loads(bindings_path.read_text())
    cache = root.parents[3] / 'ipsw/21H461/kernelcache.decompressed'
    if not cache.exists():
        raise SystemExit(f'pinned kernelcache missing: {cache}')
    digest = hashlib.sha256(cache.read_bytes()).hexdigest()
    if digest != bindings['kernelcache_sha256']:
        raise SystemExit(f'kernelcache mismatch: {digest} != {bindings["kernelcache_sha256"]}')
    # The recorded addresses are pre-slide; the sheng checker applies the recorded
    # slide before comparing against the prelinked segments, and so must we.
    slide = int(bindings['slide'])
    possible = {name: (int(value, 16) + slide) & 0xffffffffffffffff
                for name, value in bindings['addresses'].items()}
    origin = 'scripts/data/storage-bindings.json'
print(f'storage binding origin: {origin} ({len(possible)} symbols)')

missing = [name for name in undefined if name not in possible]
if missing:
    raise SystemExit(f'unqualified storage import: {missing}')

assembly = out / 'lmipro-ram-bindings.S'
assembly.write_text(''.join(f'.globl {name}\n.set {name}, {possible[name]:#x}\n' for name in sorted(set(undefined))))
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-Werror',
                '-Wno-incompatible-sysroot', '-c', str(assembly), '-o', str(out / 'lmipro-ram-bindings.o')],
               check=True, timeout=120)


archive=out/'libRamBlock.a'
provider=root/'package/LmiproVolatileStore.c'
provider_obj=out/'LmiproVolatileStore.o'
subprocess.run(['xcrun','clang','-target','arm64-apple-ios17.0','-O2',
                '-ffreestanding','-fno-stack-protector','-Wall','-Wextra','-Werror',
                '-Wno-incompatible-sysroot','-c',str(provider),'-o',str(provider_obj)],
               check=True,timeout=120)
subprocess.run(['xcrun','ar','rcs',str(archive),str(out/'lmipro-ram-bindings.o'),str(provider_obj)],check=True)
(out/'lmipro-bindings.json').write_text(json.dumps(dict(object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
    bindings={n:possible[n] for n in sorted(set(undefined))}, origin=origin,
    persistence_mode=0, persistence_io_result=0,
    volatile_provider_sha256=hashlib.sha256(provider.read_bytes()).hexdigest(),
    volatile_provider_object_sha256=hashlib.sha256(provider_obj.read_bytes()).hexdigest()),indent=2)+'\n')
