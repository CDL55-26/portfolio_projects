module multi_level_Or(
    output or_output,
    input [31:0] input_val
);

wire w1,w2,w3,w4;
or or_0(w1, input_val[0], input_val[1], input_val[2], input_val[3], input_val[4], input_val[5], input_val[6], input_val[7]);
or or_1(w2, input_val[8], input_val[9], input_val[10], input_val[11], input_val[12], input_val[13], input_val[14], input_val[15]);
or or_2(w3, input_val[16], input_val[17], input_val[18], input_val[19], input_val[20], input_val[21], input_val[22], input_val[23]);
or or_3(w4, input_val[24], input_val[25], input_val[26], input_val[27], input_val[28], input_val[29], input_val[30], input_val[31]);

or last_or(or_output,w1,w2,w3,w4);


endmodule