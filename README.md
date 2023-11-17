# FPGA-Offloading-Lab0

1. What is the maximum number of clock cycles needed for the module to output a connection ID? This duration could be constant or vary based on the number of active connections.

The cycles needed vary. It depends on the load factor of the hash table.
At best cases, it need `5` cycles.
At worst cases, it may need `4+table_len` cycles.


2. If multiple design choices or implementation methods were considered, list them. Describe their advantages and disadvantages, and explain why you opted for your chosen approach.

Advantages:
- simple

Disadvantage: 
- not pipelined
- unfixed latency

Why we opted for our approach: it's simple.