"""Reuse the qualified PMGR candidate as an ABI reference, never as the new output."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
source=Path(os.environ['LMIPRO_REFERENCE_DIR'])
cache=Path(os.environ['LMIPRO_ORIGINAL_CACHE'])
reference=ROOT/'build/pmgr-reference/lmipro-reference.macho'
reference.parent.mkdir(parents=True,exist_ok=True)
expected='49334c982a4029097f841d234cd03bd2cd3d9d017d067ce79cef88c719ca613c'
assert hashlib.sha256((source/'kernel-original.macho').read_bytes()).hexdigest()==expected
plan=json.loads((source/'kernel-prelink.json').read_text())
assert plan['candidate_sha256']==expected
assert plan['original_sha256']=='f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7'
assert hashlib.sha256(cache.read_bytes()).hexdigest()==plan['original_sha256']
shutil.copyfile(source/'kernel-original.macho',reference)
shutil.copyfile(source/'kernel-prelink.json',reference.with_suffix('.json'))
original=ROOT/'.cache/firmware/kernelcache.macho/kernelcache.release.ipad7.decompressed'
original.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(cache,original)
subprocess.run([sys.executable,str(ROOT/'scripts/audit-amfi-pmgr-contract.py'),'--candidate',str(reference)],check=True)
(reference.parent/'reference-inputs.json').write_text(json.dumps(dict(
    reused_qualified_reference=True,reference_run=40966779440,
    reference_sha256=expected,reference_runtime_mode_tested=1,
    reference_hardware_result='PMGR lifecycle PASS; publication-mode image reached root device discovery',
    original_cache_sha256=plan['original_sha256'],fresh_reference_from_current_job=False,
    reference_is_new_kernel_output=False),indent=2)+'\n')
