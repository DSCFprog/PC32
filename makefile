CC = gcc
CFLAGS = -w -g -O0 -c -Wno-format
LD = -lGL -lGLU -lmikmod -lpthread -lglfw

dat:= 's/!!DATE/$(shell date -Ihours)/g'
winstr = 's/RR1/rcx/g ; s/RR2/rdx/g ; s/RR3/r8/g ;\
         s/RR4/r9/g ; s/;win64//g ; $(dat)'
linstr = 's/RR1/rdi/g ; s/RR2/rsi/g ; s/RR3/rdx/g ; s/RR4/rcx/g;\
         $(dat)'

build: asm
	$(CC) $(CFLAGS) *.c
	$(CC) $(LD) main.o cf.o
	rm main.o
	rm cf.o

asm:
	nasm -g -felf32 cf.s -l cf_asm.txt -o cf.o

run: build
	./a.out

debug: build
	ddd a.out

32bit:
	source /etc/profile.d/32dev.sh

