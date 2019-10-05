gcc -c cpu.c -o cpu.o
gcc -c ppu.c -o ppu.o
gcc -c rom.c -o rom.o
gcc -c main.c -o main.o
gcc cpu.o ppu.o rom.o main.o -o snes.exe