module Barrel_Shifter(
    input [31:0] input_val,
    input [4:0] shifamt,
    input lr_shift,
    output [31:0] final_val
);

wire [31:0] s1_out, s2_out, s4_out, s8_out;

//fucked up argument ordering, inputs first then output
BS1 shifted_1(shifamt[0], lr_shift, input_val, s1_out);
BS2 shifted_2(shifamt[1], lr_shift, s1_out, s2_out);
BS4 shifted_4(shifamt[2], lr_shift, s2_out, s4_out);
BS8 shifted_8(shifamt[3], lr_shift, s4_out, s8_out);
BS16 shifted_16(shifamt[4], lr_shift, s8_out, final_val);


endmodule