.macro ENTRY name
    .type \name, #function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
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
    ldr pc, [pc, #0]
END hook_trampoline

/*
    mov r1, r0
    mov r2, sp
    bx  lr
    */