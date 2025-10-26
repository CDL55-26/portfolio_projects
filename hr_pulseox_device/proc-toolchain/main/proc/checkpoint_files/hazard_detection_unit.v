module hazard_detection_unit(
    output A_WB_XM_Hazard_mux_select,
    output A_BexSetx_vs_other_Hazard_mux_select,
    output ALU_A_Bypass_mux_select,
    output B_WB_XM_Hazard_mux_select,
    output ALU_B_Bypass_mux_select,
    output ALU_A_Bypass_mux_or_EXCEPTION_mux_select,
    output ALU_B_Bypass_mux_or_EXCEPTION_mux_select,
    output A_WB_xOut_data_bypassing_mux_select, 
    output B_WB_xOut_data_bypassing_mux_select,
    output DX_stalling_mux_select,
    output RAM_data_bypass_mux_select,
    input [31:0] FD_Latch_Instr, DX_Latch_Instr, XM_Latch_Instr, WB_Latch_Instr,
    input XM_ErrorFlag_Latch_out, WB_ErrorFlag_Latch_out
);

wire [4:0] FD_opcode_wire, FD_rd_wire, FD_rs_wire, FD_rt_wire, FD_shamt_wire, FD_ALU_op_wire;
wire [16:0] FD_immediate_wire;

wire [4:0] DX_opcode_wire, DX_rd_wire, DX_rs_wire, DX_rt_wire, DX_shamt_wire, DX_ALU_op_wire;
wire [16:0] DX_immediate_wire;
wire [31:0] DX_target;

wire [4:0] XM_opcode_wire, XM_rd_wire, XM_rs_wire, XM_rt_wire, XM_shamt_wire, XM_ALU_op_wire;
wire [16:0] XM_immediate_wire;
wire [31:0] XM_target;

wire [4:0] WB_opcode_wire, WB_rd_wire, WB_rs_wire, WB_rt_wire, WB_shamt_wire, WB_ALU_op_wire;
wire [16:0] WB_immediate_wire;
wire [31:0] WB_target;

wire ALU_A_XM_Arithmetic_Hazard, ALU_A_XM_Branch_Hazard, ALU_A_XM_Memory_Hazard, ALU_A_XM_JR_Hazard;
wire ALU_A_WB_Arithmetic_Hazard, ALU_A_WB_Branch_Hazard, ALU_A_WB_Memory_Hazard, ALU_A_WB_JR_Hazard;

wire ALU_B_XM_Arithmetic_Hazard, ALU_B_XM_Branch_Hazard, ALU_B_XM_Memory_Hazard;
wire ALU_B_WB_Arithmetic_Hazard, ALU_B_WB_Branch_Hazard, ALU_B_WB_Memory_Hazard;

wire ALU_A_Bypass_mux_or_EXCEPTION_mux_Arithmetic, ALU_A_Bypass_mux_or_EXCEPTION_mux_Branch, ALU_A_Bypass_mux_or_EXCEPTION_mux_JR, ALU_A_Bypass_mux_or_EXCEPTION_mux_BEX;
wire ALU_B_Bypass_mux_or_EXCEPTION_mux_Arithmetic, ALU_B_Bypass_mux_or_EXCEPTION_mux_Branch;

/* FD Instruction */
assign FD_opcode_wire = FD_Latch_Instr[31:27];
assign FD_rd_wire = FD_Latch_Instr[26:22];
assign FD_rs_wire = FD_Latch_Instr[21:17];
assign FD_rt_wire = FD_Latch_Instr[16:12];
assign FD_shamt_wire = FD_Latch_Instr[11:7];
assign FD_ALU_op_wire = FD_Latch_Instr[6:2];
assign FD_immediate_wire = FD_Latch_Instr[16:0];

/* DX Instruction */
assign DX_opcode_wire = DX_Latch_Instr[31:27];
assign DX_rd_wire = DX_Latch_Instr[26:22];
assign DX_rs_wire = DX_Latch_Instr[21:17];
assign DX_rt_wire = DX_Latch_Instr[16:12];
assign DX_shamt_wire = DX_Latch_Instr[11:7];
assign DX_ALU_op_wire = DX_Latch_Instr[6:2];
assign DX_immediate_wire = DX_Latch_Instr[16:0];
assign DX_target = {{5{DX_Latch_Instr[26]}}, DX_Latch_Instr[26:0]};

/* XM Instruction */
assign XM_opcode_wire = XM_Latch_Instr[31:27];
assign XM_rd_wire = XM_Latch_Instr[26:22];
assign XM_rs_wire = XM_Latch_Instr[21:17];
assign XM_rt_wire = XM_Latch_Instr[16:12];
assign XM_shamt_wire = XM_Latch_Instr[11:7];
assign XM_ALU_op_wire = XM_Latch_Instr[6:2];
assign XM_immediate_wire = XM_Latch_Instr[16:0];
assign XM_target = {{5{XM_Latch_Instr[26]}}, XM_Latch_Instr[26:0]};

