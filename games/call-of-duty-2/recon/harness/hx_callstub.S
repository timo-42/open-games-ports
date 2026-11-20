/*
 * cdecl call trampolines for the cod2_lnxded differential harness (i386 only).
 *
 *   void hx_call_cdecl(void *fn, const uint32_t *words, int nwords, struct hx_ret *out)
 *   void hx_call_cdecl_fp(void *fn, const uint32_t *words, int nwords, struct hx_ret *out)
 *
 * Copies nwords 32-bit argument words onto a 16-byte aligned stack, calls fn and
 * stores EAX/EDX (and, for the _fp variant, ST(0) as an 80-bit value popped from the
 * x87 stack). ESP is restored from EBP, so callee-pops conventions (ret $4 for hidden
 * struct-return pointers) cannot unbalance the caller.
 * struct hx_ret layout: +0 eax, +4 edx, +8 ten-byte x87 value (16 bytes reserved).
 */
    .text
    .globl hx_call_cdecl
    .type hx_call_cdecl, @function
hx_call_cdecl:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 16(%ebp), %ecx
    leal 0(,%ecx,4), %eax
    subl %eax, %esp
    andl $-16, %esp
    movl %esp, %edi
    movl 12(%ebp), %esi
    cld
    rep movsl
    movl 8(%ebp), %eax
    call *%eax
    movl 20(%ebp), %ecx
    movl %eax, 0(%ecx)
    movl %edx, 4(%ecx)
    leal -12(%ebp), %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
    .size hx_call_cdecl, .-hx_call_cdecl

    .globl hx_call_cdecl_fp
    .type hx_call_cdecl_fp, @function
hx_call_cdecl_fp:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 16(%ebp), %ecx
    leal 0(,%ecx,4), %eax
    subl %eax, %esp
    andl $-16, %esp
    movl %esp, %edi
    movl 12(%ebp), %esi
    cld
    rep movsl
    movl 8(%ebp), %eax
    call *%eax
    movl 20(%ebp), %ecx
    movl %eax, 0(%ecx)
    movl %edx, 4(%ecx)
    fstpt 8(%ecx)
    leal -12(%ebp), %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
    .size hx_call_cdecl_fp, .-hx_call_cdecl_fp

    .section .note.GNU-stack,"",@progbits
