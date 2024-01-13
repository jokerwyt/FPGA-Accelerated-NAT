#!/bin/sh


# experiment running example:
# 
# root@plnx:/root# ./test_server.elf -i eth1 -c 1 -l1400 -t 98:f2:b3:c9:9c:c1
# ... 
# ...
# time elapsed: 23310 ms
# data sent 97.576288 Gb
# avg thput: 4.185868 Gbps


# I need to get avg thputs of different args.
# -c 1 4 16 64 256 1024
# -l 0 200 400 600 800 1000 1200 1400

# write a script to run all the experiments and dump the result to a file. 

# iterate -c and -l separately
# when iterating through arg -l, keep -c fixed at 16
# when iterating through arg -c, keep -l fixed at 1400
# and fix other args at -t 98:f2:b3:c9:9c:c1 -i eth1.

for c in 1 4 16 64 256 1024
do
    ./test_server.elf -i eth1 -c $c -l 4000 -t 98:f2:b3:c9:9c:c1 > result/c$c.txt
done

# ten data point, from 0 to 5000, step 500
for l in 0 500 1000 1500 2000 2500 3000 3500 4000 4500 5000
do
    ./test_server.elf -i eth1 -c 1024 -l $l -t 98:f2:b3:c9:9c:c1 > result/l$l.txt
done

# 0,500,1000,1500,2000,2500,3000,3500,4000,4500,5000