all: test_server test_client

run: nat_tb.v nat.v
	rm -f nat_tb.vvp
	rm -f dump.vcd
	iverilog -o nat_tb.vvp nat_tb.v nat.v
	vvp nat_tb.vvp

showdump: dump.vcd
	gtkwave dump.vcd

test_server: test_server.c
	gcc -g -O0 -o test_server test_server.c


clean:
	rm -f nat_tb.vvp
	rm -f dump.vcd
	rm -f test_server
	rm -f test_client