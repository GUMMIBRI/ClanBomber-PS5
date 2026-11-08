"""Relink ClanBomber with the PS5 application CRT and sign a native SELF."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess

parser = argparse.ArgumentParser()
for flag in ('sdk', 'llvm', 'build', 'output', 'converter'):
    parser.add_argument('--' + flag, type=Path, required=True)
args_cli = parser.parse_args()
s, llvm, build, n, converter = (getattr(args_cli, field).resolve()
    for field in ('sdk', 'llvm', 'build', 'output', 'converter'))
n.mkdir(parents=True, exist_ok=True)
support = Path(__file__).resolve().parent
root = support.parent
suffix = '.exe' if os.name == 'nt' else ''
subprocess.run([str(llvm/('clang++'+suffix)), '--target=x86_64-sie-ps5','-isysroot',str(s),'-stdlib++-isystem',str(s/'target/include/c++/v1'),'-fPIE','-fno-stack-protector','-fno-plt','-femulated-tls','-c',str(support/'app_crt.cpp'),'-o',str(n/'app_crt.o')],check=True)
subprocess.run([str(llvm/('clang++'+suffix)), '--target=x86_64-sie-ps5','-isysroot',str(s),'-stdlib++-isystem',str(s/'target/include/c++/v1'),'-fPIE','-fno-stack-protector','-fno-plt','-femulated-tls','-c',str(support/'title_fork.cpp'),'-o',str(n/'title_fork.o')],check=True)
subprocess.run([str(llvm/('clang++'+suffix)), '--target=x86_64-sie-ps5','-isysroot',str(s),'-stdlib++-isystem',str(s/'target/include/c++/v1'),'-fPIE','-fno-stack-protector','-fno-plt','-femulated-tls','-c','-I'+str(s/'target/user/homebrew/include'),str(root/'src/ps5_title_keyboard.cpp'),'-o',str(n/'title_keyboard.o')],check=True)
libs=re.search(r'^  LINK_LIBRARIES = (.+)$',(build/'build.ninja').read_text(),re.M)[1]
libs = shlex.split(libs)
args=['-pie','-e','_start','-z','max-page-size=0x4000','--hash-style=gnu','--no-dynamic-linker','--eh-frame-hdr','--gc-sections','--exclude-libs=ALL','--wrap=fork',*('--wrap='+x for x in ('PS5_Keyboard_Init','PS5_Keyboard_Open','PS5_Keyboard_PumpEvents','PS5_HasScreenKeyboardSupport','PS5_IsScreenKeyboardShown','PS5_ShowScreenKeyboard','PS5_HideScreenKeyboard')),'--version-script='+str(support/'app-symbols.map'),'-L'+str(support),'-T'+str(support/'ps5-pie-unwind.ld'),'-L'+str(s/'target/lib'),'-L'+str(s/'target/user/homebrew/lib'),'-o',str(n/'title.elf'),str(n/'app_crt.o'),str(n/'title_fork.o'),str(n/'title_keyboard.o'),*map(str,sorted((build/'CMakeFiles/clanbomber2.dir').rglob('*.o'))),'--start-group',*libs,'-lc++','-lc++abi','-lunwind','-lc','-lSceLibcInternal','-lSceNet','-lkernel','--end-group']
(n/'link.rsp').write_text('\n'.join('"'+x.replace('\\','/')+'"' for x in args))
subprocess.run([str(llvm/('ld.lld'+suffix)),'@'+str(n/'link.rsp')],check=True)
subprocess.run([str(converter), 'link', '--in', str(n/'title.elf'), '--out',
                str(n/'eboot.elf'), '--stub-dir', str(s/'target/lib'),
                '--file-name', 'eboot.elf'], check=True)
subprocess.run([str(converter), 'self', '--sign', '--in', str(n/'eboot.elf'),
                '--out', str(n/'eboot.bin')], check=True)
subprocess.run([str(converter), 'self', '--inspect', '--file',
                str(n/'eboot.bin')], check=True)
