all: test_server test_client

run: nat_tb.v nat.v
	rm -f nat_tb.vvp
	rm -f dump.vcd
	iverilog -o nat_tb.vvp nat_tb.v nat.v
	vvp nat_tb.vvp

showdump: dump.vcd
	gtkwave dump.vcd

test_server: test_server.c
	gcc -g -O0 -o test_server test_server.c -Wall

test_client: test_client.c
	gcc -g -O0 -o test_client test_client.c -Wall

verl: our_ip.v testbench.cpp
	verilator -Wall --trace --cc our_ip.v hash_ip.v --exe testbench.cpp -CFLAGS -g -CFLAGS -Wall -CFLAGS -O0
	make -j -C obj_dir -f Vour_ip.mk Vour_ip
	# ./obj_dir/Vour_ip
	echo "run ./obj_dir/Vour_ip and see waveform.vcd for detailed."

perf_server: perf_test/perf_server.c
	gcc -O0 -g -o perf_server perf_test/perf_server.c -Wall -lpthread

perf_client: perf_test/perf_client.c
	gcc -O0 -g -o perf_client perf_test/perf_client.c -Wall -lpthread

perf_test: perf_server perf_client

clean:
	rm -f nat_tb.vvp
	rm -f dump.vcd
	rm -f test_server
	rm -f test_client
	rm -f perf_server
	rm -f perf_client
	rm -rf obj_dir