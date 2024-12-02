savedcmd_/home/user/shared/ece_ntua_oslab/lunix-tng/lunix.o := ld -m elf_x86_64 -z noexecstack   -r -o /home/user/shared/ece_ntua_oslab/lunix-tng/lunix.o @/home/user/shared/ece_ntua_oslab/lunix-tng/lunix.mod  ; ./tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --orc --retpoline --rethunk --static-call --uaccess --prefix=16  --link  --module /home/user/shared/ece_ntua_oslab/lunix-tng/lunix.o

/home/user/shared/ece_ntua_oslab/lunix-tng/lunix.o: $(wildcard ./tools/objtool/objtool)
