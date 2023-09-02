.code64
.intel_syntax noprefix

/* to make the linker shut the hell up */
.section .note.GNU-stack

.section .text

.global thrd_Switch
thrd_Switch:
    push rbp
    mov rbp, rsp
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov qword ptr[rdi], rsp

    mov rsp, qword ptr[rsi]
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rbp
    ret

