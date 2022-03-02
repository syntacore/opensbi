/// Syntacore SCR* infra
///
/// @copyright (C) Syntacore 2015-2019. All rights reserved.
/// @author mn-sc
///
/// @brief special assembler macros

#ifndef _SCR_ASM_MAGIC_H
#define _SCR_ASM_MAGIC_H

#ifndef __ASSEMBLER__

asm (".altmacro;"
// GAS macro: convert symbolic register names to numbers
".macro __scr_reg2num reg_name;"
".ifc \\reg_name, x0;.set __scr_macro_regn,0;.endif;"
".ifc \\reg_name, x1;.set __scr_macro_regn,1;.endif;"
".ifc \\reg_name, x2;.set __scr_macro_regn,2;.endif;"
".ifc \\reg_name, x3;.set __scr_macro_regn,3;.endif;"
".ifc \\reg_name, x4;.set __scr_macro_regn,4;.endif;"
".ifc \\reg_name, x5;.set __scr_macro_regn,5;.endif;"
".ifc \\reg_name, x6;.set __scr_macro_regn,6;.endif;"
".ifc \\reg_name, x7;.set __scr_macro_regn,7;.endif;"
".ifc \\reg_name, x8;.set __scr_macro_regn,8;.endif;"
".ifc \\reg_name, x9;.set __scr_macro_regn,9;.endif;"
".ifc \\reg_name, x10;.set __scr_macro_regn,10;.endif;"
".ifc \\reg_name, x11;.set __scr_macro_regn,11;.endif;"
".ifc \\reg_name, x12;.set __scr_macro_regn,12;.endif;"
".ifc \\reg_name, x13;.set __scr_macro_regn,13;.endif;"
".ifc \\reg_name, x14;.set __scr_macro_regn,14;.endif;"
".ifc \\reg_name, x15;.set __scr_macro_regn,15;.endif;"
".ifc \\reg_name, x16;.set __scr_macro_regn,16;.endif;"
".ifc \\reg_name, x17;.set __scr_macro_regn,17;.endif;"
".ifc \\reg_name, x18;.set __scr_macro_regn,18;.endif;"
".ifc \\reg_name, x19;.set __scr_macro_regn,19;.endif;"
".ifc \\reg_name, x20;.set __scr_macro_regn,20;.endif;"
".ifc \\reg_name, x21;.set __scr_macro_regn,21;.endif;"
".ifc \\reg_name, x22;.set __scr_macro_regn,22;.endif;"
".ifc \\reg_name, x23;.set __scr_macro_regn,23;.endif;"
".ifc \\reg_name, x24;.set __scr_macro_regn,24;.endif;"
".ifc \\reg_name, x25;.set __scr_macro_regn,25;.endif;"
".ifc \\reg_name, x26;.set __scr_macro_regn,26;.endif;"
".ifc \\reg_name, x27;.set __scr_macro_regn,27;.endif;"
".ifc \\reg_name, x28;.set __scr_macro_regn,28;.endif;"
".ifc \\reg_name, x29;.set __scr_macro_regn,29;.endif;"
".ifc \\reg_name, x30;.set __scr_macro_regn,30;.endif;"
".ifc \\reg_name, x31;.set __scr_macro_regn,31;.endif;"

".ifc \\reg_name, zero;.set __scr_macro_regn,0;.endif;"
".ifc \\reg_name, ra;.set __scr_macro_regn,1;.endif;"
".ifc \\reg_name, sp;.set __scr_macro_regn,2;.endif;"
".ifc \\reg_name, gp;.set __scr_macro_regn,3;.endif;"
".ifc \\reg_name, tp;.set __scr_macro_regn,4;.endif;"
".ifc \\reg_name, t0;.set __scr_macro_regn,5;.endif;"
".ifc \\reg_name, t1;.set __scr_macro_regn,6;.endif;"
".ifc \\reg_name, t2;.set __scr_macro_regn,7;.endif;"
".ifc \\reg_name, s0;.set __scr_macro_regn,8;.endif;"
".ifc \\reg_name, fp;.set __scr_macro_regn,8;.endif;"
".ifc \\reg_name, s1;.set __scr_macro_regn,9;.endif;"
".ifc \\reg_name, a0;.set __scr_macro_regn,10;.endif;"
".ifc \\reg_name, a1;.set __scr_macro_regn,11;.endif;"
".ifc \\reg_name, a2;.set __scr_macro_regn,12;.endif;"
".ifc \\reg_name, a3;.set __scr_macro_regn,13;.endif;"
".ifc \\reg_name, a4;.set __scr_macro_regn,14;.endif;"
".ifc \\reg_name, a5;.set __scr_macro_regn,15;.endif;"
".ifc \\reg_name, a6;.set __scr_macro_regn,16;.endif;"
".ifc \\reg_name, a7;.set __scr_macro_regn,17;.endif;"
".ifc \\reg_name, s2;.set __scr_macro_regn,18;.endif;"
".ifc \\reg_name, s3;.set __scr_macro_regn,19;.endif;"
".ifc \\reg_name, s4;.set __scr_macro_regn,20;.endif;"
".ifc \\reg_name, s5;.set __scr_macro_regn,21;.endif;"
".ifc \\reg_name, s6;.set __scr_macro_regn,22;.endif;"
".ifc \\reg_name, s7;.set __scr_macro_regn,23;.endif;"
".ifc \\reg_name, s8;.set __scr_macro_regn,24;.endif;"
".ifc \\reg_name, s9;.set __scr_macro_regn,25;.endif;"
".ifc \\reg_name, s10;.set __scr_macro_regn,26;.endif;"
".ifc \\reg_name, s11;.set __scr_macro_regn,27;.endif;"
".ifc \\reg_name, t3;.set __scr_macro_regn,28;.endif;"
".ifc \\reg_name, t4;.set __scr_macro_regn,29;.endif;"
".ifc \\reg_name, t5;.set __scr_macro_regn,30;.endif;"
".ifc \\reg_name, t6;.set __scr_macro_regn,31;.endif;"
".endm;"
// GAS macro: make ops with 2 args: dst_reg, src_reg
".macro mk2ds op, dst_reg, src_reg;"
"\\op x\\dst_reg, x\\src_reg;"
".endm;"
// GAS macro: make ops with 3 args: dst_reg, src_reg, imm
".macro mk3dsi op, arg1, arg2, arg3;"
"\\op x\\arg1, x\\arg2, \\arg3;"
".endm;"
// GAS macro: make ops with 3 args: dst_reg, src_reg1, src_reg2
".macro mk3dss op, dst_reg, src_reg1, src_reg2;"
"\\op x\\dst_reg, x\\src_reg1, x\\src_reg2;"
".endm;"
// GAS macro: convert ops with 3 args: dst_reg, src_reg1, src_reg2
".macro op4dsi op, rd_name, rs_name, imm;"
"__scr_reg2num \\rd_name;.set __rdn,__scr_macro_regn;"
"__scr_reg2num \\rs_name;.set __rsn,__scr_macro_regn;"
"mk3dsi \\op, %__rdn, %__rsn, \\imm;"
".endm;"
// GAS macro: custom op: clflush reg
".macro clflush reg;"
"__scr_reg2num \\reg;"
".word 0x10900073 + (__scr_macro_regn << 15);"
".endm;"
// GAS macro: custom op: cvlinvd reg
".macro clinvd reg;"
"__scr_reg2num \\reg;"
".word 0x10800073 + (__scr_macro_regn << 15);"
".endm;"
);

