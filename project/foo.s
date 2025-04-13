	.text
	.file	"Main"
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI0_0:
	.quad	0x4008000000000000
.LCPI0_1:
	.quad	0x4024000000000000
.LCPI0_2:
	.quad	0x3ff0000000000000
	.text
	.globl	Main
	.p2align	4, 0x90
	.type	Main,@function
Main:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	movsd	.LCPI0_0(%rip), %xmm0
	callq	putd@PLT
	xorpd	%xmm0, %xmm0
	.p2align	4, 0x90
.LBB0_1:
	movsd	%xmm0, (%rsp)
	movsd	(%rsp), %xmm0
	callq	putd@PLT
	movsd	(%rsp), %xmm0
	movsd	.LCPI0_1(%rip), %xmm1
	ucomisd	%xmm0, %xmm1
	jbe	.LBB0_3
	addsd	.LCPI0_2(%rip), %xmm0
	jmp	.LBB0_1
.LBB0_3:
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	Main, .Lfunc_end0-Main
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
