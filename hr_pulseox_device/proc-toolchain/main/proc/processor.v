/**
 * READ THIS DESCRIPTION!
 *
 * This is your processor module that will contain the bulk of your code submission. You are to implement
 * a 5-stage pipelined processor in this module, accounting for hazards and implementing bypasses as
 * necessary.
 *
 * Ultimately, your processor will be tested by a master skeleton, so the
 * testbench can see which controls signal you active when. Therefore, there needs to be a way to
 * "inject" imem, dmem, and regfile interfaces from some external controller module. The skeleton
 * file, Wrapper.v, acts as a small wrapper around your processor for this purpose. Refer to Wrapper.v
 * for more details.
 *
 * As a result, this module will NOT contain the RegFile nor the memory modules. Study the inputs 
 * very carefully - the RegFile-related I/Os are merely signals to be sent to the RegFile instantiated
 * in your Wrapper module. This is the same for your memory elements. 
 *
 *
 */
module processor(
    // Control signals
    clock,                          // I: The master clock
    reset,                          // I: A reset signal

    // Imem
    address_imem,                   // O: The address of the data to get from imem
    q_imem,                         // I: The data from imem

    // Dmem
    address_dmem,                   // O: The address of the data to get or put from/to dmem
    data,                           // O: The data to write to dmem
    wren,                           // O: Write enable for dmem
    q_dmem,                         // I: The data from dmem

    // Regfile
    ctrl_writeEnable,               // O: Write enable for RegFile
    ctrl_writeReg,                  // O: Register to write to in RegFile
    ctrl_readRegA,                  // O: Register to read from port A of RegFile
    ctrl_readRegB,                  // O: Register to read from port B of RegFile
    data_writeReg,                  // O: Data to write to for RegFile
    data_readRegA,                  // I: Data from port A of RegFile
    data_readRegB                   // I: Data from port B of RegFile
	 
	);

	// Control signals
	input clock, reset;
	
	// Imem
    output [31:0] address_imem;
	input [31:0] q_imem;

	// Dmem
	output [31:0] address_dmem, data;
	output wren;
	input [31:0] q_dmem;

	// Regfile
	output ctrl_writeEnable;
	output [4:0] ctrl_writeReg, ctrl_readRegA, ctrl_readRegB;
	output [31:0] data_writeReg;
	input [31:0] data_readRegA, data_readRegB;

	/* YOUR CODE STARTS HERE */

    /* Wires */

    //PC Wires
    wire [31:0] PC_output, PC_in, PC_adder_output, PC1_N_out;
    wire [31:0] T_rd_mux_out, B_J_mux_out, Pure_jump_bypass_correction_mux_out;
    wire PC_adder_OVF, PC1_N_OVF;

    //FD Latch Wires
    wire [63:0] FD_Latch_input, FD_Latch_output;
    wire [31:0] FD_Latch_PC, FD_Latch_Instr, FD_Bypass_mux_out;
    wire [16:0] FD_immediate_wire;
    wire [4:0] FD_opcode_wire, FD_rd_wire, FD_rs_wire, FD_rt_wire, FD_shamt_wire, FD_ALU_op_wire;

    //DX Latch Wires
    wire [127:0] DX_Latch_input, DX_Latch_output;
    wire [31:0] DX_Latch_PC, DX_Latch_A, DX_Latch_B, DX_Latch_Instr, DX_target, DX_Bypass_mux_out;
    wire [16:0] DX_immediate_wire;
    wire [4:0] DX_opcode_wire, DX_rd_wire, DX_rs_wire, DX_rt_wire, DX_shamt_wire, DX_ALU_op_wire;
    
    wire [31:0] DX_stalling_mux_out;
    wire  DX_stalling_mux_select;

    //XM Latch Wires
    wire [127:0] XM_Latch_input, XM_Latch_output;
    wire [31:0] XM_Latch_PC, XM_Latch_Instr, XM_Latch_xOut, XM_Latch_B, XM_target, RAM_data_bypass_mux_out;
    wire [16:0] XM_immediate_wire;
    wire [4:0] XM_opcode_wire, XM_rd_wire, XM_rs_wire, XM_rt_wire, XM_shamt_wire, XM_ALU_op_wire;
    wire XM_ErrorFlag_Latch_out, XM_ErrorFlag_mux_out;
    wire RAM_data_bypass_mux_select;

    

    //WB Latch Wires
    wire [127:0] WB_Latch_input, WB_Latch_output;
    wire [31:0] WB_Latch_PC, WB_Latch_Instr, WB_Latch_xOut, WB_Latch_dOut, WB_target;
    wire [31:0] X_D_mux_out, WB_T_PC1_mux_out, WB_xm_ctrl_mux_out, WB_Exception_Value_mux_out, WB_ALU_exception_addi_mux_out, WB_Exception_xOut_mux_out;
    wire [16:0] WB_immediate_wire;
    wire [4:0] WB_opcode_wire, WB_rd_wire, WB_rs_wire, WB_rt_wire, WB_shamt_wire, WB_ALU_op_wire, jal_setx_mux_out, WB_mux_out, WB_Exception_Destination_mux_out;
    wire WB_ErrorFlag_Latch_out;

    //execute_ALU Wires
    wire [31:0] execute_ALU_out_wire, SEX_DX_immediate_wire, B_immediate_mux_out, ALU_multDiv_mux_out;
    wire [4:0] ALU_op_control;
    wire execute_isNotEqual, execute_isLessThan, execute_overflow, R_S_ALU_mux_sel;

    //RegFile Wires
    wire [4:0] rs_rd_mux_out, rt_rs_mux_out, rt_rd_mux_out, rs_rstatus_mux_out;

    //multDiv Wires
    wire [31:0] multDiv_out;
    wire  multDiv_ctrl_DFF_out;
    wire multDiv_exception, multDiv_dataRDY, start_mult, start_div;

    //control wires
    wire [4:0] DX_opcode_OR;
    wire PC_ctrl_mux_select, B_J_mux_select, T_rd_mux_select, Pure_jump_bypass_correction_mux_select;
    wire rs_rstatus_mux_select, rs_rd_mux_select, rt_rs_mux_select, rt_rd_mux_select;
    wire B_Imm_mux_select, ALU_op_mux;
    wire RAM_WE;
    wire X_D_mux_select, jal_setx_mux_select, WB_mux_select;
    wire WB_T_PC1_mux_select, WB_xm_ctrl_mux_select, Reg_WE;
    wire assert_mult, assert_div, multDiv_start, ALU_multDiv_mux_control;
    wire reg_latch_enable;

    // Hazard Detection Unit Wires
    wire A_WB_XM_Hazard_mux_select, A_WB_XM_BexSetx_select, A_BexSetx_vs_other_Hazard_mux_select;
    wire ALU_A_Bypass_mux_select, ALU_A_Bypass_mux_or_EXCEPTION_mux_select;
    wire B_WB_XM_Hazard_mux_select, ALU_B_Bypass_mux_select, ALU_B_Bypass_mux_or_EXCEPTION_mux_select;

    // ALU A Bypass Mux Outputs
    wire [31:0] A_WB_XM_Hazard_mux_out, A_WB_XM_BexSetx_mux_out, A_BexSetx_vs_other_Hazard_mux_out, A_WB_xOut_data_bypassing_mux_out;
    wire [31:0] ALU_A_Bypass_mux_out, ALU_A_Bypass_mux_or_EXCEPTION_mux_out;
    wire [31:0] Bypassing_ALU_exception_addi_mux_out;
    wire A_WB_xOut_data_bypassing_mux_select;

    // ALU B Bypass Mux Outputs
    wire [31:0] B_WB_XM_Hazard_mux_out, ALU_B_Bypass_mux_out, ALU_B_Bypass_mux_or_EXCEPTION_mux_out, B_WB_xOut_data_bypassing_mux_out;
    wire [31:0] Bypassing_Exception_Value_mux_out;
    wire B_WB_xOut_data_bypassing_mux_select;

    //Bypassing Exception Control
    wire [2:0] Bypassing_Exception_Value_selector_mux_out;
    wire Bypassing_ALU_exception_addi_selector_mux_out;






    /*Control Signals*/

        //control module
        control control_module(
            .DX_opcode_OR(DX_opcode_OR),
            .PC_ctrl_mux_select(PC_ctrl_mux_select),
            .B_J_mux_select(B_J_mux_select),
            .T_rd_mux_select(T_rd_mux_select),
            .Pure_jump_bypass_correction_mux_select(Pure_jump_bypass_correction_mux_select),
            .rs_rstatus_mux_select(rs_rstatus_mux_select),
            .rs_rd_mux_select(rs_rd_mux_select),
            .rt_rs_mux_select(rt_rs_mux_select),
            .rt_rd_mux_select(rt_rd_mux_select),
            .B_Imm_mux_select(B_Imm_mux_select),
            .ALU_op_mux(ALU_op_mux),
            .RAM_WE(RAM_WE),
            .X_D_mux_select(X_D_mux_select),
            .jal_setx_mux_select(jal_setx_mux_select),
            .WB_mux_select(WB_mux_select),
            .WB_T_PC1_mux_select(WB_T_PC1_mux_select),
            .WB_xm_ctrl_mux_select(WB_xm_ctrl_mux_select),
            .Reg_WE(Reg_WE),
            .assert_mult(assert_mult),
            .assert_div(assert_div),
            .ALU_multDiv_mux_control(ALU_multDiv_mux_control),

            .FD_opcode_wire(FD_opcode_wire),
            .DX_opcode_wire(DX_opcode_wire),
            .XM_opcode_wire(XM_opcode_wire),
            .WB_opcode_wire(WB_opcode_wire),
            .DX_rd_wire(DX_rd_wire),
            .DX_rs_wire(DX_rs_wire),
            .DX_ALU_op_wire(DX_ALU_op_wire),
            .ALU_A_Bypass_mux_out(ALU_A_Bypass_mux_out),
            .ALU_B_Bypass_mux_out(ALU_B_Bypass_mux_out)
        );
        //either mult or div asserted
        assign multDiv_start = assert_div | assert_mult; //freeze latches when div or mult asserted
        assign reg_latch_enable = ~multDiv_ctrl_DFF_out;


        //Hazard Module
        hazard_detection_unit HDU(
            .A_WB_xOut_data_bypassing_mux_select(A_WB_xOut_data_bypassing_mux_select),
            .B_WB_xOut_data_bypassing_mux_select(B_WB_xOut_data_bypassing_mux_select),
            .A_WB_XM_Hazard_mux_select(A_WB_XM_Hazard_mux_select),
            .A_BexSetx_vs_other_Hazard_mux_select(A_BexSetx_vs_other_Hazard_mux_select),
            .ALU_A_Bypass_mux_select(ALU_A_Bypass_mux_select),
            .B_WB_XM_Hazard_mux_select(B_WB_XM_Hazard_mux_select),
            .ALU_B_Bypass_mux_select(ALU_B_Bypass_mux_select),
            .ALU_A_Bypass_mux_or_EXCEPTION_mux_select(ALU_A_Bypass_mux_or_EXCEPTION_mux_select),
            .ALU_B_Bypass_mux_or_EXCEPTION_mux_select(ALU_B_Bypass_mux_or_EXCEPTION_mux_select),
            .RAM_data_bypass_mux_select(RAM_data_bypass_mux_select),
            .FD_Latch_Instr(FD_Latch_Instr),
            .DX_Latch_Instr(DX_Latch_Instr),
            .XM_Latch_Instr(XM_Latch_Instr),
            .WB_Latch_Instr(WB_Latch_Instr),
            .XM_ErrorFlag_Latch_out(XM_ErrorFlag_Latch_out),
            .WB_ErrorFlag_Latch_out(WB_ErrorFlag_Latch_out),
            .DX_stalling_mux_select(DX_stalling_mux_select)
        );


    /* Instruction decoders and parsing */
        /* FD Instruction */
            //parse instructions
            assign FD_opcode_wire = FD_Latch_Instr[31:27];
            assign FD_rd_wire = FD_Latch_Instr[26:22];
            assign FD_rs_wire = FD_Latch_Instr[21:17];
            assign FD_rt_wire = FD_Latch_Instr[16:12];
            assign FD_shamt_wire = FD_Latch_Instr[11:7];
            assign FD_ALU_op_wire = FD_Latch_Instr[6:2];

            //immediate for i-type
            assign FD_immediate_wire = FD_Latch_Instr[16:0];

        /* DX Instruction */
            //parse instructions
            assign DX_opcode_wire = DX_Latch_Instr[31:27];
            assign DX_rd_wire = DX_Latch_Instr[26:22];
            assign DX_rs_wire = DX_Latch_Instr[21:17];
            assign DX_rt_wire = DX_Latch_Instr[16:12];
            assign DX_shamt_wire = DX_Latch_Instr[11:7];
            assign DX_ALU_op_wire = DX_Latch_Instr[6:2];

            //immediate for i-type
            assign DX_immediate_wire = DX_Latch_Instr[16:0];

            //target for j type
            assign DX_target = {{5{DX_Latch_Instr[26]}}, DX_Latch_Instr[26:0]}; //sign extend target


        /* XM Instructions */
            //parse instructions
            assign XM_opcode_wire = XM_Latch_Instr[31:27];
            assign XM_rd_wire = XM_Latch_Instr[26:22];
            assign XM_rs_wire = XM_Latch_Instr[21:17];
            assign XM_rt_wire = XM_Latch_Instr[16:12];
            assign XM_shamt_wire = XM_Latch_Instr[11:7];
            assign XM_ALU_op_wire = XM_Latch_Instr[6:2];

            //immediate for i-type
            assign XM_immediate_wire = XM_Latch_Instr[16:0];

            //target for j type
            assign XM_target = {{5{XM_Latch_Instr[26]}}, XM_Latch_Instr[26:0]}; //sign extend target

        /* WB Instructions */
            //parse instructions
            assign WB_opcode_wire = WB_Latch_Instr[31:27];
            assign WB_rd_wire = WB_Latch_Instr[26:22];
            assign WB_rs_wire = WB_Latch_Instr[21:17];
            assign WB_rt_wire = WB_Latch_Instr[16:12];
            assign WB_shamt_wire = WB_Latch_Instr[11:7];
            assign WB_ALU_op_wire = WB_Latch_Instr[6:2];

            //immediate for i-type
            assign WB_immediate_wire = WB_Latch_Instr[16:0];

            //target for j type
            assign WB_target = {{5{WB_Latch_Instr[26]}}, WB_Latch_Instr[26:0]}; //sign extend target


    /* Program Counter */

        //setup PC
        register PC_register(PC_output, PC_in, reset, (reg_latch_enable & ~DX_stalling_mux_select), ~clock ); //rising edge, always enabled

        CLA32 PC_adder(PC_adder_output, PC_adder_OVF, PC_output, 32'b1, 1'b0); //carry in zero
        CLA32 PC1_N(PC1_N_out, PC1_N_OVF, DX_Latch_PC, SEX_DX_immediate_wire, 1'b0); //add DX pc +1 with DX immediate

            //Mux tree for PC ctrl
            mux_2 T_rd_mux(T_rd_mux_out, T_rd_mux_select, ALU_A_Bypass_mux_or_EXCEPTION_mux_out, ALU_A_Bypass_mux_or_EXCEPTION_mux_out); //if 1, choose rd; jr instr ********Adjust DX_Latch_A for bypass, should be mux output not DX_Latch_A********
            mux_2 Pure_jump_bypass_correction_mux(Pure_jump_bypass_correction_mux_out, Pure_jump_bypass_correction_mux_select, T_rd_mux_out, DX_target);
            mux_2 B_J_mux(B_J_mux_out, B_J_mux_select, Pure_jump_bypass_correction_mux_out, PC1_N_out); //if 1, choose PC + 1 + N
            mux_2 PC_ctrl_mux(PC_in, PC_ctrl_mux_select, PC_adder_output, B_J_mux_out); //if 1, take the B_J_mux out 


        //Handle Ins ROM
        assign address_imem = PC_output; //imem address = current PC, not PC +1


    
    /*Pipeline Latches*/
        /* FD Latch ********************* */
        
        mux_2 FD_Bypass_mux(FD_Bypass_mux_out, PC_ctrl_mux_select, q_imem, 32'b0); //if taken branch, fill in with Nop
        
        assign FD_Latch_input = {PC_adder_output, FD_Bypass_mux_out}; //uper 32 bits = PC + 1, will use for control later

        register64 FD_Latch(FD_Latch_output, FD_Latch_input, reset, (reg_latch_enable & ~DX_stalling_mux_select), ~clock); //enable to latch always 1, falling edge reg

        assign FD_Latch_PC = FD_Latch_output[63:32];
        assign FD_Latch_Instr = FD_Latch_output[31:0];


        /* DX Latch ********************* */
        //Upper 32b should be PC from FD_Latch_output, lower 32b should be instr passed from FD latch

        mux_2 DX_Bypass_mux(DX_Bypass_mux_out, PC_ctrl_mux_select, FD_Latch_Instr, 32'b0); //if taken branch, flush instr with nop
                                                                          
                                                                            
        assign DX_Latch_input = {FD_Latch_PC, data_readRegA, data_readRegB, DX_stalling_mux_out};

        //DX Latch
        register128 DX_Latch(DX_Latch_output, DX_Latch_input, reset, reg_latch_enable, ~clock); //enable to latch always 1, falling edge reg
        assign DX_Latch_Instr = DX_Latch_output[31:0];
        assign DX_Latch_B = DX_Latch_output[63:32];
        assign DX_Latch_A = DX_Latch_output[95:64];
        assign DX_Latch_PC = DX_Latch_output[127:96];

        //Stalling Logic
        mux_2 DX_stalling_mux(DX_stalling_mux_out, DX_stalling_mux_select, DX_Bypass_mux_out, 32'b0 ); 


        /* XM Latch ******************** */
        assign XM_Latch_input = {DX_Latch_PC, ALU_multDiv_mux_out, ALU_B_Bypass_mux_or_EXCEPTION_mux_out, DX_Latch_Instr};

        //XM Latch
        register128 XM_Latch(XM_Latch_output, XM_Latch_input, reset, reg_latch_enable, ~clock);
        assign XM_Latch_PC = XM_Latch_output[127:96];
        assign XM_Latch_xOut = XM_Latch_output[95:64];
        assign XM_Latch_B = XM_Latch_output[63:32];
        assign XM_Latch_Instr = XM_Latch_output[31:0];

        //XM_ErrorFlag_Latch
        mux_2_1bit XM_ErrorFlag_mux(XM_ErrorFlag_mux_out, multDiv_start, execute_overflow, multDiv_exception); //if multDiv instr, take the multDIv exception
        //wire annoying_bullshit_out;
        dffe_ref XM_ErrorFlag_Latch(XM_ErrorFlag_Latch_out, XM_ErrorFlag_mux_out, ~clock, reg_latch_enable, reset);
        //dffe_ref annoying_bullshit(annoying_bullshit_out, 1'b1, reset, 1'b1, ~clock );

        /* WB Latch ********************* */ 
        assign WB_Latch_input = {XM_Latch_PC, XM_Latch_xOut, q_dmem, XM_Latch_Instr};

        //WB Latch
        register128 WB_Latch(WB_Latch_output, WB_Latch_input, reset, reg_latch_enable, ~clock);
        assign WB_Latch_PC = WB_Latch_output[127:96];
        assign WB_Latch_xOut = WB_Latch_output[95:64];
        assign WB_Latch_dOut = WB_Latch_output[63:32];
        assign WB_Latch_Instr = WB_Latch_output[31:0];

        //WB_ErrorFlag_Latch
        dffe_ref WB_ErrorFlag_Latch(WB_ErrorFlag_Latch_out, XM_ErrorFlag_Latch_out, ~clock, reg_latch_enable, reset);

        
    /*Register File Handling*/

        //Regfile muxes
        mux_2_5bit rs_rstatus_mux(rs_rstatus_mux_out, rs_rstatus_mux_select, FD_rs_wire, 5'b11110); //if 1, choose reg 30
        mux_2_5bit rs_rd_mux(rs_rd_mux_out, rs_rd_mux_select, rs_rstatus_mux_out, FD_rd_wire); //if 1 take rd instead of rs

        mux_2_5bit rt_rd_mux(rt_rd_mux_out, rt_rd_mux_select, FD_rt_wire, FD_rd_wire ); //if 1, take rd instead of rt
        mux_2_5bit rt_rs_mux( rt_rs_mux_out, rt_rs_mux_select, rt_rd_mux_out, FD_rs_wire); //if 1 take rs instead of rt

        //Regfile control
        assign ctrl_writeEnable = Reg_WE;
        assign ctrl_readRegA = rs_rd_mux_out; //choose rs or rd and rs or register 30
        assign ctrl_readRegB = rt_rs_mux_out; //choose rs or rt
        
        
        assign ctrl_writeReg = WB_Exception_Destination_mux_out; //write to reg determined by WB control muxes
        assign data_writeReg = WB_Exception_xOut_mux_out; //data to write to the register, from WB mux tree


    /*Execute Stage*/

        /*ALU Handling*/

            assign SEX_DX_immediate_wire = { {15{DX_immediate_wire[16]}}, DX_immediate_wire }; //sign extend the immediate
            mux_2 B_immediate_mux(B_immediate_mux_out, B_Imm_mux_select, ALU_B_Bypass_mux_or_EXCEPTION_mux_out, SEX_DX_immediate_wire); //if control is 1 (i-type), choose SEX option

            assign R_S_ALU_mux_sel = (DX_opcode_wire == 5'b0); //if the opcode is all zeros, its an R-type, set 1
            mux_2_5bit R_S_ALU_mux(ALU_op_control, R_S_ALU_mux_sel, 5'b0, DX_ALU_op_wire );//if opcode == 0, I-type, only add

            alu execute_ALU(ALU_A_Bypass_mux_or_EXCEPTION_mux_out, B_immediate_mux_out, ALU_op_control, DX_shamt_wire, execute_ALU_out_wire, execute_isNotEqual, execute_isLessThan, execute_overflow);

        /*Bypassing Handling*/

        //ALU A
        mux_2 A_WB_xOut_data_bypassing_mux(A_WB_xOut_data_bypassing_mux_out, A_WB_xOut_data_bypassing_mux_select, WB_Latch_xOut, WB_Latch_dOut);

        mux_2 A_WB_XM_Hazard_mux(A_WB_XM_Hazard_mux_out, A_WB_XM_Hazard_mux_select, A_WB_xOut_data_bypassing_mux_out, XM_Latch_xOut); //select 1 if XM bypass needed

        mux_2 A_BexSetx_vs_other_Hazard_mux(A_BexSetx_vs_other_Hazard_mux_out, A_BexSetx_vs_other_Hazard_mux_select, A_WB_XM_Hazard_mux_out, DX_target ); //some jump?

        mux_2 ALU_A_Bypass_mux(ALU_A_Bypass_mux_out, ALU_A_Bypass_mux_select, DX_Latch_A, A_BexSetx_vs_other_Hazard_mux_out); //ALU_A_Bypass_mux_out now being fed into execute
        //Adding mux to the front because easier, not pretty
        mux_2 ALU_A_Bypass_mux_or_EXCEPTION_mux(ALU_A_Bypass_mux_or_EXCEPTION_mux_out, ALU_A_Bypass_mux_or_EXCEPTION_mux_select, ALU_A_Bypass_mux_out, Bypassing_ALU_exception_addi_mux_out);//if 1, exception ahead, take that 

        //ALU B

        mux_2 B_WB_xOut_data_bypassing_mux(B_WB_xOut_data_bypassing_mux_out, B_WB_xOut_data_bypassing_mux_select, WB_Latch_xOut, WB_Latch_dOut);
                                                                                                   
        mux_2 B_WB_XM_Hazard_mux(B_WB_XM_Hazard_mux_out, B_WB_XM_Hazard_mux_select, B_WB_xOut_data_bypassing_mux_out, XM_Latch_xOut); 
        mux_2 ALU_B_Bypass_mux(ALU_B_Bypass_mux_out, ALU_B_Bypass_mux_select, DX_Latch_B, B_WB_XM_Hazard_mux_out);

        mux_2 ALU_B_Bypass_mux_or_EXCEPTION_mux(ALU_B_Bypass_mux_or_EXCEPTION_mux_out, ALU_B_Bypass_mux_or_EXCEPTION_mux_select, ALU_B_Bypass_mux_out, Bypassing_ALU_exception_addi_mux_out);//if 1, exception ahead, take that 

        //Exceptions 
        mux_2_3bit Bypassing_Exception_Value_selector_mux(Bypassing_Exception_Value_selector_mux_out, XM_ErrorFlag_Latch_out, WB_ALU_op_wire[2:0], XM_ALU_op_wire[2:0]);
        mux_2_1bit Bypassing_ALU_exception_addi_selector_mux(Bypassing_ALU_exception_addi_selector_mux_out, XM_ErrorFlag_Latch_out, WB_opcode_wire[0], XM_opcode_wire[0]);

        mux_8 Bypassing_Exception_Value_mux(Bypassing_Exception_Value_mux_out, Bypassing_Exception_Value_selector_mux_out, 32'd1, 32'd3, 32'd0, 32'd0, 32'd0, 32'd0, 32'd4, 32'd5);
        mux_2 Bypassing_ALU_exception_addi_mux(Bypassing_ALU_exception_addi_mux_out, Bypassing_ALU_exception_addi_selector_mux_out, WB_Exception_Value_mux_out, 32'd2); //if ALu op, take above, else use addi errof val



        /*MultDiv Handling*/
        
            multdiv multDiv_unit(
                ALU_A_Bypass_mux_or_EXCEPTION_mux_out, ALU_B_Bypass_mux_or_EXCEPTION_mux_out,
                start_mult, start_div, clock,
                multDiv_out,
                multDiv_exception, multDiv_dataRDY
            );

            //when multDiv asserted, enable dff, latch 1 : reset when multDiv_dataRDY asserted
            dffe_ref multDiv_ctrl_DFF(multDiv_ctrl_DFF_out, 1'b1, clock, multDiv_start, multDiv_dataRDY);

            //series DFFs, should enable mult / div for one cycle, reset on data rdy
            multDiv_assert_unit multDiv_start_unit(start_mult, start_div, assert_mult, assert_div, clock, multDiv_dataRDY);

            //ALU control
            mux_2 ALU_multDiv_mux(ALU_multDiv_mux_out, ALU_multDiv_mux_control, execute_ALU_out_wire, multDiv_out);

    /*Memory Stage*/

        assign address_dmem = XM_Latch_xOut; //should be the computed address $rs + N
        mux_2 RAM_data_bypass_mux(RAM_data_bypass_mux_out, RAM_data_bypass_mux_select, XM_Latch_B, WB_Latch_dOut ); //bypass dmem if sw after lw
        assign data = RAM_data_bypass_mux_out; //should be B, which should be $rd
        assign wren = RAM_WE;


    /*Write Back Stage*/

        //Write Back Muxes

            //Data 
            mux_2 X_D_mux(X_D_mux_out, X_D_mux_select, WB_Latch_xOut, WB_Latch_dOut); //if 1, choose data mem -> load word
            
            mux_2 WB_T_PC1_mux(WB_T_PC1_mux_out, WB_T_PC1_mux_select, WB_Latch_PC, WB_target );
            
            mux_2 WB_xm_ctrl_mux(WB_xm_ctrl_mux_out, WB_xm_ctrl_mux_select, X_D_mux_out, WB_T_PC1_mux_out); //choose WB latch out or T/PC+1

            mux_8 WB_Exception_Value_mux(WB_Exception_Value_mux_out, WB_ALU_op_wire[2:0], 32'd1, 32'd3, 32'd0, 32'd0, 32'd0, 32'd0, 32'd4, 32'd5);
            mux_2 WB_ALU_exception_addi_mux(WB_ALU_exception_addi_mux_out, WB_opcode_wire[0], WB_Exception_Value_mux_out, 32'd2); //if ALu op, take above, else use addi errof val

            mux_2 WB_Exception_xOut_mux(WB_Exception_xOut_mux_out, WB_ErrorFlag_Latch_out, WB_xm_ctrl_mux_out, WB_ALU_exception_addi_mux_out);


            //Destination
            mux_2_5bit jal_setx_mux(jal_setx_mux_out, jal_setx_mux_select, 5'b11110, 5'b11111); //if 1, write to reg 31, else write to reg 30
            
            mux_2_5bit WB_mux(WB_mux_out, WB_mux_select, WB_rd_wire, jal_setx_mux_out); //if 1, take the control mux, else take rs from WB latch

            mux_2_5bit WB_Exception_Destination_mux(WB_Exception_Destination_mux_out, WB_ErrorFlag_Latch_out, WB_mux_out, 5'b11110); //if exception flag, write to reg 30

        //Error Muxes





	/* END CODE */

endmodule
