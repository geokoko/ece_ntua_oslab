savedcmd_/home/user/shared/oslab/fs/ext2-lite.o := ld -m elf_x86_64 -z noexecstack   -r -o /home/user/shared/oslab/fs/ext2-lite.o @/home/user/shared/oslab/fs/ext2-lite.mod  ; ./tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --orc --retpoline --rethunk --static-call --uaccess --prefix=16  --link  --module /home/user/shared/oslab/fs/ext2-lite.o

/home/user/shared/oslab/fs/ext2-lite.o: $(wildcard ./tools/objtool/objtool)
