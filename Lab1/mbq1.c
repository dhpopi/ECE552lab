#include <stdio.h>

int main(int argc, char *argv[])
{
    volatile reg1 = 0;
    volatile reg2 = 0;
    volatile reg3 = 0;
    register reg4 = 0;
    int i;
    for(i = 0; i < 100000; i++){
        reg1 = 1;
        reg4 = reg1 + 1;
        reg2 = reg1;
        reg3 = reg1 + 2; 
    }
    return 0;
}

/*
$L17:
	sw	$5,16($sp)
	lw	$2,16($sp)
	lw	$2,16($sp)
	sw	$2,20($sp)  2 stall
	lw	$2,16($sp)
	addu	$3,$3,1
	addu	$2,$2,2 1 stall
	sw	$2,24($sp)  2 stall 
	slt	$2,$4,$3
	beq	$2,$0,$L17 2 stall
	move	$2,$0
	lw	$31,32($sp)
	addu	$sp,$sp,40
	j	$31
	.end	main
    */