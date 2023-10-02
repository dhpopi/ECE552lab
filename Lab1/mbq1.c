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
        reg2 = reg1;
        reg3 = reg1 + 2;
        reg4 = reg1 + 1; 
    }
    return 0;
}

/*
These codes are excuted in the for loop
    $L17:
	lw	$2,16($sp)      
	sw	$2,20($sp)      load to store. stall 2 cycle for 5 stage. no stall for 6 stage .
	lw	$2,16($sp)
	addu	$2,$2,2     load to use case. stall 2 cycle for 5 stage. 2 cycle stall for 6 stage.
	sw	$2,24($sp)      read after case. stall 2 cycle for 5 stage. no stall for 6 stage
	lw	$2,16($sp)
	addu	$3,$3,1
	slt	$2,$4,$3
	beq	$2,$0,$L17
	move	$2,$0
	lw	$31,32($sp)
	addu	$sp,$sp,40
	j	$31
*/