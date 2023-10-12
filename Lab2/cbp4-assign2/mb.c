#include <stdio.h>

int main(){
    register int i = 0;
    register int a = 0;
    register int b = 0;

    for (i = 0; i < 1000000; i++){
        if (i % 7 == 0){
            a = b + a;
        }
        

        if (i % 6 == 0){
            b = b + a;
        }

		a++;
        b++;
    }

    return 0;
}

/*
main:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	.cfi_offset 13, -24
	.cfi_offset 12, -32
	.cfi_offset 3, -40
	movl	$0, %r12d
	movl	$0, %r13d
	movl	$0, %ebx
	jmp	.L2
.L5:                          start of loop
	movl	%ebx, %eax
	andl	$7, %eax
	testl	%eax, %eax
	jne	.L3                   if i % 8 != 0, branch 
	addl	%r13d, %r12d
.L3:
	addl	$1, %r12d
	movslq	%ebx, %rax
	imulq	$715827883, %rax, %rax
	shrq	$32, %rax
	movl	%ebx, %ecx
	sarl	$31, %ecx
	movl	%eax, %edx
	subl	%ecx, %edx
	movl	%edx, %eax
	addl	%eax, %eax
	addl	%edx, %eax
	addl	%eax, %eax
	movl	%ebx, %edx
	subl	%eax, %edx
	testl	%edx, %edx
	jne	.L4                     if i % 6 != 0, branch 
	addl	%r12d, %r13d
.L4:
	addl	$1, %r13d
	addl	$1, %ebx
.L2:
	cmpl	$99999, %ebx
	jle	.L5
	movl	$0, %eax
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.ident	"GCC: (Debian 10.2.1-6) 10.2.1 20210110"
	.section	.note.GNU-stack,"",@progbits
    */