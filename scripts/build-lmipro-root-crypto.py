"""Build the official eight-control XNU crypto probe for the actual RAM test seed.

Adapted from UniXNU build-verified-root-probe.py at
ee472385743ebd16941ab14892dcb9d02e219d15. No root authorization is enabled.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess
ROOT=Path(__file__).resolve().parents[1]
policy=Path(os.environ['LMIPRO_ROOT_POLICY_DIR']).resolve()
out=ROOT/'build/verified-root-xnu';out.mkdir(parents=True,exist_ok=True)
metadata=json.loads((policy/'verification.json').read_text())
manifest=policy/'manifest.bin';key=policy/'trusted-key.bin';samples=policy/'samples.bin'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert metadata['real_c_signature_and_block_verification'] and metadata['blocks_verified']==65526
assert metadata['image_bytes']==268394496 and metadata['image_sha256']=='ba060c84ae26166f3068dcb8a5b338b6ddeb539f872f40d46482639b74997557'
assert metadata['volume_uuid']=='B76FFF45-A38F-4850-B66C-A5B6547A77D7'
assert metadata['complete_os'] is False and metadata['boot_root_authenticated'] is False
assert metadata['verified_root_revision']=='def026642c6c6e71be0dea267d989df1d29ddc64'
assert sha(key)==metadata['trusted_key_sha256'] and sha(manifest)==metadata['manifest_sha256']
assert sha(key)=='95b218c864db0787a303ec5540c51660e8ed2348f43157ae683f349be2cf5105'
assert sha(manifest)=='8d8836c00d21b290c13b87ce9913d9f93d0cbf3a88d8d156f2511af7f031e229'
assert sha(samples)==metadata['samples_sha256'] and samples.stat().st_size==16384
indices=metadata['actual_sample_indices'];lengths=metadata['actual_sample_lengths']
assert indices==[0,1,32763,65525] and lengths==[4096]*4
assert manifest.stat().st_size==160+384+65526*32 and key.stat().st_size==395
host=json.loads((policy/'host-checks.json').read_text())
assert host['real_c_parser'] and host['real_openssl_signature_verification'] and host['native_arm64_ios17_cross_compile']
assert len(host['checks'])==7
assembly=out/'inputs.S'
assembly.write_text('''.section __DATA_CONST,__const
.balign 8
.globl _verified_root_manifest, _verified_root_manifest_bytes
_verified_root_manifest:
.incbin "'''+str(manifest)+'''"
_verified_root_manifest_bytes:
.quad . - _verified_root_manifest
.globl _verified_root_key
_verified_root_key:
.incbin "'''+str(key)+'''"
.balign 8
.globl _verified_root_sample_indices
_verified_root_sample_indices:
.quad '''+','.join(map(str,indices))+'''
.globl _verified_root_sample_lengths
_verified_root_sample_lengths:
.quad '''+','.join(map(str,lengths))+'''
.globl _verified_root_samples
_verified_root_samples:
.incbin "'''+str(samples)+'"\n')
record=json.loads((ROOT/'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT/bsd_init.o.json').read_text().strip().rstrip(','))
objects=[]
for source in [ROOT/'kits/VerifiedRoot/src/verified_root.c',ROOT/'kits/VerifiedRoot/adapters/xnu_crypto.c',ROOT/'scripts/qemu-entry/verified-root-probe.c']:
    destination=out/(source.stem+'.o');args=record['arguments'];command=[];i=0
    while i<len(args):
        value=args[i]
        if value==record['file']:command.append(str(source))
        elif value=='-o':command+=['-o',str(destination)];i+=1
        elif value in ('-MF','-MJ'):i+=1
        elif value not in ('-MD','-MP'):command.append(value)
        i+=1
    command+=['-I'+str(ROOT/'kits/VerifiedRoot/include'),'-Werror']
    r=subprocess.run(command,cwd=record['directory'],capture_output=True,text=True)
    (out/(source.stem+'.log')).write_text(r.stdout+r.stderr)
    if r.returncode:raise SystemExit(r.stderr[-4000:])
    objects.append(destination)
sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
subprocess.run(['xcrun','clang','-target','arm64-apple-ios17.0','-isysroot',sdk,'-c',assembly,'-o',out/'inputs.o'],check=True)
objects.append(out/'inputs.o')
for obj in objects:
    data=obj.read_bytes();assert struct.unpack_from('<4I',data)==(0xfeedfacf,0x100000c,0,1)
    at=32;deployment=None
    for _ in range(struct.unpack_from('<I',data,16)[0]):
        cmd,size=struct.unpack_from('<II',data,at)
        if cmd==0x32:deployment=struct.unpack_from('<II',data,at+8)
        at+=size
    assert deployment==(2,0x110000)
archive=out/'libVerifiedRootProbe.a';archive.unlink(missing_ok=True)
subprocess.run(['xcrun','ar','rcs',archive,*objects],check=True)
report=dict(manifest_sha256=sha(manifest),manifest_bytes=manifest.stat().st_size,
    trusted_key_sha256=sha(key),samples_sha256=sha(samples),archive_sha256=sha(archive),
    test_image_sha256=metadata['image_sha256'],test_image_bytes=metadata['image_bytes'],
    test_image_block_count=65526,actual_sample_indices=indices,actual_sample_lengths=lengths,
    host_checks_passed=len(host['checks']),official_probe_source_unmodified=True,
    verified_root_revision='def026642c6c6e71be0dea267d989df1d29ddc64',
    complete_os=False,boot_root_authenticated=False,owner_root_trust_enabled=False,
    real_xnu_crypto_observed=False,hardware_tested=False)
(out/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: official XNU crypto bridge and eight-control probe built with measured signed RAM-test inputs; no root acceptance')
