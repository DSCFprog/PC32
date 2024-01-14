# PC32

PC32 is a 32bit graphically oriented computer language that allows for a fast development cycle.
Based off the original ColorFORTH, the language implements its own editor, 
an interpreter, a compiler and a simple but fairly powerful x86 assembler.

## Specs
### Machine
The language runs natively managing the computer registers directly.
These are arranged as follows:
- EAX: Top of Data Stack, used in arithmetic, logic and pretty much everything.
- ECX: Second parameter register, used in a complementary manner alongside EAX.
- EDX: TMP register.
- EBX: A register, used to read memory incrementally. It also holds the pointer to the source code being compiled.
- ESI: Data Stack pointer
- EDI: B register, used to store memory incrementally. It also holds the pointer to the memory being compiled to.
- ESP: Call Stack pointer, used as in any C application would(return addresses, temp storage).
- EBP: Offset Memory register, used to access memory as an array.

### Memory
PC32 has 32MB of available memory fixed at at anytime at address 0x08052000, the programmer is free to use it as they please.
**malloc** can be used to get more memory but it has to be managed using the **free** word. 

### Source Code
Source is held in memory at all times, its divided in blocks of 1024 bytes.
Replaces the **space(char 32)** for a tag containing function/color and length, 
it also encodes numbers as their direct byte representation while letters are kept as traditional ASCII.

## Build Instructions
PC32 is strictly a 32bit application and as such it needs a 32bit environment in order to be built,
in linux you'll need multilib support.

### Additional requirements
- libmikmod(compiled for 32bit) for sound, you can turn this off by taking out -llibmikmod in the makefile
- NASM
- GLFW

NASM and GLFW dont have to be 32bit as far as I understand.

In order to build it just:
- Activate the multilib environment for your shell 
- go to PC32 directory
- make

The whole thing should also build for Windows and run natively, 
but I haven't been able to get it to build successfully probably due to library linking. 
If you get it working please do write to me, I'll add it to the main release.

## Usage
After successfully building the language you'll have a fairly large executable file due to itself containing the
32MB of available memory. 
The executable reads the data.blk file which contains the source code, **make sure to make a backup of the data.blk file**
It should boot on block 300 which is the tutorial.

Press F5 to execute a block, the source and block you're in gets automatically saved anytime you execute.



