#include <stdio.h>

int main(int argc, char *argv[])
{
    volatile reg1 = 0;
    volatile reg2 = 0;
    volatile reg3 = 0;
    int i;
    for(i = 0; i < 100000; i++){
        reg1 = 1;
        reg2 = reg1;
        reg3 = reg1 + 2;
    }
    return 0;
}

/*
                    #.set	volatile
	sw	$5,16($sp)
	                #.set	novolatile
	                .set	noreorder
	                #.set	volatile
	lw	$2,16($sp)  
	                #.set	novolatile
                    #nop
                    .set	reorder
                    #.set	volatile
	sw	$2,20($sp)
                    #.set	novolatile
                    .set	noreorder
                    #.set	volatile
	lw	$2,16($sp)
                    #.set	novolatile
                    .set	reorder
	addu	$3,$3,1
	addu	$2,$2,2
	                #.set	volatile
	sw	$2,24($sp)
	                #.set	novolatile
	slt	$2,$4,$3
	beq	$2,$0,$L17
*/