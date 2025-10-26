module tff(
    output q,     
    input T,
    input clk,   
    input en,
    input clr 
);
    wire d;

    //input for flipflop

    assign d = T ^ q;

    
    dffe_ref dff(
        .q(q),
        .d(d),
        .clk(clk),
        .en(1'b1), //always enabled
        .clr(clr)
    );

endmodule
