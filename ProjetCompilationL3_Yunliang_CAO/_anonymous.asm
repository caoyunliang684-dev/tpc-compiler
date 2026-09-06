; ========= builtin =========
; ======== builtin: clrbuf ========
global clrbuf
clrbuf:
    push    rbp
    mov     rbp, rsp
.read_more:
    call    getchar
    test    rax, rax
    je      .done_clrbuf
    cmp     al, 10
    je      .done_clrbuf
    cmp     al, 13
    je      .done_clrbuf
    jmp     .read_more
.done_clrbuf:
    mov     rsp, rbp
    pop     rbp
    ret

; ======== builtin: getchar ========
global getchar
getchar:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 1
    mov     rax, 0
    mov     rdi, 0
    mov     rsi, rsp
    mov     rdx, 1
    syscall
    cmp     rax, 1
    jne     .fail_getchar
    xor     eax, eax
    mov     al, [rsp]
    add     rsp, 1
    mov     rsp, rbp
    pop     rbp
    ret
.fail_getchar:
    add     rsp, 1
    mov     rsp, rbp
    pop     rbp
    mov     rax, 0
    ret

; ======== builtin: getint ========
global getint
getint:
    push    rbp
    mov     rbp, rsp
    push    r12
    push    r13
    xor     r12, r12
    xor     r13, r13
read_first:
    call    getchar
    cmp     rax, 0
    je      io_fail
    cmp     al, '-'
    je      minus_sign
    cmp     al, '+'
    je      plus_sign
    jmp     check_digit_first
minus_sign:
    mov     r13, 1
    jmp     need_digit
plus_sign:
    jmp     need_digit
bad_first:
    jmp     io_fail
io_fail:
    mov     rax, 60
    mov     rdi, 5
    syscall
need_digit:
    call    getchar
    cmp     rax, 0
    je      io_fail
check_digit_first:
    cmp     al, '0'
    jl      bad_first
    cmp     al, '9'
    jg      bad_first
    jmp     convert_digit
loop_read_digit:
    call    getchar
    cmp     rax, 0
    je      io_fail
    cmp     al, '0'
    jl      maybe_eol
    cmp     al, '9'
    jg      maybe_eol
convert_digit:
    imul    r12, 10
    sub     rax, '0'
    add     r12, rax
    jmp     loop_read_digit
maybe_eol:
    cmp     al,10
    jne     io_fail
    jmp     finish
finish:
    mov     rax, r12
    cmp     r13, 0
    je      store_result
    neg     rax
store_result:
    pop     r13
    pop     r12
    mov     rsp, rbp
    pop     rbp
    ret

; ======== builtin: putchar ========
global putchar
putchar:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 8
    mov     byte [rsp], dil
    mov     rax, 1
    mov     rdi, 1
    mov     rsi, rsp
    mov     rdx, 1
    syscall
    add     rsp, 8
    pop     rbp
    ret

; ======== builtin: putint ========
global putint
putint:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    xor     r13, r13
    mov     r13, rdi
    xor     r12, r12
    cmp     r13, 0
    jne     check_sign
    mov     rdi, '0'
    call    putchar
    jmp     done
check_sign:
    jge     convert_digits
    mov     rdi, '-'
    call    putchar
    neg     r13
    mov     rax, r13
convert_digits:
    mov     rbx, 10
conv_loop:
    xor     rdx, rdx
    idiv    rbx
    push    rdx
    inc     r12
    cmp     rax, 0
    jne     conv_loop
print_loop:
    pop     rax
    add     al, '0'
    mov     rdi, rax
    call    putchar
    dec     r12
    jnz     print_loop
done:
    pop     r13
    pop     r12
    pop     rbx
    mov     rsp, rbp
    pop     rbp
    ret

section .bss
section .text

global _start
_start:
    call main
    mov rdi, rax
    mov rax, 60
    syscall

; ----- function main -----
main:
    push rbp
    mov rbp, rsp
    mov rax, 0
    jmp ret_main

ret_main:
    mov rsp, rbp
    pop rbp
    ret