#else // !__ASSEMBLER__

.altmacro;
// GAS macro: convert symbolic register names to numbers
.macro __scr_reg2num reg_name
	.ifc \reg_name, x0; .set __scr_macro_regn, 0; .endif
	.ifc \reg_name, x1; .set __scr_macro_regn, 1; .endif
	.ifc \reg_name, x2; .set __scr_macro_regn, 2; .endif
	.ifc \reg_name, x3; .set __scr_macro_regn, 3; .endif
	.ifc \reg_name, x4; .set __scr_macro_regn, 4; .endif
	.ifc \reg_name, x5; .set __scr_macro_regn, 5; .endif
	.ifc \reg_name, x6; .set __scr_macro_regn, 6; .endif
	.ifc \reg_name, x7; .set __scr_macro_regn, 7; .endif
	.ifc \reg_name, x8; .set __scr_macro_regn, 8; .endif
	.ifc \reg_name, x9; .set __scr_macro_regn, 9; .endif
	.ifc \reg_name, x10; .set __scr_macro_regn, 10; .endif
	.ifc \reg_name, x11; .set __scr_macro_regn, 11; .endif
	.ifc \reg_name, x12; .set __scr_macro_regn, 12; .endif
	.ifc \reg_name, x13; .set __scr_macro_regn, 13; .endif
	.ifc \reg_name, x14; .set __scr_macro_regn, 14; .endif
	.ifc \reg_name, x15; .set __scr_macro_regn, 15; .endif
	.ifc \reg_name, x16; .set __scr_macro_regn, 16; .endif
	.ifc \reg_name, x17; .set __scr_macro_regn, 17; .endif
	.ifc \reg_name, x18; .set __scr_macro_regn, 18; .endif
	.ifc \reg_name, x19; .set __scr_macro_regn, 19; .endif
	.ifc \reg_name, x20; .set __scr_macro_regn, 20; .endif
	.ifc \reg_name, x21; .set __scr_macro_regn, 21; .endif
	.ifc \reg_name, x22; .set __scr_macro_regn, 22; .endif
	.ifc \reg_name, x23; .set __scr_macro_regn, 23; .endif
	.ifc \reg_name, x24; .set __scr_macro_regn, 24; .endif
	.ifc \reg_name, x25; .set __scr_macro_regn, 25; .endif
	.ifc \reg_name, x26; .set __scr_macro_regn, 26; .endif
	.ifc \reg_name, x27; .set __scr_macro_regn, 27; .endif
	.ifc \reg_name, x28; .set __scr_macro_regn, 28; .endif
	.ifc \reg_name, x29; .set __scr_macro_regn, 29; .endif
	.ifc \reg_name, x30; .set __scr_macro_regn, 30; .endif
	.ifc \reg_name, x31; .set __scr_macro_regn, 31; .endif

	.ifc \reg_name, zero; .set __scr_macro_regn, 0; .endif
	.ifc \reg_name, ra; .set __scr_macro_regn, 1; .endif
	.ifc \reg_name, sp; .set __scr_macro_regn, 2; .endif
	.ifc \reg_name, gp; .set __scr_macro_regn, 3; .endif
	.ifc \reg_name, tp; .set __scr_macro_regn, 4; .endif
	.ifc \reg_name, t0; .set __scr_macro_regn, 5; .endif
	.ifc \reg_name, t1; .set __scr_macro_regn, 6; .endif
	.ifc \reg_name, t2; .set __scr_macro_regn, 7; .endif
	.ifc \reg_name, s0; .set __scr_macro_regn, 8; .endif
	.ifc \reg_name, fp; .set __scr_macro_regn, 8; .endif
	.ifc \reg_name, s1; .set __scr_macro_regn, 9; .endif
	.ifc \reg_name, a0; .set __scr_macro_regn, 10; .endif
	.ifc \reg_name, a1; .set __scr_macro_regn, 11; .endif
	.ifc \reg_name, a2; .set __scr_macro_regn, 12; .endif
	.ifc \reg_name, a3; .set __scr_macro_regn, 13; .endif
	.ifc \reg_name, a4; .set __scr_macro_regn, 14; .endif
	.ifc \reg_name, a5; .set __scr_macro_regn, 15; .endif
	.ifc \reg_name, a6; .set __scr_macro_regn, 16; .endif
	.ifc \reg_name, a7; .set __scr_macro_regn, 17; .endif
	.ifc \reg_name, s2; .set __scr_macro_regn, 18; .endif
	.ifc \reg_name, s3; .set __scr_macro_regn, 19; .endif
	.ifc \reg_name, s4; .set __scr_macro_regn, 20; .endif
	.ifc \reg_name, s5; .set __scr_macro_regn, 21; .endif
	.ifc \reg_name, s6; .set __scr_macro_regn, 22; .endif
	.ifc \reg_name, s7; .set __scr_macro_regn, 23; .endif
	.ifc \reg_name, s8; .set __scr_macro_regn, 24; .endif
	.ifc \reg_name, s9; .set __scr_macro_regn, 25; .endif
	.ifc \reg_name, s10; .set __scr_macro_regn, 26; .endif
	.ifc \reg_name, s11; .set __scr_macro_regn, 27; .endif
	.ifc \reg_name, t3; .set __scr_macro_regn, 28; .endif
	.ifc \reg_name, t4; .set __scr_macro_regn, 29; .endif
	.ifc \reg_name, t5; .set __scr_macro_regn, 30; .endif
	.ifc \reg_name, t6; .set __scr_macro_regn, 31; .endif
