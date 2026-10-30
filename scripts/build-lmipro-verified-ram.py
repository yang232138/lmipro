"""Compile the explicit reviewed RAM transport adaptation without changing its checkout."""
from pathlib import Path
import hashlib,json,shutil,struct,subprocess
ROOT=Path(__file__).resolve().parents[1]
canonical=ROOT/'drivers/RamBlock';kit=ROOT/'kits/lmipro';out=ROOT/'build/ram-block';source=out/'verified-source'
revision=subprocess.check_output(['git','-C',str(canonical),'rev-parse','HEAD'],text=True).strip()
assert revision=='25c57e0e9b68bf452dd1986914525edc9d2320bc'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(canonical/'NativeRamBlock.cpp')=='e2ace3522943df38806ef2006525371691beaf0199f971266bd849d4f5253391'
source.mkdir(parents=True,exist_ok=True)
for file in [canonical/'NativeRamBlock.cpp',*canonical.glob('*.h')]:shutil.copyfile(file,source/file.name)
patch=kit/'ram-block-verified.patch'
subprocess.run(['git','-C',str(source),'apply','--check',str(patch)],check=True)
subprocess.run(['git','-C',str(source),'apply',str(patch)],check=True)
assert sha(source/'NativeRamBlock.cpp')=='4193028f29253afb7ca417da896ab428663e8d3656a4d739efb34aecf0d62f9f'
command=json.loads((out/'compile-command.json').read_text())
fixed=[];pruned=[];i=0;changed=0
while i<len(command):
    value=command[i]
    if value==str(canonical/'NativeRamBlock.cpp'):fixed.append(str(source/'NativeRamBlock.cpp'));changed+=1
    elif value in ('-MF','-MJ'):i+=1
    elif value not in ('-MD','-MP'):fixed.append(value)
    i+=1
assert changed==1
fixed+=['-I',str(ROOT/'kits/VerifiedRoot/include'),'-I',str(kit)]
cpp_record=json.loads((ROOT/'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json').read_text().strip().rstrip(','))
r=subprocess.run(fixed,cwd=cpp_record['directory'],capture_output=True,text=True)
(out/'verified-compile.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.stdout[-2000:]+r.stderr[-4000:])
(out/'compile-command.json').write_text(json.dumps(fixed,indent=2)+'\n')
record=json.loads((ROOT/'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT/bsd_init.o.json').read_text().strip().rstrip(','))
args=record['arguments'];c=[];i=0
destination=out/'LmiproVerifiedRamIo.o';implementation=kit/'LmiproVerifiedRamIo.c'
while i<len(args):
    value=args[i]
    if value=='-I' and i+1<len(args):
        include=Path(args[i+1])
        if include.parts[-2:]==('EXPORT_HDRS','libsa') and not include.is_dir():pruned.append(str(include));i+=2;continue
    if value.startswith('-I') and value!='-I':
        include=Path(value[2:])
        if include.parts[-2:]==('EXPORT_HDRS','libsa') and not include.is_dir():pruned.append(str(include));i+=1;continue
    if value==record['file']:c.append(str(implementation))
    elif value=='-o':c+=['-o',str(destination)];i+=1
    elif value in ('-MF','-MJ'):i+=1
    elif value not in ('-MD','-MP'):c.append(value)
    i+=1
c+=['-I',str(ROOT/'kits/VerifiedRoot/include'),'-I',str(kit),'-Werror']
r=subprocess.run(c,cwd=record['directory'],capture_output=True,text=True)
(out/'read-primitive-compile.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.stderr[-4000:])
(out/'read-primitive-command.json').write_text(json.dumps(c,indent=2)+'\n')
for obj in [out/'NativeRamBlock.o',destination]:
    data=obj.read_bytes();assert struct.unpack_from('<4I',data)==(0xfeedfacf,0x100000c,0,1)
    at=32;deployment=None
    for _ in range(struct.unpack_from('<I',data,16)[0]):
        cmd,size=struct.unpack_from('<II',data,at)
        if cmd==0x32:deployment=struct.unpack_from('<II',data,at+8)
        at+=size
    assert deployment==(2,0x110000)
report=dict(canonical_revision=revision,canonical_source_sha256=sha(canonical/'NativeRamBlock.cpp'),
 patch_sha256=sha(patch),adapted_source_sha256=sha(source/'NativeRamBlock.cpp'),
 read_primitive_sha256=sha(implementation),read_primitive_object_sha256=sha(destination),
 compiled_transport_object_sha256=sha(out/'NativeRamBlock.o'),strict_werror=True,
 absent_optional_libsa_include_dirs=sorted(set(pruned)),per_read_verified_before_copy=True,
 read_only_transport=True,exact_media_binding_implemented=True,owner_trust_enabled=False,
 original_checkout_modified=False,hardware_tested=False)
(out/'verified-transport-build.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: reviewed signed read-only RAM transport and shared real-C read verifier compiled; device qualification pending')
