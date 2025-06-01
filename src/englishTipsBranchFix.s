.macro CODE_BEGIN name
	.section .text.\name, "ax", %progbits
	.global \name
	.type \name, %function
	.align 2
	.cfi_startproc
\name:
.endm

.macro CODE_END
	.cfi_endproc
.endm

CODE_BEGIN englishTipsBranchFix1 
    cmp w18, #0x8d
    b.lt show1
    cmp w18, #0x91
    b.hi show1
    ldr x0, englishTipsHideBranch1
    br x0
show1:
    ldr x0, englishTipsShowBranch1
    br x0
CODE_END

CODE_BEGIN englishTipsBranchFix2 
    cmp w17, #0x8d
    b.lt show2
    cmp w17, #0x91
    b.hi show2
    ldr x0, englishTipsHideBranch2
    br x0
show2:
    ldr x0, englishTipsShowBranch2
    br x0
CODE_END