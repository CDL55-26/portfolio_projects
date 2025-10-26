module full_adder (S, A, B, Cin);

input A, B, Cin;
output S;

xor Sresult(S, A, B, Cin);

endmodule