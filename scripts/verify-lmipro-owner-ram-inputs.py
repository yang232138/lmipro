"""Fail closed before final link if actual signed RAM transport inputs drift."""
from pathlib import Path
import hashlib, json, os, subprocess
ROOT = Path(__file__).resolve().parents[1]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert os.environ.get('XNU_LMIPRO_VERIFIED_RAM_ROOT') == '1'
for name in ('XNU_NATIVE_RAM_BLOCK','XNU_VERIFIED_ROOT_PROBE','XNU_OWNER_ROOT_AUDIT','XNU_OWNER_ROOT_TRUST'):
    assert os.environ.get(name) == '1', name
for name in ('XNU_NATIVE_UFS','XNU_NATIVE_VIRTIO_BLOCK','XNU_SOURCE_MOCKFS'):
    assert os.environ.get(name, '0') == '0', name
driver = ROOT/'build/ram-block'
transport = json.loads((driver/'verified-transport-build.json').read_text())
binding = json.loads((driver/'bindings.json').read_text())
crypto = ROOT/'build/verified-root-xnu'
inputs = json.loads((crypto/'inputs.json').read_text())
assert transport['read_only_transport'] and transport['per_read_verified_before_copy'] and transport['owner_trust_enabled']
assert transport['compiled_transport_object_sha256'] == binding['compiled_object_sha256'] == sha(driver/'NativeRamBlock.o')
assert transport['read_primitive_object_sha256'] == sha(driver/'LmiproVerifiedRamIo.o')
assert binding['virtual_methods_checked'] == 190 and binding['ram_only_store_mode'] == 0
assert not binding['persistent_io_available'] and not binding['original_code_modified']
members = subprocess.check_output(['xcrun','ar','-t',str(driver/'libRamBlock.a')], text=True).splitlines()
# macOS ar may include a generated symbol index; verify all actual object members.
object_members = {name for name in members if name.endswith('.o')}
assert object_members == {'NativeRamBlock.o','original-bindings.o','LmiproRamOnlyStore.o','LmiproVerifiedRamIo.o'}
assert inputs['owner_root_trust_enabled'] and inputs['official_probe_source_unmodified']
assert inputs['archive_sha256'] == sha(crypto/'libVerifiedRootProbe.a')
assert inputs['test_image_sha256'] == os.environ['LMIPRO_SIGNED_IMAGE_SHA256']
assert inputs['manifest_sha256'] == os.environ['LMIPRO_MANIFEST_SHA256']
assert inputs['trusted_key_sha256'] == os.environ['LMIPRO_TRUSTED_KEY_SHA256']
print('PASS: exact real readonly RAM, store mode 0, signed image/key, concrete ABI and archive members before final link')
