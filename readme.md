# DupTree & PrefixTree

The implementation of *DupTree* and *PrefixTree* from [The Locality of Memory Checking](https://eprint.iacr.org/2023/1358).

This repo depends on:
- [OpenSSL](https://github.com/openssl/openssl) for hash functions.
- [LevelDB](https://github.com/google/leveldb) for a key-value database.



## Instructions

- Build and make ```duptree```
   ```bash
    $ cmake .
    $ make
   ```
- Evaluate *DupTree* (Section 5.1)
   ```bash
    $ ./duptree [tree height]
   ```
- Evaluate *PrefixTree* (Section 5.2)
    - Preparations
        - Download workloads from [link](https://drive.google.com/drive/folders/1Q3ItZPFEZZAM-1wAnjsDdZy15UfbF8ir?usp=sharing).
        - Download a cloned and modifiled [repository](https://github.com/yujie6/go-ethereum) of [go-ethereum](https://github.com/ethereum/go-ethereum) to generate database opeartions from the above real-world workload. Put files of operations ```eth00.txt, eth01.txt, eth02.txt, eth03.txt``` in the same directory of ```duptree```.
    - Run
        ```bash
         $ ./duptree
        ```
