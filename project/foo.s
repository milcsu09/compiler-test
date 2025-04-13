	.text
	.file	"Main"
	.globl	"!"
	.p2align	4, 0x90
	.type	"!",@function
"!":
	.cfi_startproc
	andb	$1, %dil
	movb	%dil, -1(%rsp)
	je	.LBB0_2
	xorl	%eax, %eax
	retq
.LBB0_2:
	movb	$1, %al
	retq
.Lfunc_end0:
	.size	"!", .Lfunc_end0-"!"
	.cfi_endproc

	.globl	"=="
	.p2align	4, 0x90
	.type	"==",@function
"==":
	.cfi_startproc
	subq	$24, %rsp
	.cfi_def_cfa_offset 32
	movsd	%xmm0, 16(%rsp)
	movsd	%xmm1, 8(%rsp)
	subsd	%xmm1, %xmm0
	xorpd	%xmm1, %xmm1
	xorl	%edi, %edi
	ucomisd	%xmm1, %xmm0
	setne	%dil
	callq	"!"@PLT
	addq	$24, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	"==", .Lfunc_end1-"=="
	.cfi_endproc

	.globl	or
	.p2align	4, 0x90
	.type	or,@function
or:
	.cfi_startproc
	andb	$1, %dil
	movb	%dil, -1(%rsp)
	andl	$1, %esi
	movb	%sil, -2(%rsp)
	testb	%dil, %dil
	je	.LBB2_2
	movb	-1(%rsp), %al
	retq
.LBB2_2:
	movb	-2(%rsp), %al
	retq
.Lfunc_end2:
	.size	or, .Lfunc_end2-or
	.cfi_endproc

	.globl	and
	.p2align	4, 0x90
	.type	and,@function
and:
	.cfi_startproc
	andb	$1, %dil
	movb	%dil, -1(%rsp)
	andl	$1, %esi
	movb	%sil, -2(%rsp)
	testb	%dil, %dil
	je	.LBB3_2
	movb	-2(%rsp), %al
	retq
.LBB3_2:
	movb	-1(%rsp), %al
	retq
.Lfunc_end3:
	.size	and, .Lfunc_end3-and
	.cfi_endproc

	.globl	putb
	.p2align	4, 0x90
	.type	putb,@function
putb:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	andl	$1, %edi
	movb	%dil, 7(%rsp)
	cvtsi2sd	%edi, %xmm0
	callq	putd@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end4:
	.size	putb, .Lfunc_end4-putb
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI5_0:
	.quad	0x4000000000000000
.LCPI5_1:
	.quad	0x4008000000000000
	.text
	.globl	Main
	.p2align	4, 0x90
	.type	Main,@function
Main:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	or@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	movl	$1, %edi
	xorl	%esi, %esi
	callq	or@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	xorl	%edi, %edi
	movl	$1, %esi
	callq	or@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	movl	$1, %edi
	movl	$1, %esi
	callq	or@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	callq	separator@PLT
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	and@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	movl	$1, %edi
	xorl	%esi, %esi
	callq	and@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	xorl	%edi, %edi
	movl	$1, %esi
	callq	and@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	movl	$1, %edi
	movl	$1, %esi
	callq	and@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	callq	separator@PLT
	movb	$1, 7(%rsp)
	movsd	.LCPI5_0(%rip), %xmm0
	movsd	.LCPI5_1(%rip), %xmm1
	callq	"=="@PLT
	andb	$1, %al
	movb	%al, 6(%rsp)
	movb	$1, 7(%rsp)
	movl	$1, %edi
	callq	putb@PLT
	movzbl	6(%rsp), %edi
	callq	putb@PLT
	movzbl	6(%rsp), %esi
	movzbl	7(%rsp), %edi
	callq	or@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	movzbl	6(%rsp), %esi
	movzbl	7(%rsp), %edi
	callq	and@PLT
	movzbl	%al, %edi
	callq	putb@PLT
	xorps	%xmm0, %xmm0
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end5:
	.size	Main, .Lfunc_end5-Main
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
