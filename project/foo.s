	.text
	.file	"Main"
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI0_0:
	.quad	0x3ff0000000000000
	.text
	.globl	"||"
	.p2align	4, 0x90
	.type	"||",@function
"||":
	.cfi_startproc
	movapd	%xmm0, %xmm2
	xorpd	%xmm3, %xmm3
	ucomisd	%xmm3, %xmm1
	movsd	.LCPI0_0(%rip), %xmm1
	xorpd	%xmm0, %xmm0
	jne	.LBB0_1
	ucomisd	%xmm3, %xmm2
	jne	.LBB0_3
.LBB0_4:
	retq
.LBB0_1:
	movapd	%xmm1, %xmm0
	ucomisd	%xmm3, %xmm2
	je	.LBB0_4
.LBB0_3:
	movapd	%xmm1, %xmm0
	retq
.Lfunc_end0:
	.size	"||", .Lfunc_end0-"||"
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI1_0:
	.quad	0x3ff0000000000000
	.text
	.globl	"&&"
	.p2align	4, 0x90
	.type	"&&",@function
"&&":
	.cfi_startproc
	movapd	%xmm0, %xmm2
	xorpd	%xmm0, %xmm0
	ucomisd	%xmm0, %xmm1
	xorpd	%xmm1, %xmm1
	jne	.LBB1_1
	ucomisd	%xmm0, %xmm2
	jne	.LBB1_3
.LBB1_4:
	retq
.LBB1_1:
	movsd	.LCPI1_0(%rip), %xmm1
	ucomisd	%xmm0, %xmm2
	je	.LBB1_4
.LBB1_3:
	movapd	%xmm1, %xmm0
	retq
.Lfunc_end1:
	.size	"&&", .Lfunc_end1-"&&"
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI2_0:
	.quad	0x3ff0000000000000
	.text
	.globl	"=="
	.p2align	4, 0x90
	.type	"==",@function
"==":
	.cfi_startproc
	movapd	%xmm0, %xmm2
	subsd	%xmm1, %xmm2
	xorpd	%xmm0, %xmm0
	ucomisd	%xmm0, %xmm2
	jne	.LBB2_2
	movsd	.LCPI2_0(%rip), %xmm0
.LBB2_2:
	retq
.Lfunc_end2:
	.size	"==", .Lfunc_end2-"=="
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI3_0:
	.quad	0x3ff0000000000000
	.text
	.globl	"!="
	.p2align	4, 0x90
	.type	"!=",@function
"!=":
	.cfi_startproc
	movapd	%xmm0, %xmm2
	subsd	%xmm1, %xmm2
	xorpd	%xmm0, %xmm0
	ucomisd	%xmm0, %xmm2
	je	.LBB3_2
	movsd	.LCPI3_0(%rip), %xmm0
.LBB3_2:
	retq
.Lfunc_end3:
	.size	"!=", .Lfunc_end3-"!="
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI4_0:
	.quad	0x3ff0000000000000
	.text
	.globl	"!"
	.p2align	4, 0x90
	.type	"!",@function
"!":
	.cfi_startproc
	movapd	%xmm0, %xmm1
	xorpd	%xmm0, %xmm0
	ucomisd	%xmm0, %xmm1
	jne	.LBB4_2
	movsd	.LCPI4_0(%rip), %xmm0
.LBB4_2:
	retq
.Lfunc_end4:
	.size	"!", .Lfunc_end4-"!"
	.cfi_endproc

	.globl	"-"
	.p2align	4, 0x90
	.type	"-",@function
"-":
	.cfi_startproc
	xorpd	%xmm1, %xmm1
	subsd	%xmm0, %xmm1
	movapd	%xmm1, %xmm0
	retq
.Lfunc_end5:
	.size	"-", .Lfunc_end5-"-"
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI6_0:
	.quad	0x40c3940000000000
.LCPI6_1:
	.quad	0x3ff0000000000000
.LCPI6_2:
	.quad	0x4010000000000000
	.text
	.globl	mandelbrot_converge_help
	.p2align	4, 0x90
	.type	mandelbrot_converge_help,@function
mandelbrot_converge_help:
	.cfi_startproc
	subq	$56, %rsp
	.cfi_def_cfa_offset 64
	movsd	%xmm4, 48(%rsp)
	movsd	%xmm3, 40(%rsp)
	movapd	%xmm0, %xmm3
	movsd	.LCPI6_0(%rip), %xmm0
	movsd	%xmm2, (%rsp)
	cmpltsd	%xmm2, %xmm0
	movsd	.LCPI6_1(%rip), %xmm2
	andpd	%xmm2, %xmm0
	movsd	%xmm3, 32(%rsp)
	mulsd	%xmm3, %xmm3
	movsd	%xmm1, 24(%rsp)
	mulsd	%xmm1, %xmm1
	movsd	%xmm3, 16(%rsp)
	movsd	%xmm1, 8(%rsp)
	addsd	%xmm1, %xmm3
	movsd	.LCPI6_2(%rip), %xmm1
	cmpltsd	%xmm3, %xmm1
	andpd	%xmm2, %xmm1
	callq	"||"@PLT
	xorpd	%xmm1, %xmm1
	ucomisd	%xmm1, %xmm0
	je	.LBB6_2
	movsd	(%rsp), %xmm0
	addq	$56, %rsp
	.cfi_def_cfa_offset 8
	retq
.LBB6_2:
	.cfi_def_cfa_offset 64
	movsd	16(%rsp), %xmm0
	subsd	8(%rsp), %xmm0
	movsd	40(%rsp), %xmm3
	addsd	%xmm3, %xmm0
	movsd	32(%rsp), %xmm1
	addsd	%xmm1, %xmm1
	mulsd	24(%rsp), %xmm1
	movsd	48(%rsp), %xmm4
	addsd	%xmm4, %xmm1
	movsd	(%rsp), %xmm2
	addsd	.LCPI6_1(%rip), %xmm2
	callq	mandelbrot_converge_help@PLT
	addq	$56, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end6:
	.size	mandelbrot_converge_help, .Lfunc_end6-mandelbrot_converge_help
	.cfi_endproc

	.globl	mandelbrot_converge
	.p2align	4, 0x90
	.type	mandelbrot_converge,@function
mandelbrot_converge:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorps	%xmm2, %xmm2
	movaps	%xmm0, %xmm3
	movaps	%xmm1, %xmm4
	callq	mandelbrot_converge_help@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end7:
	.size	mandelbrot_converge, .Lfunc_end7-mandelbrot_converge
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3
.LCPI8_0:
	.quad	0x4006000000000000
.LCPI8_1:
	.quad	0x3ff8000000000000
.LCPI8_2:
	.quad	0x3ff4000000000000
.LCPI8_3:
	.quad	0x3fa3333333333333
.LCPI8_4:
	.quad	0x3fa999999999999a
	.text
	.globl	Main
	.p2align	4, 0x90
	.type	Main,@function
Main:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	movsd	.LCPI8_0(%rip), %xmm0
	callq	"-"@PLT
	movsd	%xmm0, (%rsp)
	movsd	.LCPI8_1(%rip), %xmm0
	callq	"-"@PLT
	movaps	%xmm0, %xmm3
	movsd	.LCPI8_2(%rip), %xmm1
	movsd	.LCPI8_3(%rip), %xmm2
	movsd	.LCPI8_4(%rip), %xmm5
	movsd	(%rsp), %xmm0
	movsd	.LCPI8_1(%rip), %xmm4
	callq	mandelbrot_plot@PLT
	xorps	%xmm0, %xmm0
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end8:
	.size	Main, .Lfunc_end8-Main
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
