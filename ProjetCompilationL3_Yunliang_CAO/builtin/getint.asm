; ======== builtin: getint ========
global getint
getint:
    push    rbp
    mov     rbp, rsp
    push    r12                  ; r12 = accumulateur pour le nombre
    push    r13                  ; r13 = indicateur de signe
    xor     r12, r12             ; r12 = 0
    xor     r13, r13             ; r13 = 0 (positif)

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
    mov     r13, 1               ; Négatif
    jmp     need_digit

plus_sign:
    jmp     need_digit

bad_first:
    jmp     io_fail

io_fail:
    mov     rax, 60              ; exit
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
    cmp     al, 10               ; Doit finir par '\n'
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
