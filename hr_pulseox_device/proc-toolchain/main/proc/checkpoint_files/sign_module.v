module sign_module(
    output [63:0] sc_extended_OPA,
    output [31:0] sc_OPB,
    output invert_flag,
    

    input [31:0] OPA,
    input [31:0] OPB
);

wire [31:0] sc_OPA;
wire [1:0] ovf_wires;

inverter invertA(sc_OPA, ovf_wires[1], OPA, OPA[31]);
inverter invertB(sc_OPB, ovf_wires[0], OPB, OPB[31]); //assign output, done

assign invert_flag = OPA[31] ^ OPB[31]; //if both pos or both neg, no problem, else flip at end of division

assign sc_extended_OPA[63:32] = 32'b0;
assign sc_extended_OPA[31:0] = sc_OPA;


endmodule