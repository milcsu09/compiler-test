	.text
	.file	"Main"
	.globl	or
	.p2align	4, 0x90
	.type	or,@function
or:
	.cfi_startproc
	movl	%edi, %eax
	testb	$1, %al
	jne	.LBB0_2
	movl	%esi, %eax
.LBB0_2:
	retq
.Lfunc_end0:
	.size	or, .Lfunc_end0-or
	.cfi_endproc

	.globl	and
	.p2align	4, 0x90
	.type	and,@function
and:
	.cfi_startproc
	movl	%edi, %eax
	testb	$1, %al
	je	.LBB1_2
	movl	%esi, %eax
.LBB1_2:
	retq
.Lfunc_end1:
	.size	and, .Lfunc_end1-and
	.cfi_endproc

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
	movzbl	%al, %eax
	andl	$1, %eax
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	movl	$1, %edi
	xorl	%esi, %esi
	callq	or@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	xorl	%edi, %edi
	movl	$1, %esi
	callq	or@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	movl	$1, %edi
	movl	$1, %esi
	callq	or@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	callq	separator@PLT
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	and@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	movl	$1, %edi
	xorl	%esi, %esi
	callq	and@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	xorl	%edi, %edi
	movl	$1, %esi
	callq	and@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	movl	$1, %edi
	movl	$1, %esi
	callq	and@PLT
	movzbl	%al, %eax
	andl	$1, %eax
	xorps	%xmm0, %xmm0
	cvtsi2sd	%eax, %xmm0
	callq	putd@PLT
	xorpd	%xmm0, %xmm0
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end2:
	.size	Main, .Lfunc_end2-Main
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
