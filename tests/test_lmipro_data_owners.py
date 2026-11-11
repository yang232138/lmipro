from pathlib import Path
import tempfile,subprocess,os
device=Path(__file__).resolve().parents[1]
patch=(device/'patches/xnu/lmipro-data-mobile-owners.patch').read_text()
first=patch.split('\n@@',2)[1]
code='\n'.join(line[1:] for line in first.splitlines() if line.startswith('+'))+'\n'
assert 'lmipro_data_mobile_owners' in code and code.count('vnode_setattr(')==1
with tempfile.TemporaryDirectory() as work:
 p=Path(work);(p/'owner-helper.c').write_text(code)
 exe=p/('test.exe' if os.name=='nt' else 'test')
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I',str(p),str(device/'tests/test_lmipro_data_owners.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
