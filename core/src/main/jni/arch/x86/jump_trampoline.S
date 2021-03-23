.macro ENTRY name
    .type \name, @function
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

.macro VAR name
    .global \name
\name:
.endm

    .extern nativeHookNoBackUp
ENTRY hook_trampoline
    mov %eax, %ecx
    mov %esp, %edx
    ret
END hook_trampoline