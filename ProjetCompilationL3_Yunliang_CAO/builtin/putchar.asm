; ======== builtin: putchar ========
global putchar
putchar:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 8               ; Réserver espace temporaire
    mov     byte [rsp], dil      ; Placer le caractère dans buffer
    mov     rax, 1               ; syscall: write
    mov     rdi, 1               ; stdout
    mov     rsi, rsp             ; buffer
    mov     rdx, 1               ; taille = 1 octet
    syscall
    add     rsp, 8
    pop     rbp
    ret
