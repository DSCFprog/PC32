;;;; PC32 COLORFORTH V 1.0 ;;;;
bits 32


; VM SPEC
; 32bit VM
; calling C saves ebx, esi, edi, ebp
; data stack on esi, grows down
; eax TOS
; ecx, edx scratch (r1, r2)
; B on edi (because we store on B)
; A on ebx


; COMPILER SPEC


%define LINK 0
%define OLDLINK 0


%macro CODE 2
	%xdefine OLDLINK LINK
	%xdefine LINK %2
	%strlen len %1
	align 4
	dd OLDLINK
	db %1
	times (12-len) db 0
	%2:
%endmacro

%macro OCALL 0		

%endmacro

%macro DUP 0			
	sub esi, 4				; NUP make space for new data
	mov [esi], eax
%endmacro


global comp
global DATABLK
global HEIGHT
global WIDTH
global CDATA

extern cprint

; DATA BEGIN
section .data exec
align	256
CSTK		dd 0x0						; stack position at COLD
CEBP		dd 0x0
HERE		dd 0x0						; compilation address
L			dd 0x0						; last dict entry
errorfn	dd error_dummy
wbuf		times 32 db 0x0			; word buf
WIDTH		dd 1024 ;1280					
HEIGHT	dd 768; 1024
CDATA		times 256 dd 0x0			; C&GL call data
align 4096
DATABLK	incbin "data/alloc.bin"	; 32M memory
align 4096
PADM		incbin "data/padmem.bin"
VMEM		incbin "data/vmem.bin"
SINE		incbin "data/sine.bin"

dq 0

; CODE BEGIN
section .text write

error_dummy:
	ret

CODE "O:O:", dofirst				; first word never gets searched since it points to 0
	ret

CODE "brk", dobreak				; for break points
	ret 
	ret

CODE "L", dolatest
	DUP
	mov eax, [L]
	ret
	ret

CODE "LINK", dolink
	DUP
	mov eax, L
	ret
	ret

CODE "C#", docdata
	DUP
	mov eax, CDATA
	ret
	ret

CODE "DATABLK", dodatablk
	DUP 
	mov eax, DATABLK
	ret 
	ret

CODE "VMEM", dovmem
	DUP
	mov eax, VMEM
	ret
	ret

CODE "ERRFN", doerrorfn
	DUP
	mov eax, errorfn
	ret
	ret

CODE "SINT", dosinetable
	DUP
	mov eax, SINE
	ret
	ret


CODE "isqrt", doinvsqrt		; copy pasted from godbolt, mulss goes faster directly from memory?
	cvtsi2ss        xmm0, eax
	mov     ecx, 1597463007
	mulss   xmm0, dword [.LCPI0_0]
	movd    eax, xmm0
	mulss   xmm0, dword [.LCPI0_1]
	sar     eax, 1
	sub     ecx, eax
	movd    xmm1, ecx
	mulss   xmm0, xmm1
	mulss   xmm0, xmm1
	addss   xmm0, dword [.LCPI0_2]
	mulss   xmm0, xmm1
	mulss   xmm0, dword [.LCPI0_3]
	cvttss2si       eax, xmm0
	ret 
	ret
.LCPI0_0:	dd		964689920               ; float 2.44140625E-4
.LCPI0_1:	dd		3204448256              ; float -0.5
.LCPI0_2:	dd		1069547520              ; float 1.5
.LCPI0_3:	dd		1166016512              ; float 4096


CODE "simdtest", dosimdtest
	movdqu xmm0, [eax]
	lodsd 
	movdqu xmm1, [eax]
	paddd xmm0, xmm1
	lodsd 
	movups [eax], xmm0
	ret 
	ret


;; COMPILER ;;
; 1 byte tag to ease text
; 3 byte + 1 comment tag for cursor pos storage
; 1024 byte blocks but could be longer
; most compiler words can be called inside forth
; edi is used to hold HERE for compilation
; ebx pushed and holds SRC since we never use the A&B in compilation
; colors:
;	COMP				IMM			LEN/FLG
;	0	DEF		8	,JMP			12
;	1	CALL		9	IMM CALL		12
;	2	NUM		A	IMM NUM		HEX | FL | LEN 2b (no NEG)
;	3	REF		B	IMM REF		16
;	4	DATA		C	IMM DATA		16 (blue changes display)
;	5	ASM		D	MACRO			HEX | BASE5 | LEN 2b (keeps compat with SHBA)
;	6	EDIT		E 	COMMENT		same as text
;	7	---		F	---			

%macro TAL 0
	mov ecx, [ebx]
	and ecx, 0xF0
	shr ecx, 4
%endmacro

getlen:
	TAL
	cmp cl, 2
	je .n
	cmp cl, 5
	je .n
	cmp cl, 10
	je .n
	mov cl, [ebx]
	and ecx, 0x0F
	jmp .done
.n:
	mov cl, [ebx]
	and ecx, 3
	cmp cl, 3
	je .add1
	jmp .done
.add1:	add cl, 1
.done:
	ret

%macro TAG 0
	mov cl, [ebx]
	and ecx, 0xF0
	shr ecx, 4
%endmacro

%macro NEXTW 0
	call getlen
	add ebx, ecx
	inc ebx
	call cword
%endmacro


zerobuf:	
	push eax
	push edi
	mov eax, 0
	mov edi, wbuf			; zero wbuf
	stosd
	stosd
	stosd
	stosd
	pop edi
	pop eax
	ret

