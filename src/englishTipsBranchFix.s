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

CODE_BEGIN englishTipsBranchFix
    ldr w18, [x12]
    cmp w18, #0x8d
    b.lt show
    cmp w18, #0x91
    b.hi show
    ccmn wzr, wzr, 0b0010, mi   // Trigger the b.hi to hide the TIPs
    ret                         // (Z == 0) && (C == 1)
show:
    ccmn wzr, wzr, 0b0000, mi   // Don't trigger and show TIPs
    ret
CODE_END