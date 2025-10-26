* with the memory elements. Each imem and dmem modules will take 12-bit 
 * addresses and will allow for storing of 32-bit values at each address. 
 * Each memory module should receive a single clock. At which edges, is 
 * purely a design choice (and thereby up to you). 
 * 
 * You must change line 36 to add the memory file of the test you created using the assembler
 * For example, you would add sample inside of the quotes on line 38 after assembling sample.s
 *
 **/

module Wrapper (
    input CLK100MHZ, 			// 100 MHz System Clock // Reset Signal
	output hSync, 		// H Sync Signal
	output vSync, 		// Veritcal Sync Signal
	output[3:0] VGA_R,  // Red Signal Bits
	output[3:0] VGA_G,  // Green Signal Bits
	output[3:0] VGA_B,  // Blue Signal Bits
	input BTNU,
	input BTNL,
	input BTNR,
	input BTND,
	input BTNC,
	
	inout sda,
	inout scl);

	wire rwe, mwe;
	wire[4:0] rd, rs1, rs2;
	wire[31:0] instAddr, instData, 
		rData, regA, regB,
		memAddr, memDataIn, memDataOut;
		
	wire reset, start;
	assign reset = BTNC;
	assign start = BTNU;
	
	//CLOCK wizard shit
	wire locked;
	wire clock6_25mHz;
	wire clock;
	
	assign clock = clock6_25mHz;
	
	 clk_wiz_0 clk_wiz(
        .clk_out1(clock6_25mHz),
        .reset(1'b0),
        .locked(locked),
        .clk_in1(CLK100MHZ)
 );

wire [31:0] heartrate, bloodOx;


    VGAController VGA(
    .clk(CLK100MHZ), 			// 100 MHz System Clock
	.reset(reset), 		// Reset Signal
	.hSync(hSync), 		// H Sync Signal
	.vSync(vSync), 		// Veritcal Sync Signal
	.VGA_R(VGA_R),  // Red Signal Bits
	.VGA_G(VGA_G),  // Green Signal Bits
	.VGA_B(VGA_B),  // Blue Signal Bits
	.BTNU(BTNU),
	.BTNL(BTNL),
	.BTNR(BTNR),
	.BTND(BTND),
	.BOX(bloodOx),
	.HR(heartrate)
    
    );
    
    
	// ADD YOUR MEMORY FILE HERE
	localparam INSTR_FILE = "chipIntegratedSF";
	
	// Main Processing Unit
	processor CPU(.clock(clock), .reset(reset), 
								
		// ROM
		.address_imem(instAddr), .q_imem(instData),
									
		// Regfile
		.ctrl_writeEnable(rwe),     .ctrl_writeReg(rd),
		.ctrl_readRegA(rs1),     .ctrl_readRegB(rs2), 
		.data_writeReg(rData), .data_readRegA(regA), .data_readRegB(regB),
									
		// RAM
		.wren(mwe), .address_dmem(memAddr), 
		.data(memDataIn), .q_dmem(memDataOut)); 
	
	// Instruction Memory (ROM)
	ROM #(.MEMFILE({INSTR_FILE, ".mem"}))
	InstMem(.clk(clock), 
		.addr(instAddr[11:0]), 
		.dataOut(instData));
	
	// Register File
	regfile RegisterFile(.clock(clock), 
		.ctrl_writeEnable(mmio_rfe ? mmio_rfe :rwe), .ctrl_reset(reset), 
		.ctrl_writeReg(mmio_rfe ? 5'd9 : rd),
		.ctrl_readRegA(rs1), .ctrl_readRegB(rs2), 
		.data_writeReg(mmio_rfe ? 32'd1 : rData), .data_readRegA(regA), .data_readRegB(regB));
						
	// Processor Memory (RAM)
	RAM ProcMem(.clk(clock), 
		.wEn(sample_wr_flag ? mmio_ram_we : mwe), 
		.addr(sample_wr_flag ? mmio_ram_ptr : memAddr[11:0]), 
		.dataIn(sample_wr_flag ? sample_output : memDataIn), 
		.dataOut(memDataOut),
		.bloodOx(bloodOx),
		.heartrate(heartrate));
		
    //i2c controller
	wire [31:0] sample_output;
    wire        mmio_ram_we;
    wire [11:0] mmio_ram_ptr;
    wire        sample_wr_flag;
    wire        mmio_rfe;
    wire [15:0] my_leds;
    
    i2c_controller #(
        .PRESCALE(16'd16),
        .STOP_ON_IDLE(1)
    ) i2c_ctrl_inst (
        .start           (start),
        .clk             (clock),
        .rst             (reset),
    
        .sample_output   (sample_output),
        .mmio_ram_we     (mmio_ram_we),
        .mmio_ram_ptr    (mmio_ram_ptr),
        .sample_wr_flag  (sample_wr_flag),
        .mmio_rfe        (mmio_rfe),
    
        .controller_LED  (my_leds),
    
        .i2c_scl         (scl),
        .i2c_sda         (sda)
    );

endmodule