; ======== builtin: getchar ========
global getchar
getchar:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 1               ; Réserver 1 octet sur la pile
    mov     rax, 0               ; syscall: read
    mov     rdi, 0               ; stdin
    mov     rsi, rsp             ; buffer cible
    mov     rdx, 1               ; lire 1 octet
    syscall
    cmp     rax, 1
    jne     .fail_getchar       ; Si lecture échoue
    xor     eax, eax
    mov     al, [rsp]           ; Charger l'octet lu dans AL
    add     rsp, 1
    mov     rsp, rbp
    pop     rbp
    ret
.fail_getchar:
    add     rsp, 1
    mov     rsp, rbp
    pop     rbp
    mov     rax, 0              ; Valeur par défaut en cas d'erreur
    ret
