// SPDX-License-Identifier: GPL-3.0-or-later
// Recorded from the original inputs with pinned D3DX8, before migration.
static const char RiverWaterReferenceSource[] =
    "ps.1.1\n"
    " \t\t\ttex t0 \n"
    "\t\t\ttex t1\t\n"
    "\t\t\ttex t2\t\n"
    "\t\t\ttex t3\n"
    "\t\t\tmul r0.rgb, v0, t0 ; blend vertex color into t0. \n"
    "\t\t\tmov r0.a, t0 ; keep vertex alpha from fading the base water. \n"
    "\t\t\tmul r1, t1, t2 ; mul\n"
    "\t\t\tadd r1.rgb, r1, t3\n"
    "\t\t\tmul r1.rgb, r1, v0.a\n"
    "\t\t\t+mul r0.a, r0, t3\n"
    "\t\t\tadd r0.rgb, r0, r1\n"
    "";
static const unsigned long RiverWaterReferenceCode[] = {
    0xffff0101ul, 0x0009fffeul, 0x58443344ul, 0x68532038ul, 0x72656461ul, 0x73734120ul,
    0x6c626d65ul, 0x56207265ul, 0x69737265ul, 0x30206e6ful, 0x0031392eul, 0x00000042ul,
    0xb00f0000ul, 0x00000042ul, 0xb00f0001ul, 0x00000042ul, 0xb00f0002ul, 0x00000042ul,
    0xb00f0003ul, 0x00000005ul, 0x80070000ul, 0x90e40000ul, 0xb0e40000ul, 0x00000001ul,
    0x80080000ul, 0xb0e40000ul, 0x00000005ul, 0x800f0001ul, 0xb0e40001ul, 0xb0e40002ul,
    0x00000002ul, 0x80070001ul, 0x80e40001ul, 0xb0e40003ul, 0x00000005ul, 0x80070001ul,
    0x80e40001ul, 0x90ff0000ul, 0x40000005ul, 0x80080000ul, 0x80e40000ul, 0xb0e40003ul,
    0x00000002ul, 0x80070000ul, 0x80e40000ul, 0x80e40001ul, 0x0000fffful,
};
static const char WaterReferenceSource[] =
    "ps.1.1\n"
    " \t\t\ttex t0 \n"
    "\t\t\ttex t1\t\n"
    "\t\t\ttexbem t2, t1 ; use t1 as env map adjustment on t2.\n"
    "\t\t\tmul r0,v0,t0 ; blend vertex color into t0. \n"
    "\t\t\tmul r1.rgb,t2,c0 ; reduce t2 (environment mapped reflection) by constant\n"
    "\t\t\tadd r0.rgb, r0, r1";
static const unsigned long WaterReferenceCode[] = {
    0xffff0101ul, 0x0009fffeul, 0x58443344ul, 0x68532038ul, 0x72656461ul, 0x73734120ul,
    0x6c626d65ul, 0x56207265ul, 0x69737265ul, 0x30206e6ful, 0x0031392eul, 0x00000042ul,
    0xb00f0000ul, 0x00000042ul, 0xb00f0001ul, 0x00000043ul, 0xb00f0002ul, 0xb0e40001ul,
    0x00000005ul, 0x800f0000ul, 0x90e40000ul, 0xb0e40000ul, 0x00000005ul, 0x80070001ul,
    0xb0e40002ul, 0xa0e40000ul, 0x00000002ul, 0x80070000ul, 0x80e40000ul, 0x80e40001ul,
    0x0000fffful,
};
static const char TrapezoidWaterReferenceSource[] =
    "ps.1.1\n"
    " \t\t\ttex t0 ;get water texture\n"
    "\t\t\ttex t1 ;get white highlights on black background\n"
    "\t\t\ttex t2 ;get white highlights with more tiling\n"
    "\t\t\ttex t3\t; get black shroud \n"
    "\t\t\tmul r0,v0,t0 ; blend vertex color and alpha into base texture. \n"
    "\t\t\tmad r0.rgb, t1, t2, r0\t; blend sparkles and noise \n"
    "\t\t\tmul r0.rgb, r0, t3 ; blend in black shroud \n"
    "\t\t\t;\n"
    "";
static const unsigned long TrapezoidWaterReferenceCode[] = {
    0xffff0101ul, 0x0009fffeul, 0x58443344ul, 0x68532038ul, 0x72656461ul, 0x73734120ul,
    0x6c626d65ul, 0x56207265ul, 0x69737265ul, 0x30206e6ful, 0x0031392eul, 0x00000042ul,
    0xb00f0000ul, 0x00000042ul, 0xb00f0001ul, 0x00000042ul, 0xb00f0002ul, 0x00000042ul,
    0xb00f0003ul, 0x00000005ul, 0x800f0000ul, 0x90e40000ul, 0xb0e40000ul, 0x00000004ul,
    0x80070000ul, 0xb0e40001ul, 0xb0e40002ul, 0x80e40000ul, 0x00000005ul, 0x80070000ul,
    0x80e40000ul, 0xb0e40003ul, 0x0000fffful,
};
static const char ProfilerSwizzleReferenceSource[] =
    "ps.1.4\n"
    "texld r0, t0\n"
    "mov r1.a, r0.r\n"
    "mov r2.a, r0.g\n"
    "mov r3.a, r0.b\n"
    "mul r0.rgb, r3.a, c0\n"
    "mad r0.rgb, r2.a, c1, r0\n"
    "mad r0.rgb, r1.a, c2, r0\n"
    "";
static const unsigned long ProfilerSwizzleReferenceCode[] = {
    0xffff0104ul, 0x0009fffeul, 0x58443344ul, 0x68532038ul, 0x72656461ul, 0x73734120ul,
    0x6c626d65ul, 0x56207265ul, 0x69737265ul, 0x30206e6ful, 0x0031392eul, 0x00000042ul,
    0x800f0000ul, 0xb0e40000ul, 0x00000001ul, 0x80080001ul, 0x80000000ul, 0x00000001ul,
    0x80080002ul, 0x80550000ul, 0x00000001ul, 0x80080003ul, 0x80aa0000ul, 0x00000005ul,
    0x80070000ul, 0x80ff0003ul, 0xa0e40000ul, 0x00000004ul, 0x80070000ul, 0x80ff0002ul,
    0xa0e40001ul, 0x80e40000ul, 0x00000004ul, 0x80070000ul, 0x80ff0001ul, 0xa0e40002ul,
    0x80e40000ul, 0x0000fffful,
};