/* WB Instruction */
assign WB_opcode_wire = WB_Latch_Instr[31:27];
assign WB_rd_wire = WB_Latch_Instr[26:22];
assign WB_rs_wire = WB_Latch_Instr[21:17];
assign WB_rt_wire = WB_Latch_Instr[16:12];
assign WB_shamt_wire = WB_Latch_Instr[11:7];
assign WB_ALU_op_wire = WB_Latch_Instr[6:2];
assign WB_immediate_wire = WB_Latch_Instr[16:0];
assign WB_target = {{5{WB_Latch_Instr[26]}}, WB_Latch_Instr[26:0]};

/*Control for stalling*/
assign DX_stalling_mux_select = (DX_opcode_wire == 5'd8) && (DX_rd_wire != 5'd0) && (
    ((FD_opcode_wire == 5'd0) && ((FD_rs_wire == DX_rd_wire) || (FD_rt_wire == DX_rd_wire))) ||
    ((FD_opcode_wire == 5'd5) && (FD_rs_wire == DX_rd_wire)) ||
    (((FD_opcode_wire == 5'd7) || (FD_opcode_wire == 5'd8)) && (FD_rs_wire == DX_rd_wire)) ||
    (((FD_opcode_wire == 5'd2) || (FD_opcode_wire == 5'd6)) && ((FD_rd_wire == DX_rd_wire) || (FD_rs_wire == DX_rd_wire))) ||
    ((FD_opcode_wire == 5'd4) && (FD_rd_wire == DX_rd_wire))
);

/*Control For ALU input A*/

//A_WB_xOut_data_bypassing_mux
assign A_WB_xOut_data_bypassing_mux_select = (WB_opcode_wire == 5'd8);

//A_DX_XM_Hazard_mux ****XM CHECKS****
assign ALU_A_XM_Arithmetic_Hazard = ((DX_opcode_wire==5'd0)||(DX_opcode_wire==5'd5)) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rs_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rs_wire==5'd31))
);
assign ALU_A_XM_Branch_Hazard = ((DX_opcode_wire==5'd2)||(DX_opcode_wire==5'd6)) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rd_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);//for branch rd goes into Alu A
assign ALU_A_XM_Memory_Hazard = ((DX_opcode_wire==5'd7) || (DX_opcode_wire==5'd8)) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rs_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);
assign ALU_A_XM_JR_Hazard = (DX_opcode_wire==5'd4) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rd_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);//for jr rd is the A output  
assign A_WB_XM_Hazard_mux_select = (ALU_A_XM_Arithmetic_Hazard | ALU_A_XM_Branch_Hazard | ALU_A_XM_Memory_Hazard| ALU_A_XM_JR_Hazard); //1 if taking bypass from XM

//A_BexSetx_vs_other_Hazard_mux
assign A_BexSetx_vs_other_Hazard_mux_select = ((DX_opcode_wire==5'd22) && (XM_opcode_wire == 5'd21) && (XM_target != 32'd0)) || (((DX_opcode_wire==5'd22) && (WB_opcode_wire == 5'd21) && (WB_target != 32'd0))); //if setx infront != 0

//ALU_A_Bypass_mux ***WB CHECKS****
assign ALU_A_WB_Arithmetic_Hazard = ((DX_opcode_wire==5'd0)||(DX_opcode_wire==5'd5)) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rs_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rs_wire==5'd31))
);
assign ALU_A_WB_Branch_Hazard = ((DX_opcode_wire==5'd2)||(DX_opcode_wire==5'd6)) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rd_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);//for branch rd goes into Alu A
assign ALU_A_WB_Memory_Hazard = ((DX_opcode_wire==5'd7) || (DX_opcode_wire==5'd8)) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rs_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);
assign ALU_A_WB_JR_Hazard = (DX_opcode_wire==5'd4) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rd_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);//for jr rd is the A output

assign ALU_A_Bypass_mux_select = A_WB_XM_Hazard_mux_select 
   | (ALU_A_WB_Arithmetic_Hazard | ALU_A_WB_Branch_Hazard | ALU_A_WB_Memory_Hazard | ALU_A_WB_JR_Hazard)
   | A_BexSetx_vs_other_Hazard_mux_select;

//ALU_A_Bypass_mux OR EXCEPTION
assign ALU_A_Bypass_mux_or_EXCEPTION_mux_Arithmetic = (((DX_opcode_wire==5'd0)||(DX_opcode_wire==5'd5)) && (XM_ErrorFlag_Latch_out||WB_ErrorFlag_Latch_out) && (DX_rs_wire==5'd30)); //30 for error register
assign ALU_A_Bypass_mux_or_EXCEPTION_mux_Branch = (((DX_opcode_wire==5'd2)||(DX_opcode_wire==5'd6)) && (XM_ErrorFlag_Latch_out||WB_ErrorFlag_Latch_out) && (DX_rd_wire==5'd30)); //check rd for branches
assign ALU_A_Bypass_mux_or_EXCEPTION_mux_JR = ((DX_opcode_wire==5'd4) && (XM_ErrorFlag_Latch_out||WB_ErrorFlag_Latch_out) && (DX_rd_wire==5'd30)); //check rd for branches
assign ALU_A_Bypass_mux_or_EXCEPTION_mux_BEX = (DX_opcode_wire==5'd22) && (XM_ErrorFlag_Latch_out||WB_ErrorFlag_Latch_out); //check rd for branches

assign ALU_A_Bypass_mux_or_EXCEPTION_mux_select = ALU_A_Bypass_mux_or_EXCEPTION_mux_Arithmetic 
   || ALU_A_Bypass_mux_or_EXCEPTION_mux_Branch 
   || ALU_A_Bypass_mux_or_EXCEPTION_mux_JR 
   || ALU_A_Bypass_mux_or_EXCEPTION_mux_BEX;

/*ALU B*/

//A_WB_xOut_data_bypassing_mux
assign B_WB_xOut_data_bypassing_mux_select = (WB_opcode_wire == 5'd8);

//B_DX_XM_Hazard_mux
assign ALU_B_XM_Arithmetic_Hazard = ((DX_opcode_wire==5'd0)||(DX_opcode_wire==5'd5)) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rt_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rt_wire==5'd31))
);
assign ALU_B_XM_Branch_Hazard = ((DX_opcode_wire==5'd2)||(DX_opcode_wire==5'd6)) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rs_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rs_wire==5'd31))
);
//for branch rd goes into Alu A
assign ALU_B_XM_Memory_Hazard = ((DX_opcode_wire==5'd7)) && (
   ( ((XM_opcode_wire==5'd0)||(XM_opcode_wire==5'd5)) && (XM_rd_wire != 5'd0) && (DX_rd_wire==XM_rd_wire) )
   || ((XM_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);

assign B_WB_XM_Hazard_mux_select = ALU_B_XM_Arithmetic_Hazard | ALU_B_XM_Branch_Hazard | ALU_B_XM_Memory_Hazard;

//ALU_B_Bypass_mux
assign ALU_B_WB_Arithmetic_Hazard = ((DX_opcode_wire==5'd0)||(DX_opcode_wire==5'd5)) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rt_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rt_wire==5'd31))
);
assign ALU_B_WB_Branch_Hazard = ((DX_opcode_wire==5'd2)||(DX_opcode_wire==5'd6)) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rs_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rs_wire==5'd31))
);
//for branch rd goes into Alu A
assign ALU_B_WB_Memory_Hazard = ((DX_opcode_wire==5'd7)) && (
   ( ((WB_opcode_wire==5'd0)||(WB_opcode_wire==5'd5)||(WB_opcode_wire==5'd8)) && (WB_rd_wire != 5'd0) && (DX_rd_wire==WB_rd_wire) )
   || ((WB_opcode_wire==5'd3) && (DX_rd_wire==5'd31))
);

assign ALU_B_Bypass_mux_select = B_WB_XM_Hazard_mux_select 
   | (ALU_B_WB_Arithmetic_Hazard | ALU_B_WB_Branch_Hazard | ALU_B_WB_Memory_Hazard);

//ALU_B_Bypass_mux OR EXCEPTION 
assign ALU_B_Bypass_mux_or_EXCEPTION_mux_Arithmetic = (((DX_opcode_wire==5'd0)||(DX_opcode_wire==5'd5)) && (XM_ErrorFlag_Latch_out||WB_ErrorFlag_Latch_out) && (DX_rt_wire==5'd30)); //30 for error register
assign ALU_B_Bypass_mux_or_EXCEPTION_mux_Branch = (((DX_opcode_wire==5'd2)||(DX_opcode_wire==5'd6)) && (XM_ErrorFlag_Latch_out||WB_ErrorFlag_Latch_out) && (DX_rs_wire==5'd30)); //check rd for branches
assign ALU_B_Bypass_mux_or_EXCEPTION_mux_select = ALU_B_Bypass_mux_or_EXCEPTION_mux_Arithmetic || ALU_B_Bypass_mux_or_EXCEPTION_mux_Branch;


//Memory Bypass - sw after lw
assign RAM_data_bypass_mux_select = (XM_opcode_wire == 5'd7) && (WB_opcode_wire == 5'd8) && (XM_rd_wire == WB_rd_wire); //if lw in WB and sw using lw's data in xm

endmodule