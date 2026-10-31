#!/usr/bin/env python3
"""Append the explicitly selected board observer to its existing, tracked board archive."""
import hashlib,json,os,subprocess
from pathlib import Path

if os.environ.get('XNU_SHENG_PMGR','0')!='1':
    print('Board UFS observer not linked in the flag-off reference')
    raise SystemExit(0)
root=Path(__file__).resolve().parents[1];out=root/'build/lmipro-ufs-handoff';out.mkdir(parents=True,exist_ok=True)
source=root/'kits/lmipro/LmiproUfsHandoff.cpp';obj=out/'LmiproUfsHandoff.o'
record=json.loads((root/'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json').read_text().strip().rstrip(','))
arguments=record['arguments'];command=[];i=0
while i<len(arguments):
    v=arguments[i]
    if v==record['file']:command.append(str(source))
    elif v=='-o':command+=['-o',str(obj)];i+=1
    elif v=='-Xclang' and arguments[i+1].startswith('-W'):i+=1
    elif not v.startswith('-W'):command.append(v)
    i+=1
command+=['-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-c99-designator','-Wno-reorder-init-list','-Wno-deprecated-enum-enum-conversion','-Wno-unnecessary-virtual-specifier']
subprocess.run(command,cwd=record['directory'],check=True,timeout=90)
symbols=subprocess.check_output(['llvm-nm','--defined-only',str(obj)],text=True)
assert '_lmipro_ufs_handoff_probe' in symbols and '_lmipro_ufs_handoff_observation' in symbols
text=source.read_text()
assert 'const volatile uint32_t*' in text and 'writeWord' not in text and 'write32' not in text
archive=root/'build/sheng-platform-power/libShengPlatformPower.a'
proofpath=archive.parent/'build-report.json';pmgr=json.loads(proofpath.read_text())
assert pmgr['virtual_entries']==340 and not pmgr['original_executable_modified']
assert hashlib.sha256(archive.read_bytes()).hexdigest()==pmgr['archive_sha256']
original_sha=pmgr['archive_sha256']
subprocess.run(['xcrun','ar','rcs',str(archive),str(obj)],check=True,timeout=90)
combined=hashlib.sha256(archive.read_bytes()).hexdigest()
pmgr['archive_sha256']=combined
pmgr['auxiliary_readonly_ufs_observer']=dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),pmgr_archive_before_observer=original_sha)
proofpath.write_text(json.dumps(pmgr,indent=2)+'\n')
proof=dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),combined_archive_sha256=combined,pmgr_archive_before_observer=original_sha,hardware_register_writes=False,storage_commands_submitted=False,clock_power_or_smmu_state_changed=False,runtime_default=0,test_mode=1,source_reference='Linux v6.12 gcc-sm8250.c and sm8250.dtsi; measured lmipro FDT windows',compiler_record=record)
(out/'build-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps({k:v for k,v in proof.items() if k!='compiler_record'},indent=2))
