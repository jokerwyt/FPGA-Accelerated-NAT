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

verl: our_ip.v testbench.cpp
	verilator -Wall --trace --cc our_ip.v --exe testbench.cpp -CFLAGS -g -CFLAGS -Wall -CFLAGS -O0
	make -j -C obj_dir -f Vour_ip.mk Vour_ip
	# ./obj_dir/Vour_ip
	echo "run ./obj_dir/Vour_ip and see waveform.vcd for detailed."

clean:
	rm -f nat_tb.vvp
	rm -f dump.vcd
	rm -f test_server
	rm -rf obj_dir