cword:
	call zerobuf
	push esi
	push edi
	mov esi, ebx			; src
	inc esi
	mov edi, wbuf
	call getlen
	rep movsb
	pop edi
	pop esi
	ret

; answer in ecx
find:							; word already in wbuf
	push esi
	push edi
	mov esi, [L]
.search:
	sub esi, 16				; 1st word links to nothing
	cmp dword [esi], 0
	je .n
	push esi
	add esi, 4				; name loc
	mov edi, wbuf			; init search
	mov ecx, 3
	rep cmpsd
	pop esi
	jz .found
	mov esi, [esi]			; next word
	jmp .search
.found:
	add esi, 16
	mov ecx, esi
	jmp .e
.n mov ecx, 0
	pop edi
	pop esi
	jmp cferror
.e pop edi
	pop esi
	ret

CODE "ERROR", cferror
	mov ecx, [errorfn]
	call ecx
	; jmp cmanage
cmanage:
	DUP
	mov ecx, wbuf
	push ecx
	call cprint
	mov ecx, HERE
	mov [ecx], edi		; store new HERE
	mov eax, esi
	pop edi
	pop ebx
	pop ecx				; 0xDEADBABE
	ret

cftag:
	push eax
	push esi
	mov eax, 0x10EB		; 16 byte jump
	stosw
	mov eax, [L]
	stosd						; pointer to LATEST
	mov esi, wbuf
	movsd
	movsd
	movsd
	mov eax, L
	mov [eax], edi			; update latest
	pop esi
	pop eax
	ret

cfjmp:						
	call find				; ecx
	mov byte [edi], 0xE9	; jmp rel32
storecall:
	inc edi					
	sub ecx, edi			; distance
	sub ecx, 4				; instruction offset
	mov [edi], ecx
	add edi, 4
	ret
	
cfcall: 	
	call find
	mov byte [edi], 0xE8 ; call rel32
	jmp storecall


immcall:
	call find
	jmp ecx
	ret

num1: xor ecx, ecx
	mov cl, byte [ebx+1]
	ret
num2: xor ecx, ecx
	mov cx, word [ebx+1]
	ret
num4: mov ecx, [ebx+1]
	ret

numtbl: dd cferror, num1, num2, num4, num4
getnum:
	call getlen
	shl ecx, 2
	add ecx, numtbl
	jmp [ecx]

cfnum:
	call getnum					; ecx
storeLIT:
	push eax
	mov eax, 0x8904EE83		; DUP & B8(load next word into eax)
	stosd
	mov eax, 0xB806
	stosw
	mov eax, ecx
	stosd
	pop eax
	ret 

immnum:
	call getnum
	DUP
	mov eax, ecx
	ret

cfref:
	call find
	jmp storeLIT
immref:
	call find
	DUP
	mov eax, ecx
	ret

cfdata:
	mov ecx, ebx
	inc ecx
	jmp storeLIT
immdata:
	DUP
	mov eax, ebx
	inc eax
	ret
	
iasm1: mov byte [edi], cl
	inc edi
	ret
iasm2: mov word [edi], cx
	add edi, 2
	ret
iasm4: mov dword [edi], ecx
	add edi, 4
	ret

asmtbl: dd cferror, iasm1, iasm2, iasm4, iasm4
immasm:
	call getlen
	shl ecx, 2
	mov edx, asmtbl
	add edx, ecx
	call getnum
	jmp [edx]


CODE "macro", domacro
	push eax
	mov ecx, eax
	jmp cfmacro.loop
cfmacro:
	call find			; ecx
	push eax
.loop:
	mov eax, [ecx]
	and eax, 0xFFFF
	cmp eax, 0xC3C3
	je .done
	stosb
	inc ecx
	jmp .loop
.done:
	pop eax

cfcomment: ret

align 32
jmptbl:
	dd cftag, cfcall, cfnum, cfref, cfdata, immasm, cfcomment, cfcomment
	dd cfjmp, immcall, immnum, immref, immdata, cfmacro, cfcomment, cfcomment

CODE "eval", doeval	
	call getlen
	cmp ecx, 0
	jz .done
	TAG
	shl ecx, 2
	mov edx, jmptbl
	add edx, ecx
	call [edx]
	NEXTW
	jmp doeval
.done:
	ret
	ret


CODE "bye", dobye
	DUP
	mov eax, esi
	mov ebp, [CEBP]
	mov esp, [CSTK]
	ret

CODE "COLD", doreset
	mov ebp, [CEBP]
	mov esp, [CSTK]
comp:						; compiler setup
	mov edx, L
	mov dword [edx], LINK	; set latest properly
	mov edx, CSTK
	mov [edx], esp		; store stack for debugging
	mov edx, CEBP
	mov [edx], ebp		; to avoid segfaulting
	mov esi, esp		
	add esi, 512		; data stack
	mov ebx, HERE
	mov [ebx], dword DATABLK+((2880*1024)+1024)	; store reset HERE
	mov ebx, 0	
	mov edx, 0

	mov eax, 0xCAFEBABE
	push eax
	mov eax, 0xDEADBABE
	
	mov ebx, DATABLK+2048
	mov edi, [HERE]
	call doeval
.done:	
	pop ecx
	DUP
	mov eax, esi
	ret

START:

testesi:

