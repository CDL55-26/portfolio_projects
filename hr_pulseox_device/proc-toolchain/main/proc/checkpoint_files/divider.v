module divider(
    data_operandA, data_operandB, 
	ctrl_DIV, 
	clock, 
	data_result, data_exception, data_resultRDY);

    input [31:0] data_operandA, data_operandB;
    input ctrl_DIV, clock;

    output [31:0] data_result;
    output data_exception, data_resultRDY;


    //Start divider

    //wires
    wire [63:0] sc_extended_OPA, reg_mux_out, reg_out, reg_out_shifted, calc_reg_in;
    wire [62:0] acc_Q_noLSB;
    wire [31:0] sc_OPB, TC_out, lsb_mux_out;
    wire [1:0] OVF_bus;
    wire invert_flag, count_wire, div_by_zero, latch_enable;

    //Get sc OpB and OPA
    sign_module signMod(sc_extended_OPA, sc_OPB, invert_flag, data_operandA, data_operandB);//get flag for final val invert + ovf + sign corrected obB and A

    //Counter
    counter count(count_wire,clock,~ctrl_DIV,ctrl_DIV);
    assign latch_enable = ~count_wire;

    //Register
    mux_2_64 reg_mux(reg_mux_out, ~ctrl_DIV, sc_extended_OPA, calc_reg_in); //pick between inital val or reg output (inital when ctrl Div High)
    register64 reg_64(reg_out, reg_mux_out, 1'b0, latch_enable, clock);

    assign reg_out_shifted = reg_out << 1;

    //Add Logic
    Tcomp_unit tc(TC_out, sc_OPB, ~reg_out[63]); //add/sub decided by acc value BEFORE shift. If neg-> add, else sub
    CLA32 cla(acc_Q_noLSB[62:31], OVF_bus[0], reg_out_shifted[63:32], TC_out, ~reg_out[63]); //if msb of ACC == 0, subtract
    assign acc_Q_noLSB[30:0] = reg_out_shifted[31:1]; //input bits are shifted Qbits

    //LSB logic
    mux_2 lsb_mux(lsb_mux_out, calc_reg_in[63], 32'b1, 32'b0); //if post shift,post add acc > 0, add 1
    assign calc_reg_in = {acc_Q_noLSB, lsb_mux_out[0]}; //lsb of new reg input 

    //Inversion logic
    inverter invert(data_result, OVF_bus[1], reg_out[31:0], invert_flag); //pass data result through inverted, if invert_flag high, invert
    
    //Error logic
    assign div_by_zero = ~(|data_operandB[31:0]); //if opB is all zeros, assert 1 
    assign data_exception = (|OVF_bus[1:0] | div_by_zero); //ovf from adder or div by zero
    assign data_resultRDY = count_wire & ~ctrl_DIV; //count down

endmodule