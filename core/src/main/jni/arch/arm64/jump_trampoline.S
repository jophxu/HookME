.macro ENTRY name
    .type \name, #function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    /* Cache alignment for function entry */
    .balign 16
\name:
    .cfi_startproc
.endm

.macro END name
    .cfi_endproc
    .size \name, .-\name
.endm

    .extern hook_bridge_method
ENTRY hook_trampoline
    ldr x16, #8
    br x16
/*
    mov     x1, x0
    mov     x2, sp
    fmov    d0, x0
    ret
    */
END hook_trampoline
