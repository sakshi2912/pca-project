# pca-project

## Generate graphs
`python3 script.py`

## Compile
- To compile STM and Mimicing Transactional approach: `make`
- To compile HTM:  `g++ -mrtm -mavx -march=native -fopenmp -o coloring_tsx graph_txn.cpp main_coloring.cpp`

## Env
- HTM will have to be compiled on Intel Sapphire/ Emerald rapids with TSX enabled.
- STM can be compiled on GHC and PSC