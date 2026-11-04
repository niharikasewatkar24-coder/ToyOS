.section .text
.global switch_to
switch_to:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 20(%esp), %eax
    testl %eax, %eax
    jz 1f
    movl %esp, (%eax)

1:
    movl 24(%esp), %edx
    testl %edx, %edx
    jz 2f
    movl %edx, %esp

2:
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
