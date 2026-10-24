import pathlib,shutil,subprocess,sys
root=pathlib.Path(__file__).resolve().parent
if not sys.platform.startswith('linux'):sys.exit('Run this build inside Ubuntu/WSL, not Windows Python.')
cc=shutil.which('gcc') or shutil.which('clang')
if not cc:sys.exit('Install build-essential first.')
(root/'build').mkdir(exist_ok=True)
flags=['-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2','-I',str(root/'src')]
for name,src in [('ecu','src/ecu.c'),('test_protocol','tests/test_protocol.c')]:
 subprocess.run([cc,*flags,str(root/'src/protocol.c'),str(root/src),'-o',str(root/'build'/name)],check=True)
subprocess.run([str(root/'build/test_protocol')],check=True)
print('Build passed. Run: python3 tests/test_vcan.py vcan0, then python3 dashboard.py vcan0')
