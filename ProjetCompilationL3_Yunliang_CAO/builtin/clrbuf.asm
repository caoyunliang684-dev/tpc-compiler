; ======== builtin: clrbuf ========
global clrbuf
clrbuf:
    push    rbp
    mov     rbp, rsp
.read_more:
    call    getchar              ; Lire un caractère depuis l'entrée
    test    rax, rax             ; Si EOF (rax == 0) → sortir
    je      .done_clrbuf
    cmp     al, 10               ; '\n' ?
    je      .done_clrbuf
    cmp     al, 13               ; '\r' ?
    je      .done_clrbuf
    jmp     .read_more           ; Continuer à lire
.done_clrbuf:
    mov     rsp, rbp
    pop     rbp
    ret
