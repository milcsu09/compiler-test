	.text
	.file	"Main"
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI0_0:
	.quad	0x3ff0000000000000
.LCPI0_1:
	.quad	0x4000000000000000
.LCPI0_2:
	.quad	0x4008000000000000
	.text
	.globl	Main
	.p2align	4, 0x90
	.type	Main,@function
Main:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	movsd	.LCPI0_0(%rip), %xmm1
	movsd	.LCPI0_1(%rip), %xmm2
	movsd	.LCPI0_2(%rip), %xmm0
	movaps	%xmm0, %xmm3
	movb	$4, %al
	callq	putds@PLT
	xorps	%xmm0, %xmm0
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	Main, .Lfunc_end0-Main
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
