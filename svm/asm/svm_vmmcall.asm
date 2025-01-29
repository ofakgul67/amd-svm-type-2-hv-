.code

pushaq macro
        push    rax
        push    rcx
        push    rdx
        push    rbx
        push    -1
        push    rbp
        push    rsi
        push    rdi
        push    r8
        push    r9
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
        endm
popaq macro
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
        pop     rdi
        pop     rsi
        pop     rbp
        pop     rbx    
        pop     rbx
        pop     rdx
        pop     rcx
        pop     rax
endm

svm_vmmcall proc	
    pushaq
	lea r11, [rsp+20h+88h]	; 5th param
	mov r11, qword ptr [r11]
    lea r12, [rsp+28h+88h]	; 6th param
	mov r12, qword ptr [r12]
	vmmcall
    popaq
	ret	
svm_vmmcall endp

end