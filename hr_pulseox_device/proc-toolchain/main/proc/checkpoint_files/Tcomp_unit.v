module Tcomp_unit(
    output [31:0] final_val,
    input [31:0] input_val,
    input sub_bit
);

//if sub_bit is 1, the xor will flip all bits in input, if sub = 0, will act as buffer

xor bit_flip_0(final_val[0],input_val[0],sub_bit);
xor bit_flip_1(final_val[1],input_val[1],sub_bit);
xor bit_flip_2(final_val[2],input_val[2],sub_bit);
xor bit_flip_3(final_val[3],input_val[3],sub_bit);
xor bit_flip_4(final_val[4],input_val[4],sub_bit);
xor bit_flip_5(final_val[5],input_val[5],sub_bit);
xor bit_flip_6(final_val[6],input_val[6],sub_bit);
xor bit_flip_7(final_val[7],input_val[7],sub_bit);
xor bit_flip_8(final_val[8],input_val[8],sub_bit);
xor bit_flip_9(final_val[9],input_val[9],sub_bit);
xor bit_flip_10(final_val[10],input_val[10],sub_bit);
xor bit_flip_11(final_val[11],input_val[11],sub_bit);
xor bit_flip_12(final_val[12],input_val[12],sub_bit);
xor bit_flip_13(final_val[13],input_val[13],sub_bit);
xor bit_flip_14(final_val[14],input_val[14],sub_bit);
xor bit_flip_15(final_val[15],input_val[15],sub_bit);
xor bit_flip_16(final_val[16],input_val[16],sub_bit);
xor bit_flip_17(final_val[17],input_val[17],sub_bit);
xor bit_flip_18(final_val[18],input_val[18],sub_bit);
xor bit_flip_19(final_val[19],input_val[19],sub_bit);
xor bit_flip_20(final_val[20],input_val[20],sub_bit);
xor bit_flip_21(final_val[21],input_val[21],sub_bit);
xor bit_flip_22(final_val[22],input_val[22],sub_bit);
xor bit_flip_23(final_val[23],input_val[23],sub_bit);
xor bit_flip_24(final_val[24],input_val[24],sub_bit);
xor bit_flip_25(final_val[25],input_val[25],sub_bit);
xor bit_flip_26(final_val[26],input_val[26],sub_bit);
xor bit_flip_27(final_val[27],input_val[27],sub_bit);
xor bit_flip_28(final_val[28],input_val[28],sub_bit);
xor bit_flip_29(final_val[29],input_val[29],sub_bit);
xor bit_flip_30(final_val[30],input_val[30],sub_bit);
xor bit_flip_31(final_val[31],input_val[31],sub_bit);

endmodule