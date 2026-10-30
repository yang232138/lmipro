#!/usr/bin/env python3
"""Statically qualify lmipro RamBlock bindings to the pinned original storage class.\n\nNo live metadata observation or block I/O success is asserted by this host audit.\nAdapted from UniXNU audit-storage-interface.py at ee472385743ebd16941ab14892dcb9d02e219d15.\n"""
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'kits/XnuPrelink'))
import native_prelink as np
source = ROOT / 'refer/IOStorageFamily'
revision = subprocess.check_output(['git', '-C', source, 'rev-parse', 'HEAD'], text=True).strip()
if revision != 'e45fbe30a7879e0b16e26dddb78538f1b00e0bff':
    raise SystemExit('Unexpected public Darwin 23 IOStorageFamily source')
out = ROOT / 'build/ram-block'
out.mkdir(parents=True, exist_ok=True)
headers = ROOT / 'build/storage-abi/include/IOKit/storage'
headers.mkdir(parents=True, exist_ok=True)
for path in source.glob('*.h'):
    shutil.copyfile(path, headers / path.name)
# Only this platform-selection header is missing from the source-kernel SDK.
# The reference object is an annotation instrument and is never linked/executed.
sdk = Path(subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip())
conditions = sdk / 'usr/include/TargetConditionals.h'
shutil.copyfile(conditions, ROOT / 'build/storage-abi/include/TargetConditionals.h')
record = json.loads((ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json').read_text().strip().rstrip(','))
obj_path = out / 'IOBlockStorageDevice-reference.o'
arguments = record['arguments']
command, index = [], 0
while index < len(arguments):
    value = arguments[index]
    if value == record['file']:
        command.append(str(source / 'IOBlockStorageDevice.cpp'))
    elif value == '-o':
        command += ['-o', str(obj_path)]
        index += 1
    elif value in ('-MF', '-MJ'):
        index += 1
    elif value == '-Xclang' and arguments[index + 1].startswith('-W'):
        index += 1
    elif not value.startswith('-W') and value not in ('-MD', '-MP'):
        command.append(value)
    index += 1
command += ['-I', str(ROOT / 'build/storage-abi/include'), '-I', str(ROOT / '.cache/xnu-work/osfmk'),
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-deprecated-declarations',
            '-Wno-c99-designator', '-Wno-reorder-init-list', '-Wno-unnecessary-virtual-specifier',
            '-Wno-deprecated-enum-enum-conversion']
(out / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
r = subprocess.run(command, cwd=record['directory'], capture_output=True, text=True)
(out / 'compile.log').write_text(r.stdout + r.stderr)
if r.returncode:
    raise SystemExit((r.stdout + r.stderr)[-3500:])


def require(value, message):
    if not value:
        raise SystemExit(message)


obj = obj_path.read_bytes()
om = np.macho(obj)
require(om['kind'] == 1, 'Expected an ordinary ARM64 reference object')
dwarf = subprocess.check_output(['xcrun', 'dwarfdump', '--name=IOBlockStorageDevice', '--show-children', obj_path], text=True)
class_size = re.search(r'DW_AT_name\s+\("IOBlockStorageDevice"\)[\s\S]{0,200}?DW_AT_byte_size\s+\((0x[0-9a-f]+)\)', dwarf)
require(class_size and int(class_size[1], 16) == 0x90, 'Actual compiled public class size changed')
version_command = next(at for command, at, size in om['commands'] if command == 0x32)
require(struct.unpack_from('<II', obj, version_command + 8) == (2, 0x110000), 'Expected iOS 17 reference deployment')
sycmd = next(at for command, at, size in om['commands'] if command == 2)
symoff, count, stroff, _ = struct.unpack_from('<4I', obj, sycmd + 8)
symbols = []
for i in range(count):
    ix, kind, _, _, value = struct.unpack_from('<IBBHQ', obj, symoff + i * 16)
    symbols.append(dict(name=obj[stroff + ix:obj.index(b'\0', stroff + ix)].decode(), value=value, kind=kind))
vtable = next(s['value'] for s in symbols if s['name'] == '__ZTV20IOBlockStorageDevice')
end = min(s['value'] for s in symbols if not s['kind'] & 0xe0 and s['kind'] & 0xe == 0xe and s['value'] > vtable)
relocations = {}
for segment in om['segments']:
    for section in segment['sections']:
        if section['va'] <= vtable < section['va'] + section['size']:
            ro, rc = struct.unpack_from('<II', obj, section['command_offset'] + 56)
            for i in range(rc):
                address, word = struct.unpack_from('<II', obj, ro + i * 8)
                if word >> 27 & 1 and word >> 28 == 0 and word >> 25 & 3 == 3:
                    relocations[section['va'] + address] = symbols[word & 0xffffff]['name']
methods = []
for at in range(vtable + 16, end, 8):
    if at not in relocations:
        break
    methods.append(relocations[at])
require(len(methods) == 190 and methods.count('___cxa_pure_virtual') == 14, 'Public iOS class dispatch shape changed')
kernel_path = ROOT / 'build/pmgr-reference/lmipro-reference.macho'
kernel = kernel_path.read_bytes()
plan = json.loads(kernel_path.with_suffix('.json').read_text())
require(hashlib.sha256(kernel).hexdigest() == plan['candidate_sha256'] and
        plan['original_sha256'] == 'f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7', 'Unexpected original candidate provenance')
native = np.macho(kernel)
exports = np.exports(kernel, native)
by_name = {name: va for va, names in exports.items() for name in names}
extents = [e for e in plan['extents'] if e['module'] == 'com.apple.iokit.IOStorageFamily']
const = next(e for e in extents if e['segment'] == '__DATA_CONST')
text = next(e for e in extents if e['segment'] == '__TEXT_EXEC')


def read(va, length):
    at = np.backed_offset(native['segments'], va, length, len(kernel))
    return kernel[at:at + length]


require(hashlib.sha256(read(text['va'], text['size'])).hexdigest() ==
        plan['original_executable_sha256']['com.apple.iokit.IOStorageFamily'], 'Original storage executable code changed')
blob = read(const['va'], const['size'])
found = []
for at in range(0, len(blob) - (len(methods) + 2) * 8 + 1, 8):
    if blob[at:at + 16] != bytes(16):
        continue
    pointers = struct.unpack_from('<' + 'Q' * len(methods), blob, at + 16)
    if all(pointers[i] == by_name[name] for i, name in enumerate(methods) if name in by_name):
        found.append((const['va'] + at, pointers))
require(len(found) == 1, 'Cannot uniquely identify the original block-storage class dispatch')
table, pointers = found[0]
matched = sum(name in by_name for name in methods)
require(matched == 176, 'Unexpected original/native kernel binding count')
for i, name in enumerate(methods):
    if name not in by_name:
        require(text['va'] <= pointers[i] < text['va'] + text['size'], 'Owned storage method outside original text')


def relative_branch(address, instruction):
    immediate = instruction & 0x3ffffff
    if immediate & 0x2000000:
        immediate -= 0x4000000
    return address + immediate * 4


def thunk_names(address):
    a, load, branch = struct.unpack('<3I', read(address, 12))
    require(a & 0x9f00001f == 0x90000010 and load & 0xffc003ff == 0xf9400210 and branch == 0xd61f0200,
            'Unqualified original native import thunk')
    immediate = ((a >> 5 & 0x7ffff) << 2) | (a >> 29 & 3)
    if immediate & 0x100000:
        immediate -= 0x200000
    slot = (address & ~4095) + immediate * 4096 + (load >> 10 & 4095) * 8
    target = struct.unpack('<Q', read(slot, 8))[0]
    return exports.get(target, [])


constructors = []
for address in range(text['va'], text['va'] + text['size'] - 32, 4):
    words = struct.unpack('<8I', read(address, 32))
    if (words[:2] != (0xa9bf7bfd, 0x910003fd) or words[2] & 0xfc000000 != 0x94000000 or
        words[3] & 0x9f00001f != 0x90000008 or words[4] & 0xffc003ff != 0x91000108 or
        words[5:] != (0xf9000008, 0xa8c17bfd, 0xd65f03c0)):
        continue
    immediate = ((words[3] >> 5 & 0x7ffff) << 2) | (words[3] >> 29 & 3)
    if immediate & 0x100000:
        immediate -= 0x200000
    stored = ((address + 12) & ~4095) + immediate * 4096 + (words[4] >> 10 & 4095)
    if stored == table + 16:
        constructors.append((address, relative_branch(address + 8, words[2])))
require(len(constructors) == 1 and '__ZN9IOServiceC2EPK11OSMetaClass' in thunk_names(constructors[0][1]),
        'Original protected block constructor does not forward to actual source IOService')
destructor = pointers[0] - 4
word = struct.unpack('<I', read(destructor, 4))[0]
require(word & 0xfc000000 == 0x14000000 and
        '__ZN9IOServiceD2Ev' in thunk_names(relative_branch(destructor, word)), 'Unqualified actual base destructor')
require(all(read(pointers[i], 4) == struct.pack('<I', 0xd4200020) for i in (0, 1)),
        'Unexpected original abstract complete-destructor behavior')
function = pointers[7]
a, add, ret = struct.unpack('<3I', read(function, 12))
require(a & 0x9f00001f == 0x90000000 and add & 0xffc003ff == 0x91000000 and ret == 0xd65f03c0,
        'Unqualified original storage metaclass getter')
immediate = ((a >> 5 & 0x7ffff) << 2) | (a >> 29 & 3)
if immediate & 0x100000:
    immediate -= 0x200000
meta = (function & ~4095) + immediate * 4096 + (add >> 10 & 4095)
service_meta = by_name['__ZN9IOService10gMetaClassE']
# Check the genuine original metaclass registration instructions. This is a static
# constructor data-flow check, not a fabricated live metadata snapshot.
def adrp(pc, word, reg):
    require(word & 0x9f00001f == 0x90000000 | reg, 'Expected ADRP register')
    imm=((word>>5 & 0x7ffff)<<2)|(word>>29 & 3)
    if imm & 0x100000: imm-=0x200000
    return (pc & ~4095)+(imm<<12)
def adradd(pc, reg):
    first,second=struct.unpack('<2I',read(pc,8))
    require(second & 0xffc003ff == 0x91000000 | (reg<<5) | reg,'Expected exact ADD register')
    return adrp(pc,first,reg)+(second>>10 & 4095)
registrations=[]
for pc in range(text['va']+24,text['va']+text['size']-4,4):
    if read(pc,4)!=struct.pack('<I',0x52801203): continue # MOV w3,#0x90
    try:
        target=adradd(pc-24,0)
        if target!=meta:continue
        name=adradd(pc-16,1)
        require(read(name,21)==b'IOBlockStorageDevice\0','Original class name differs')
        first,load=struct.unpack('<2I',read(pc-8,8))
        require(load & 0xffc003ff==0xf9400042,'Expected original parent GOT load')
        slot=adrp(pc-8,first,2)+(load>>10 & 4095)*8
        require(struct.unpack('<Q',read(slot,8))[0]==service_meta,'Original parent is not IOService')
        bl=struct.unpack('<I',read(pc+4,4))[0]
        require(bl & 0xfc000000==0x94000000,'Expected metaclass constructor BL')
        require('__ZN11OSMetaClassC2EPKcPKS_j' in thunk_names(relative_branch(pc+4,bl)),
                'Original metaclass constructor import differs')
        registrations.append(pc-32)
    except (SystemExit,ValueError):continue
require(len(registrations)==1,'Cannot uniquely qualify original storage metaclass registration')
result = dict(public_source_revision=revision, source_sha256=hashlib.sha256((source / 'IOBlockStorageDevice.cpp').read_bytes()).hexdigest(),
              reference_object_sha256=hashlib.sha256(obj).hexdigest(), reference_object_executed_or_linked=False,
              platform_header_sdk_version=subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-version'], text=True).strip(),
              platform_header_sha256=hashlib.sha256(conditions.read_bytes()).hexdigest(),
              candidate_sha256=plan['candidate_sha256'], original_executable_code_unchanged=True,
              method_slots=190, kernel_bindings=matched, pure_virtual_slots=14, inherited_kernel_methods=162,
              vtable=hex(table), original_metaclass=hex(meta), source_ioservice_metaclass=hex(service_meta),
              original_protected_constructor=hex(constructors[0][0]), original_base_destructor=hex(destructor),
              abstract_complete_destructors_trap=True,
              static_original_class_size=0x90, static_parent_class='IOService',
              original_metaclass_registration=hex(registrations[0]), live_metadata_observed=False,
              compiled_public_class_size=int(class_size[1], 16),
              methods=[dict(slot=i, reference_name=name, candidate_target=hex(pointers[i]), kernel_names=exports.get(pointers[i], []))
                       for i, name in enumerate(methods)], disk_read_tested=False, transport_implemented=False, hardware_tested=False)
(out / 'interface-audit.json').write_text(json.dumps(result, indent=2) + '\n')

# Bind only the actual unmodified canonical RamBlock object after every slot audit.
audit=result
reference_path=kernel_path
canonical='RamBlock';component='NativeRamBlock';mangled='14NativeRamBlock'
obj_path = out / (component + '.o')
obj = obj_path.read_bytes()
image = np.macho(obj)
if audit['method_slots'] != 190 or not audit['abstract_complete_destructors_trap']:
    raise SystemExit('First qualify the exact original storage dispatch and base functions')
offset = next(at for cmd, at, size in image['commands'] if cmd == 2)
sy, count, st, _ = struct.unpack_from('<4I', obj, offset + 8)
symbols = []
for i in range(count):
    ix, kind, _, _, value = struct.unpack_from('<IBBHQ', obj, sy + i * 16)
    symbols.append((obj[st + ix:obj.index(b'\0', st + ix)].decode(), value, kind))
vtable = next(v for name, v, kind in symbols if name == '__ZTV' + mangled)
relocations = {}
for segment in image['segments']:
    for section in segment['sections']:
        if section['va'] <= vtable < section['va'] + section['size']:
            ro, rc = struct.unpack_from('<II', obj, section['command_offset'] + 56)
            for i in range(rc):
                address, word = struct.unpack_from('<II', obj, ro + i * 8)
                if word >> 27 & 1 and word >> 28 == 0 and word >> 25 & 3 == 3:
                    relocations[section['va'] + address] = symbols[word & 0xffffff][0]
compiled = [relocations.get(vtable + 16 + i * 8) for i in range(190)]
if not all(compiled) or vtable + 16 + 190 * 8 in relocations or '___cxa_pure_virtual' in compiled:
    raise SystemExit('Concrete block class has missing/additional/pure virtual methods')
all_names = list(dict.fromkeys(compiled + [r['reference_name'] for r in audit['methods']]))
demangled = subprocess.check_output(['xcrun', 'c++filt'], input='\n'.join(n[1:] for n in all_names) + '\n', text=True).splitlines()
readable = dict(zip(all_names, demangled))
overrides = 0
for row, actual in zip(audit['methods'], compiled):
    expected = row['reference_name']
    own = actual.startswith(('__ZN' + mangled, '__ZNK' + mangled))
    if expected == '___cxa_pure_virtual':
        if not own:
            raise SystemExit('A required transport operation is not implemented')
    elif actual != expected:
        if not own or (row['slot'] not in (0, 1) and readable[actual].split('::', 1)[1] != readable[expected].split('::', 1)[1]):
            raise SystemExit('Concrete subclass dispatch mismatch at slot ' + str(row['slot']))
    overrides += own
kernel_path = reference_path
# The binding values come from the recorded audit; the candidate only proves the audit's identity. Build products are pruned, so a
# missing candidate is accepted (the audit JSON is the pinned record) while a present one must still match.
if kernel_path.exists() and hashlib.sha256(kernel_path.read_bytes()).hexdigest() != audit['candidate_sha256']:
    raise SystemExit('Original binding input identity changed')
possible = {r['reference_name']: int(r['candidate_target'], 16) for r in audit['methods'] if not r['kernel_names']}
possible.update({'__ZN20IOBlockStorageDeviceC2EPK11OSMetaClass': int(audit['original_protected_constructor'], 16),
                 '__ZN20IOBlockStorageDeviceD2Ev': int(audit['original_base_destructor'], 16),
                 '__ZN20IOBlockStorageDevice10gMetaClassE': int(audit['original_metaclass'], 16),
                 '__ZTV20IOBlockStorageDevice': int(audit['vtable'], 16)})
undefined = [name for name, value, kind in symbols if kind == 1 and ('IOBlockStorageDevice' in name)]
if any(name not in possible for name in undefined):
    raise SystemExit('Unqualified original storage import')
# Abstract D1/D0 are BRK traps. Only actual base D2 is appropriate for this subclass.
if any(re.search(r'IOBlockStorageDeviceD[01]Ev$', n) for n in undefined):
    raise SystemExit('Concrete driver must not invoke abstract complete/deleting destruction')
bindings = {name: possible[name] for name in undefined}
assembly = out / 'original-bindings.S'
assembly.write_text(''.join(f'.globl {name}\n.set {name}, {value:#x}\n' for name, value in sorted(bindings.items())))
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-Werror', '-Wno-incompatible-sysroot',
                '-c', assembly, '-o', out / 'original-bindings.o'], check=True)
archive = out / ('lib' + canonical + '.a')
store_source = ROOT / 'kits/lmipro/LmiproRamOnlyStore.c'
store_object = out / 'LmiproRamOnlyStore.o'
subprocess.run(['xcrun','clang','-target','arm64-apple-ios17.0','-O2',
                '-ffreestanding','-fno-builtin','-fno-stack-protector','-mgeneral-regs-only',
                '-Wall','-Wextra','-Werror','-Wno-incompatible-sysroot',
                '-c',store_source,'-o',store_object],check=True)
store_data = store_object.read_bytes()
store_image = np.macho(store_data)
store_symbols = np.exports(store_data,store_image)
store_names = {name:address for address,names in store_symbols.items() for name in names}
store_expected = {'_native_ufs_store_'+name for name in ('mode','open','read','write','sync','gpt_check')}
require(store_expected <= store_names.keys(), 'All six exact optional-store signatures must be defined')
section = next(s for seg in store_image['segments'] for s in seg['sections'] if s['name']=='__text')
mode_at=section['fileoff']+store_names['_native_ufs_store_mode']-section['va']
require(struct.unpack_from('<2I',store_data,mode_at) in ((0x52800000,0xd65f03c0),(0x2a1f03e0,0xd65f03c0)),
        'RAM-only mode must return zero immediately without calls or hardware access')
archive.unlink(missing_ok=True)
transport=json.loads((out/'verified-transport-build.json').read_text())
require(transport['compiled_transport_object_sha256']==hashlib.sha256(obj).hexdigest(),'Actual adapted object must be the audited vtable input')
primitive=out/'LmiproVerifiedRamIo.o'
require(transport['read_primitive_object_sha256']==hashlib.sha256(primitive.read_bytes()).hexdigest(),'Actual authenticated-read primitive changed')
subprocess.run(['xcrun', 'ar', 'rcs', archive, obj_path, out / 'original-bindings.o',store_object,primitive], check=True)
(out / 'bindings.json').write_text(json.dumps(dict(original_candidate_sha256=audit['candidate_sha256'],
    compiled_object_sha256=hashlib.sha256(obj).hexdigest(), virtual_methods_checked=190,
    concrete_overrides=overrides, unresolved_original_imports=0,
    bindings={name: hex(value) for name, value in bindings.items()}, original_code_modified=False,
    actual_io_qualified=False,ram_only_store_mode=0,persistent_io_available=False,
    ram_only_store_source_sha256=hashlib.sha256(store_source.read_bytes()).hexdigest(),
    ram_only_store_object_sha256=hashlib.sha256(store_data).hexdigest(),
    ram_only_store_callbacks=sorted(store_expected)), indent=2) + '\n')

print('PASS: exact 190-slot static storage ABI and actual RamBlock imports; device I/O remains unqualified')

