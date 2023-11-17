run: nat_tb.v nat.v
	rm -f nat_tb.vvp
	rm -f dump.vcd
	iverilog -o nat_tb.vvp nat_tb.v nat.v
	vvp nat_tb.vvp

showdump: dump.vcd
	gtkwave dump.vcd