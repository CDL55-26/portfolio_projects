module counter(
    output count,
    input clk,   
    input en,
    input clr
);

wire Q0,Q1,Q2,Q3,Q4,Q5, and2, and3, and4, and5, and_check;

tff tff0(Q0,1'b1,clk,en,clr);

tff tff1(Q1,Q0,clk,en,clr);

assign and2 = Q0 & Q1;
tff tff2(Q2,and2,clk,en,clr);

assign and3 = Q2 & and2;
tff tff3(Q3,and3,clk,en,clr);

assign and4 = Q3 & and3;
tff tff4(Q4,and4,clk,en,clr);

assign and5 = Q4 & and4;
tff tff5(Q5,and5,clk,en,clr);

assign count = Q5; //counter output

//Going to need logic for clearing this wwhen mult is asserted 


endmodule