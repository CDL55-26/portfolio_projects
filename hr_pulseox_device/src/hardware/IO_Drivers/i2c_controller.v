You said:
timescale 1ns / 1ps
module i2c_controller #(
    parameter integer PRESCALE       = 16'd250, //scale factor for i2c clock 50mHz -> 100kHz 50e6 /(4 * 100e3) = 250
    parameter         STOP_ON_IDLE   = 1 //if nothing being activley transmitted, send stop condition
                                         // currently manually stopping
)(
    input              start,
    input  wire        clk, //global clock 50 mHz rn
    input  wire        rst,

    output reg [31:0]       sample_output, //buffer to send Red and IR data
    output reg              mmio_ram_we,
    output reg [11:0]       mmio_ram_ptr,
    output reg              sample_wr_flag,

    output reg              mmio_rfe,

    output wire [15:0] controller_LED, //debug LEDs

    inout  wire        i2c_scl, //sda and scl lines
    inout  wire        i2c_sda
);

    wire scl_o, scl_t, scl_i;
    wire sda_o, sda_t, sda_i;

    IOBUF scl_buf ( //Verilog primitive, allow inout wire functionality (must release for i2c slave)
        .IO  (i2c_scl), //actual pin
        .I   (scl_o), //value to drive onto line
        .T   (scl_t), //tristate control
        .O   (scl_i) //value read from line
    );

    IOBUF sda_buf (
        .IO  (i2c_sda),
        .I   (sda_o),
        .T   (sda_t),
        .O   (sda_i)
    );

    //setup for i2c_master module
    reg  [6:0]  ctrl_cmd_address;
    reg         ctrl_cmd_start, ctrl_cmd_stop, ctrl_cmd_read, ctrl_cmd_write, ctrl_cmd_write_multiple;
    reg         ctrl_cmd_valid;
    wire        i2cm_cmd_ready;

    reg  [7:0]  ctrl_data_tdata;
    reg         ctrl_data_tvalid, ctrl_data_tlast;
    wire        i2cm_data_tready;

    wire [7:0]  i2cm_data_tdata;
    wire        i2cm_data_tvalid, i2cm_data_tlast;
    reg         ctrl_data_tready;

    i2c_master i2c_m (
        .clk                  (clk),    // O: system clock
        .rst                  (rst),    // O: synchronous reset

        .s_axis_cmd_address   (ctrl_cmd_address),   // O: 7-bit I2C slave address
        .s_axis_cmd_start     (ctrl_cmd_start),     // O: assert to send START
        .s_axis_cmd_read      (ctrl_cmd_read),      // O: assert to read
        .s_axis_cmd_write     (ctrl_cmd_write),     // O: assert to write one byte
        .s_axis_cmd_write_multiple (ctrl_cmd_write_multiple),  // O: assert to write multiple bytes
        .s_axis_cmd_stop      (ctrl_cmd_stop),      // O: assert to send STOP
        .s_axis_cmd_valid     (ctrl_cmd_valid),     // O: assert when command is valid
        .s_axis_cmd_ready     (i2cm_cmd_ready),     // I: high when command is accepted

        .s_axis_data_tdata    (ctrl_data_tdata),    // O: byte to write
        .s_axis_data_tvalid   (ctrl_data_tvalid),   // O: assert when write byte is valid
        .s_axis_data_tready   (i2cm_data_tready),   // I: high when ready for byte
        .s_axis_data_tlast    (ctrl_data_tlast),    // O: assert with final byte

        .m_axis_data_tdata    (i2cm_data_tdata),    // I: byte read from slave
        .m_axis_data_tvalid   (i2cm_data_tvalid),   // I: high when read byte is valid
        .m_axis_data_tready   (ctrl_data_tready),   // O: assert to accept read byte
        .m_axis_data_tlast    (i2cm_data_tlast),    // I: high with final read byte

        .scl_i                (scl_i),       // I: input from SCL pin
        .scl_o                (scl_o),       // O: drives SCL low
        .scl_t                (scl_t),       // O: tristate control for SCL
        .sda_i                (sda_i),       // I: input from SDA pin
        .sda_o                (sda_o),       // O: drives SDA low
        .sda_t                (sda_t),       // O: tristate control for SDA

        .prescale             (PRESCALE),           // O: clock divider for SCL speed
        .stop_on_idle         (STOP_ON_IDLE)        // O: auto STOP if idle
    );

    //states
    localparam [4:0] IDLE               = 5'd0,
                 CFG_START_TRANSAC      = 5'd1,
                 CFG_WRITE_ADDRESS      = 5'd2,
                 CFG_WRITE_DATA         = 5'd3,
                 CFG_END_TRANSAC        = 5'd4,
                 
                 POLL_START_TRANSAC     = 5'd5,
                 POLL_WRITE_ADDRESS     = 5'd6,
                 POLL_RESTART_TRANSAC   = 5'd7,
                 POLL_WAIT_DATA         = 5'd8,
                 POLL_CALC_SAMPLE       = 5'd9,
                 
                 SAMP_START_TRANSAC     = 5'd10,
                 SAMP_WRITE_ADDRESS     = 5'd11,
                 SAMP_RESTART_TRANSAC   = 5'd12,
                 SAMP_WAIT_DATA         = 5'd13,
                 SAMP_READ_NEXT_BYTE    = 5'd14,
                 SAMP_READ_LAST_BYTE    = 5'd15,
                 SAMP_PROCESS_DATA      = 5'd16,

                 SETUP_REG_IO           = 5'd17;


    reg [4:0] state;

    // MAX Registers
    localparam [6:0] MAX30102_ADDR = 7'h57;
    
    localparam [7:0] FIFO_DATA_REG = 8'h07; // TODO -> need to write to FIFO then restart
    localparam [7:0] FIFO_WR_PTR   = 8'h04;
    localparam [7:0] FIFO_RD_PTR   = 8'h06;

    localparam POLL_STAGE_WR = 1'b0;
    localparam POLL_STAGE_RD = 1'b1;
    reg fifo_ptr_stage;

    reg [7:0] fifo_ptr_add [0:1];
    initial begin
        fifo_ptr_add[0] = FIFO_WR_PTR;
        fifo_ptr_add[1] = FIFO_RD_PTR;
    end

    //Polling Logic
    reg [7:0] wr_ptr_buff, rd_ptr_buff;

    wire [4:0] avail_sample;
    assign avail_sample = (wr_ptr_buff - rd_ptr_buff) & 5'b11111; //calculate available samples, pointers are MOD32

    // ROM Block
    reg [2:0] config_index; //index into ROM block

    localparam [7:0] REG_MODE_CONFIG        = 8'h09;
    localparam [7:0] REG_SPO2_CONFIG        = 8'h0A;
    localparam [7:0] REG_LED1_PA            = 8'h0C;
    localparam [7:0] REG_LED2_PA            = 8'h0D;
    localparam [7:0] FIFO_CFG_ROLLOVER      = 8'h08;

    localparam [7:0] MODE_SPO2_EN               = 8'h03;
    localparam [7:0] SPO2_CFG_16UA_100SPS_18BIT = 8'hC7;
    localparam [7:0] LED_CURRENT_25MA           = 8'h7F;
    localparam [7:0] FIFO_CFG_ROLLOVER_NOINT    = 8'h10;

    localparam CONFIG_TABLE_SIZE = 5;
    reg [7:0] config_register_addr [0:CONFIG_TABLE_SIZE-1];
    reg [7:0] config_register_data [0:CONFIG_TABLE_SIZE-1];
    initial begin
        config_register_addr[0] = REG_MODE_CONFIG;       config_register_data[0] = MODE_SPO2_EN;
        config_register_addr[1] = REG_SPO2_CONFIG;       config_register_data[1] = SPO2_CFG_16UA_100SPS_18BIT;
        config_register_addr[2] = REG_LED1_PA;           config_register_data[2] = LED_CURRENT_25MA;
        config_register_addr[3] = REG_LED2_PA;           config_register_data[3] = LED_CURRENT_25MA;
        config_register_addr[4] = FIFO_CFG_ROLLOVER;     config_register_data[4] = FIFO_CFG_ROLLOVER_NOINT;
    end

    //RAM and data logic
    reg [2:0] byte_counter; // Renamed from byte_index for clarity
    reg [23:0] red_sample; //buffer to collect red sample
    reg [23:0] ir_sample;   //ir sample

    reg [9:0] sample_counter;

    reg [11:0] red_ram_ptr;
    reg [11:0] ir_ram_ptr;

    localparam RED_RAM_BASE = 100;
    localparam IR_RAM_BASE  = 600;
    localparam TOTAL_SAMPLES = 500;

    localparam RED_DONE_INDEX = 3'd2;
    localparam IR_DONE_INDEX  = 3'd5;

    always @(*) begin
        sample_output = 0;
        mmio_ram_ptr  = 0;
        mmio_ram_we   = 0;
        sample_wr_flag= 0;

        if ((state == SAMP_WAIT_DATA) &&(byte_counter == RED_DONE_INDEX)) begin
            sample_output = {8'b0, red_sample};
            mmio_ram_ptr  = red_ram_ptr;
            sample_wr_flag = 1;
            mmio_ram_we   = 1;
        end 
        else if ((state == SAMP_WAIT_DATA) &&(byte_counter == IR_DONE_INDEX)) begin
            sample_output = {8'b0, ir_sample};
            mmio_ram_ptr  = ir_ram_ptr;
            sample_wr_flag = 1;
            mmio_ram_we   = 1;
        end
    end

    
    always @(posedge clk or posedge rst) begin
        if (rst) begin //reset all i2c_master control lines on reset
            ctrl_cmd_valid         <= 0;
            ctrl_cmd_start         <= 0;
            ctrl_cmd_write         <= 0;
            ctrl_cmd_write_multiple <= 0;
            ctrl_cmd_read          <= 0;
            ctrl_cmd_stop          <= 0;
            ctrl_cmd_address       <= 0;

            ctrl_data_tdata        <= 0;
            ctrl_data_tvalid       <= 0;
            ctrl_data_tlast        <= 0;
            ctrl_data_tready       <= 0;

            config_index           <= 0;
            fifo_ptr_stage         <= 0;  
            wr_ptr_buff            <= 0;
            rd_ptr_buff            <= 0;
            
            byte_counter           <= 0;
            sample_counter         <= 0;
            red_sample             <= 0;
            ir_sample              <= 0;
            red_ram_ptr            <= RED_RAM_BASE;
            ir_ram_ptr             <= IR_RAM_BASE;

            state                  <= IDLE;
        end 
        else begin
            ctrl_cmd_valid         <= 0; //reset cmd_valid each tick, must manually overide

            if (i2cm_data_tvalid) begin //If master asserts valid data, set tready flag (t for transfer apparently)
                ctrl_data_tready <= 1;
            end 
            else begin
                ctrl_data_tready <= 0;
            end

            case (state)
                IDLE: begin //set lines to zero in idle
                    ctrl_cmd_start          <= 0;
                    ctrl_cmd_write          <= 0;
                    ctrl_cmd_write_multiple <= 0;
                    ctrl_cmd_read           <= 0;
                    ctrl_cmd_stop           <= 0;
                    ctrl_data_tvalid        <= 0;
                    ctrl_data_tlast         <= 0;

                    byte_counter           <= 0;
                    sample_counter         <= 0;
                    red_sample             <= 0;
                    ir_sample              <= 0;
                    red_ram_ptr            <= RED_RAM_BASE;
                    ir_ram_ptr             <= IR_RAM_BASE;
                    mmio_rfe                <= 0;
                    
                    if (start) begin
                        state <= CFG_START_TRANSAC;
                    end
                end

                CFG_START_TRANSAC: begin
                    //Cluster for start signal
                    ctrl_cmd_start   <= 1;
                    ctrl_cmd_stop    <= 0;
                    ctrl_cmd_write_multiple <= 1;
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    // *** KEY LOGIC, make sure valid and cmd_ready flag high -> START handshake
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0; //turn off valid and other signals NEXT clock cycle
                        ctrl_cmd_start <= 0;
                        ctrl_cmd_write_multiple <= 0; //turned off for clarity if vald = 0, nothing else matters
                        state <= CFG_WRITE_ADDRESS;
                    end
                end

                CFG_WRITE_ADDRESS: begin
                   
                    ctrl_data_tdata  <= config_register_addr[config_index]; //start writing registers from ROM
                    ctrl_data_tvalid <= 1; //set write data on line and present tvalid flag 
                    ctrl_data_tlast  <= 0; //make sure tlast not presented
                    
                    if (ctrl_data_tvalid && i2cm_data_tready) begin //handshake for master latching data
                        ctrl_data_tvalid <= 0; //reset valid NEXT cycle <- very important, i think
                        state <= CFG_WRITE_DATA;
                    end
                end

                CFG_WRITE_DATA: begin
                    ctrl_data_tdata  <= config_register_data[config_index];
                    ctrl_data_tvalid <= 1;
                    ctrl_data_tlast  <= 1; //last byte in sequence to write, set tlast with it 
                    
                    if (ctrl_data_tvalid && i2cm_data_tready) begin
                        ctrl_data_tvalid <= 0;
                        state <= CFG_END_TRANSAC;
                    end
                end

                CFG_END_TRANSAC: begin
                    ctrl_cmd_stop    <= 1; //stop cluster
                    ctrl_cmd_valid   <= 1;
    
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0;
                        ctrl_cmd_stop  <= 0;
                        
                        if (config_index == CONFIG_TABLE_SIZE - 1) begin
                            state <= POLL_START_TRANSAC;
                        end 
                        else begin
                            config_index <= config_index + 1;
                            state <= CFG_START_TRANSAC;
                        end
                    end
                end

                POLL_START_TRANSAC: begin
                    //Cluster for start signal
                    ctrl_cmd_start   <= 1;
                    ctrl_cmd_stop    <= 0;
                    ctrl_cmd_write   <= 1; //writing 1 byte
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    // *** KEY LOGIC, make sure valid and cmd_ready flag high -> START handshake
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0; //turn off valid and other signals NEXT clock cycle
                        ctrl_cmd_start <= 0; //no new starts
                        ctrl_cmd_write <= 0; //turned off for clarity if vald = 0, nothing else matters
                        state <= POLL_WRITE_ADDRESS;
                    end
                end

                POLL_WRITE_ADDRESS: begin
                    ctrl_data_tdata  <= fifo_ptr_add[fifo_ptr_stage]; //start writing ptr addresses from mini-ROM
                    ctrl_data_tvalid <= 1; //set write data on line and present tvalid flag 

                    if (ctrl_data_tvalid && i2cm_data_tready) begin //handshake for master latching data
                        ctrl_data_tvalid <= 0; //reset valid NEXT cycle <- very important, i think
                        state <= POLL_RESTART_TRANSAC;
                    end
                end

                POLL_RESTART_TRANSAC: begin
                    ctrl_cmd_start   <= 1; //issuing a restart
                    ctrl_cmd_stop    <= 1; //Start and stop BOTH on, indicates a 1 Byte read
                    ctrl_cmd_read    <= 1; //READING 1 byte
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    // *** KEY LOGIC, make sure valid and cmd_ready flag high -> START handshake
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0; //turn off valid and other signals NEXT clock cycle
                        ctrl_cmd_start <= 0; //no new starts
                        ctrl_cmd_stop  <= 0; //disable stop flag
                        ctrl_cmd_read  <= 0; //turned off for clarity if vald = 0, nothing else matters
                        state <= POLL_WAIT_DATA;
                    end
                end

                POLL_WAIT_DATA: begin
                    if (ctrl_data_tready && i2cm_data_tvalid) begin //Read handshake
                        if(fifo_ptr_stage == POLL_STAGE_WR) begin
                            wr_ptr_buff <= i2cm_data_tdata;
                            fifo_ptr_stage <= POLL_STAGE_RD;
                            
                            state <= POLL_START_TRANSAC;
                        end 
                        else begin
                            rd_ptr_buff <= i2cm_data_tdata;
                            fifo_ptr_stage <= POLL_STAGE_WR; //reset the fifo_ptr counter
                            
                            state <= POLL_CALC_SAMPLE;
                        end
                    end
                end

                POLL_CALC_SAMPLE: begin
                    if (avail_sample != 0)
                        state <= SAMP_START_TRANSAC;
                    else
                        state <= POLL_START_TRANSAC;
                end

                // -- NEW SAMPLING STATE MACHINE STARTS HERE --
                
                SAMP_START_TRANSAC: begin
                    // Start transaction to write the register address
                    ctrl_cmd_start   <= 1;
                    ctrl_cmd_stop    <= 0;
                    ctrl_cmd_write   <= 1; // Writing 1 byte (register address)
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    // Reset byte counter for the upcoming multi-byte read
                    byte_counter <= 0;
                    
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0;
                        ctrl_cmd_start <= 0;
                        ctrl_cmd_write <= 0;
                        state <= SAMP_WRITE_ADDRESS;
                    end
                end

                SAMP_WRITE_ADDRESS: begin
                    // Send the register address we want to read from
                    ctrl_data_tdata  <= FIFO_DATA_REG;
                    ctrl_data_tvalid <= 1;
                    
                    if (ctrl_data_tvalid && i2cm_data_tready) begin
                        ctrl_data_tvalid <= 0;
                        state <= SAMP_RESTART_TRANSAC;
                    end
                end

                SAMP_RESTART_TRANSAC: begin
                    // Issue a restart condition for reading
                    ctrl_cmd_start   <= 1;
                    ctrl_cmd_stop    <= 0; // Don't stop after read - we want multiple bytes
                    ctrl_cmd_read    <= 1;
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0;
                        ctrl_cmd_start <= 0;
                        ctrl_cmd_read  <= 0;
                        state <= SAMP_WAIT_DATA;
                    end
                end

                SAMP_WAIT_DATA: begin
                    // Make sure we're ready to receive data
                    ctrl_data_tready <= i2cm_data_tvalid;
                    
                    if (ctrl_data_tready && i2cm_data_tvalid) begin
                        // Byte received, increment the counter
                        case (byte_counter)
                            3'd0: red_sample[23:16] <= i2cm_data_tdata; // RED MSB
                            3'd1: red_sample[15:8]  <= i2cm_data_tdata;
                            3'd2: red_sample[7:0]   <= i2cm_data_tdata;
                            3'd3: ir_sample[23:16]  <= i2cm_data_tdata; // IR MSB
                            3'd4: ir_sample[15:8]   <= i2cm_data_tdata;
                            3'd5: ir_sample[7:0]    <= i2cm_data_tdata;
                        endcase
                        
                        byte_counter <= byte_counter + 1;
                        
                        // Check if we've read all bytes
                        if (byte_counter == 5) begin
                            // We've received all 6 bytes (0-5), proceed to process data
                            state <= SAMP_PROCESS_DATA;
                        end else if (byte_counter == 4) begin
                            // If this is the 5th byte (index 4), next read will be the last
                            state <= SAMP_READ_LAST_BYTE;
                        end else begin
                            // More bytes to read, request the next one
                            state <= SAMP_READ_NEXT_BYTE;
                        end
                    end
                end

                SAMP_READ_NEXT_BYTE: begin
                    // For bytes 1-4, continue reading without stop
                    ctrl_cmd_start   <= 0;
                    ctrl_cmd_stop    <= 0; // Do NOT stop yet
                    ctrl_cmd_read    <= 1;
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0;
                        ctrl_cmd_read  <= 0;
                        state <= SAMP_WAIT_DATA;
                    end
                end

                SAMP_READ_LAST_BYTE: begin
                    // For the 6th byte (index 5), set stop=1 to indicate last read + stop condition
                    ctrl_cmd_start   <= 0;
                    ctrl_cmd_stop    <= 1; // IMPORTANT: Set stop=1 for the last byte
                    ctrl_cmd_read    <= 1;
                    ctrl_cmd_address <= MAX30102_ADDR;
                    ctrl_cmd_valid   <= 1;
                    
                    if (ctrl_cmd_valid && i2cm_cmd_ready) begin
                        ctrl_cmd_valid <= 0;
                        ctrl_cmd_stop  <= 0;
                        ctrl_cmd_read  <= 0;
                        state <= SAMP_WAIT_DATA;
                    end
                end

                SAMP_PROCESS_DATA: begin
                    // All 6 bytes received, update the RAM pointers for next sample
                    red_ram_ptr <= red_ram_ptr + 1;
                    ir_ram_ptr  <= ir_ram_ptr + 1;
                    
                    // Reset byte counter for next read
                    byte_counter <= 0;
                    
                    // Check if we've read all samples
                    if (sample_counter == TOTAL_SAMPLES - 1) begin
                        state <= SETUP_REG_IO;
                    end else begin
                        sample_counter <= sample_counter + 1;
                        state <= POLL_START_TRANSAC;
                    end
                end
                
                // -- NEW SAMPLING STATE MACHINE ENDS HERE --
                
                SETUP_REG_IO: begin
                    mmio_rfe <= 1;
                    state    <= IDLE;
                end
                
                default: state <= IDLE;
            endcase
        end
    end

    //Debugging
    reg read_occurred;
    reg read_count;
    reg data_live;
    reg missing_seg;
    reg [15:0] sample_portion;
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            read_occurred <= 0;
            read_count    <= 0;
            sample_portion <= 0;
            missing_seg    <= 0;
            data_live      <=0;
        end else begin
            if (sample_wr_flag !=0 )
                read_occurred <= 1;
    
//            if (red_ram_ptr == 278)
//                sample_portion <= red_sample[13:0];
                
            if (sample_counter == 405) begin
                //read_count <= 1;
                sample_portion <= red_sample[17:2] & 16'hffff;
            end
            
            if (state == SAMP_WAIT_DATA) begin
                data_live <= 1;
            end
        end
    end

    wire [4:0] state_ptr;
    assign state_ptr = state;
    assign controller_LED = sample_portion;

endmodule