; ======== builtin: putint ========
global putint
putint:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13

    xor     r13, r13
    mov     r13, rdi             ; r13 ← entier à afficher
    xor     r12, r12             ; compteur de chiffres

    cmp     r13, 0
    jne     check_sign
    mov     rdi, '0'             ; afficher '0'
    call    putchar
    jmp     done

check_sign:
    jge     convert_digits
    mov     rdi, '-'             ; si négatif, afficher '-'
    call    putchar
    neg     r13                  ; puis prendre la valeur absolue
    mov     rax, r13

convert_digits:
    mov     rbx, 10              ; base décimale

conv_loop:
    xor     rdx, rdx
    idiv    rbx                  ; rax = rax / 10, rdx = rax % 10
    push    rdx                  ; empiler le chiffre
    inc     r12                  ; incrémenter le compteur
    cmp     rax, 0
    jne     conv_loop

print_loop:
    pop     rax
    add     al, '0'              ; convertir chiffre → caractère ASCII
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