.endm
// GAS macro: make ops with 2 args: dst_reg, src_reg
.macro mk2ds op, dst_reg, src_reg
	\op x\dst_reg, x\src_reg
.endm
// GAS macro: make ops with 3 args: dst_reg, src_reg, imm
.macro mk3dsi op, arg1, arg2, arg3
	\op x\arg1, x\arg2, \arg3
.endm
// GAS macro: make ops with 3 args: dst_reg, src_reg1, src_reg2
.macro mk3dss op, dst_reg, src_reg1, src_reg2
	\op x\dst_reg, x\src_reg1, x\src_reg2
.endm
// GAS macro: convert ops with 3 args: dst_reg, src_reg1, src_reg2
.macro op4dsi op, rd_name, rs_name, imm
	__scr_reg2num \rd_name; .set __rdn, __scr_macro_regn
	__scr_reg2num \rs_name; .set __rsn, __scr_macro_regn
	mk3dsi \op, % __rdn, % __rsn, \imm
.endm
// GAS macro: custom op: clflush reg
.macro clflush reg
	__scr_reg2num \reg
	.word 0x10900073 + (__scr_macro_regn << 15)
.endm
// GAS macro: custom op: clinvd reg
.macro clinvd reg
	__scr_reg2num \reg
	.word 0x10800073 + (__scr_macro_regn << 15)
.endm

#endif // !__ASSEMBLER__
#endif // _SCR_ASM_MAGIC_H
