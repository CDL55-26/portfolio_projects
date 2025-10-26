module BS16(
    input shift_select,
    input lr_shift,
    input [31:0] input_val,
    output [31:0] final_val
);

// forgot bout mux select, naming is bad cuz had to switch vars and im dumb. output_val is the shifted value, final_valis 
//the mux output

wire [31:0] lls_shifted, ras_shifted, shift_out;

wire Aw;
assign Aw = input_val[31];

mux_2 LR_mux(shift_out,lr_shift, lls_shifted, ras_shifted);
mux_2 shift_mux(final_val, shift_select, input_val, shift_out);

// Left Logical Shift (lls_shifted)
assign lls_shifted[0] = 1'b0;
assign lls_shifted[1] = 1'b0;
assign lls_shifted[2] = 1'b0;
assign lls_shifted[3] = 1'b0;
assign lls_shifted[4] = 1'b0;
assign lls_shifted[5] = 1'b0;
assign lls_shifted[6] = 1'b0;
assign lls_shifted[7] = 1'b0;
assign lls_shifted[8] = 1'b0;
assign lls_shifted[9] = 1'b0;
assign lls_shifted[10] = 1'b0;
assign lls_shifted[11] = 1'b0;
assign lls_shifted[12] = 1'b0;
assign lls_shifted[13] = 1'b0;
assign lls_shifted[14] = 1'b0;
assign lls_shifted[15] = 1'b0;
assign lls_shifted[16] = input_val[0];
assign lls_shifted[17] = input_val[1];
assign lls_shifted[18] = input_val[2];
assign lls_shifted[19] = input_val[3];
assign lls_shifted[20] = input_val[4];
assign lls_shifted[21] = input_val[5];
assign lls_shifted[22] = input_val[6];
assign lls_shifted[23] = input_val[7];
assign lls_shifted[24] = input_val[8];
assign lls_shifted[25] = input_val[9];
assign lls_shifted[26] = input_val[10];
assign lls_shifted[27] = input_val[11];
assign lls_shifted[28] = input_val[12];
assign lls_shifted[29] = input_val[13];
assign lls_shifted[30] = input_val[14];
assign lls_shifted[31] = input_val[15];


// Right Arithmetic Shift (ras_shifted)
assign ras_shifted[0] = input_val[16];
assign ras_shifted[1] = input_val[17];
assign ras_shifted[2] = input_val[18];
assign ras_shifted[3] = input_val[19];
assign ras_shifted[4] = input_val[20];
assign ras_shifted[5] = input_val[21];
assign ras_shifted[6] = input_val[22];
assign ras_shifted[7] = input_val[23];
assign ras_shifted[8] = input_val[24];
assign ras_shifted[9] = input_val[25];
assign ras_shifted[10] = input_val[26];
assign ras_shifted[11] = input_val[27];
assign ras_shifted[12] = input_val[28];
assign ras_shifted[13] = input_val[29];
assign ras_shifted[14] = input_val[30];
assign ras_shifted[15] = input_val[31];
assign ras_shifted[16] = Aw;
assign ras_shifted[17] = Aw;
assign ras_shifted[18] = Aw;
assign ras_shifted[19] = Aw;
assign ras_shifted[20] = Aw;
assign ras_shifted[21] = Aw;
assign ras_shifted[22] = Aw;
assign ras_shifted[23] = Aw;
assign ras_shifted[24] = Aw;
assign ras_shifted[25] = Aw;
assign ras_shifted[26] = Aw;
assign ras_shifted[27] = Aw;
assign ras_shifted[28] = Aw;
assign ras_shifted[29] = Aw;
assign ras_shifted[30] = Aw;
assign ras_shifted[31] = Aw;


endmodule