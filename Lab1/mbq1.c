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
		reg1 = reg2 + 1;
    }
    return 0;
}
// For 5 stage without forwarding
/*
$L17:
	sw	$5,16($sp)
	lw	$2,16($sp)
	lw	$2,16($sp)
	sw	$2,20($sp)		For Q1) 2 stall, This test if the code captures RAW due to data flow from M stage to M stage. 
						For Q2) 0 stall, The W/M forwarding handles it
	lw	$2,16($sp)
	addu	$2,$2,2		For Q1) 2 stall, This test if the code captures RAW due to data flow from M stage to X stage. 
						For Q2) 2 stall, This is the load to use hazard that cannot be handled by forwarding
	sw	$2,24($sp)		For Q1) 2 stall, This test if the code captures RAW due to data flow from X stage to M stage. 
						For Q2) 0 stall, The M/X forwarding handles it
	lw	$2,20($sp)
	addu	$3,$3,1
	addu	$2,$2,1		For Q1) 1 stall, This test if the code captures the 1 cycle stall correctly base on the distance between data dependent instructions. 
						For Q2) 1 stall, This is the load to use hazard that cannot be handled by forwarding
	sw	$2,16($sp)		For Q1) 2 stall, This test if the code captures RAW due to data flow from X stage to M stage. 
						For Q2) 0 stall, The M/X forwarding handles it
	slt	$2,$4,$3		
	beq	$2,$0,$L17		For Q1) 2 stall, This test if the code captures RAW due to data flow from X stage to X stage. 
						For Q2) 1 stall, due to the added x stage
	move	$2,$0
	lw	$31,32($sp)
	addu	$sp,$sp,40
	j	$31
	.end	main
*/

// For Q1) the ratio of 1 stall to 2 stall is 1:5, so the number count should be roughtly the same ratio.
// For Q2) the ratio of 1 stall to 2 stall is 2:1, so the number count should be roughtly the same ratio.