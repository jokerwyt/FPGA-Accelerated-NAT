# FPGA-Offloading-Lab0

1. What is the maximum number of clock cycles needed for the module to output a connection ID? This duration could be constant or vary based on the number of active connections.

    The cycles needed vary. It depends on the load factor of the hash table.
    - At best cases, it need `5` cycles.
    - At worst cases, it may need `4+table_len` cycles.


2. If multiple design choices or implementation methods were considered, list them. Describe their advantages and disadvantages, and explain why you opted for your chosen approach.

    We implemented a naive approach for now. Some possible optimizations are listed below:

    1. Pipelining. We can reduce latency to `4` cycles if we parallelize input and hash-table lookup. **(We have implemented a prototype and are testing it)**
    2. Lookup unroll. We can look up multiple location within the same cyle. This benefits in high load case.
    3. Smarter hash function and probing. For now we just xor those bits in input, and use linear probe.
    