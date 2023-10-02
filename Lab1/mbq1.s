	.file	1 "mbq1.c"

 # GNU C 2.7.2.3 [AL 1.1, MM 40, tma 0.1] SimpleScalar running sstrix compiled by GNU C

 # Cc1 defaults:
 # -mgas -mgpOPT

 # Cc1 arguments (-G value = 8, Cpu = default, ISA = 1):
 # -quiet -dumpbase -O2 -o

gcc2_compiled.:
__gnu_compiled_c:
	.text
	.align	2
	.globl	main

	.extern	stdin, 4
	.extern	stdout, 4

	.text

	.loc	1 4
	.ent	main
main:
	.frame	$sp,40,$31		# vars= 16, regs= 1/0, args= 16, extra= 0
	.mask	0x80000000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,40
	sw	$31,32($sp)
	jal	__main
	move	$3,$0
	li	$5,0x00000001		# 1
	li	$4,0x00010000		# 65536
	ori	$4,$4,0x869f
	#.set	volatile
	sw	$0,16($sp)
	#.set	novolatile
	#.set	volatile
	sw	$0,20($sp)
	#.set	novolatile
	#.set	volatile
	sw	$0,24($sp)
	#.set	novolatile
$L17:
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
	#nop
	.set	reorder
	addu	$2,$2,2
	#.set	volatile
	sw	$2,24($sp)
	#.set	novolatile
	.set	noreorder
	#.set	volatile
	lw	$2,16($sp)
	#.set	novolatile
	.set	reorder
	addu	$3,$3,1
	slt	$2,$4,$3
	beq	$2,$0,$L17
	move	$2,$0
	lw	$31,32($sp)
	addu	$sp,$sp,40
	j	$31
	.end	main
