#!/usr/bin/env python3
"""Identify the original generic framebuffer implementation and its abstract leaves."""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'kits/XnuPrelink'))
from native_prelink import macho,backed_offset,exports
def require(v,why):
    if not v:raise SystemExit(why)
path=ROOT/'build/pmgr-reference/lmipro-reference.macho'
FAMILY_TEXT_SHA='2e044ee68c1650397e1435234ca6459206262e4c7ed709cb7131c6f664f9bd9b'
blob=path.read_bytes();plan=json.loads(path.with_suffix('.json').read_text());image=macho(blob)
require(hashlib.sha256(blob).hexdigest()==plan['candidate_sha256'] and
        (plan['candidate_sha256']=='aded28d3c61dafaf7b8a7c96b14427a798e9e95470ae6f287bcd038c07626798' or
         plan['original_executable_sha256']['com.apple.iokit.IOMobileGraphicsFamily']==FAMILY_TEXT_SHA),
        'Exact previously qualified original-family candidate')
def read(va,n):
    at=backed_offset(image['segments'],va,n,len(blob));return blob[at:at+n]
names={n:v for v,group in exports(blob,image).items() for n in group}
family='com.apple.iokit.IOMobileGraphicsFamily'
text=next(e for e in plan['extents'] if e['module']==family and e['segment']=='__TEXT_EXEC')
require(hashlib.sha256(read(text['va'],text['size'])).hexdigest()==plan['original_executable_sha256'][family],
        'Complete original executable remains unchanged')
def vptr(pc):
    a,b,c=struct.unpack('<3I',read(pc,12))
    require(a&0x9f00001f==0x90000008 and b&0xffc003ff==0x91000108 and c==0x91004108,'Actual constructor ADRP/ADD/vptr header')
    imm=((a>>5)&0x7ffff)*4+((a>>29)&3)
    if imm&(1<<20):imm-=1<<21
    return (pc&~4095)+(imm<<12)+((b>>10)&4095)+16
def branch(pc):
    w=struct.unpack('<I',read(pc,4))[0];require(w&0xfc000000==0x94000000,'Actual direct constructor call')
    value=w&0x3ffffff;return pc+4*(value-(1<<26) if value&(1<<25) else value)
base_vptr=vptr(0xfffffff00577b324);service_vptr=vptr(0xfffffff00577b51c);legacy_vptr=vptr(0xfffffff0057865d8)
require(branch(0xfffffff0057865fc)==0xfffffff00577b510
        and branch(0xfffffff00577b540)==0xfffffff00577b318,'Actual protected Legacy -> Service -> Framebuffer constructor chain')
base=struct.unpack('<262Q',read(base_vptr,262*8));service=struct.unpack('<262Q',read(service_vptr,262*8))
require([i for i,(a,b) in enumerate(zip(base,service)) if a!=b]==[0,1,7],'Service adds no generic operational overrides')
legacy=struct.unpack('<358Q',read(legacy_vptr,358*8))
require(all(legacy) and read(legacy_vptr+358*8,24)==bytes(24),'Exact nonnull method extent and next aligned vtable boundary')
pure=[i for i,va in enumerate(legacy) if va==names['___cxa_pure_virtual']]
require(pure==[211,213,245,246,306,307,337,338,345],'Actual remaining abstract slots')
changed=[i for i,(a,b) in enumerate(zip(base,legacy)) if a!=b]
require(len(changed)==103 and [i for i in changed if i<168]==[0,1,7,14,21,44,79,85,86,87,111,141,155],
        'Actual original generic implementation overrides')
require(names['__ZN9IOService13newUserClientEP4taskPvjPP12IOUserClient']==base[141],
        'Actual newUserClient override occupies the native four-argument ABI slot')
for i,va in enumerate(legacy):
    if i>=168 and i not in pure:require(text['va']<=va<text['va']+text['size'],'Original family owns inherited generic method')

# Independent static metadata argument audit; no borrowed runtime evidence.
require(read(0xfffffff0045fbc47,26)==b'IOMobileFramebufferLegacy\0','Original metaclass identity')
require(read(0xfffffff0057866a4,24)==bytes.fromhex(
 'a173ffb0211c3191429d00f0420010910314835258700094'),
 'Original metadata class-name, parent and 0x18a0 extent arguments')
out=ROOT/'build/display-analysis/mobile-graphics-legacy-abi.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(dict(candidate_sha256=plan['candidate_sha256'],
 original_sha256=plan['original_sha256'],original_family_text_sha256=FAMILY_TEXT_SHA,
 base_vptr=hex(base_vptr),service_vptr=hex(service_vptr),legacy_vptr=hex(legacy_vptr),
 legacy_instance_bytes=0x18a0,legacy_virtual_entries=358,abstract_slots=pure,
 methods=[dict(slot=i,address=hex(va)) for i,va in enumerate(legacy)],
 static_metadata_arguments_checked=True,runtime_metadata_checked_on_handset=False),indent=2)+'\n')
print('PASS: exact original family, 358 slots, nine abstract leaves, constructor chain, metadata arguments')
