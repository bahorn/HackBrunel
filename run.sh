#!/bin/bash
. ./edksetup.sh
build -p ./ShellPkg/ShellPkg.dsc
#uefi-run -b ./Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd ./Build/Shell/DEBUG_GCC5/X64/HackBrunel.efi
cp ./Build/Shell/DEBUG_GCC5/X64/HackBrunel.efi ./hda-contents/main.efi
ls HackBrunel/images | xargs -I "{}" -n 1 python ./HackBrunel/convert.py `pwd`/HackBrunel/images/{} `pwd`/hda-contents/{}.dat
qemu-system-x86_64 -cpu qemu64 --bios ./Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd -hda fat:rw:hda-contents -net none
