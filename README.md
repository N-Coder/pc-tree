# PC-Trees

This is the code accompanying the paper "Experimental Comparison of PC-Trees and PQ-Trees".
The source code for UFPC can be found in the "include" and "src" directories,
where only the top-level files are part of the implementation,
the other directories contain various utilities.
The code for HsuPC can be found in the [HsuPCSubmodule](https://github.com/N-Coder/pc-tree/tree/HsuPCSubmodule) branch.
Compiling the code requires cmake and a working OGDF installation.
For an example how to use the trees, see `src/test/construction.cpp` and `src/exec/test_planarity.cpp`.

The source code of other implementations we compared against is not included due to copyright concerns.
It can be downloaded automatically, though, using the script `download/download.sh`.
Note that the conversion of Zanetti to C++ and applying our minor changes to other implementations
needs `clang` and `clang-format`, preferably version 11.

Running the evaluation automatically needs a SLURM cluster, esp. the `sbatch` command, to run all jobs in parallel.
Furthermore, credentials for a MongoDB database and collection for the evaluation results need to be entered in `evaluation.py` and `plots/common.py`.

## Installation

```shell
# Check out project and submodules
git clone https://github.com/N-Coder/pc-tree.git pc-tree-public
cd pc-tree-public/
git submodule update --init --recursive

# Optionally download libraries for comparison (needs clang and clang-format 11)
cd download/
./download.sh
cd ..

# Build everything 
mkdir build-release
cd build-release/
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DOGDF_DIR=/scratch/finksim/ogdf/build-release
# to enable LIKWID also pass -DLIKWID_PERFMON=true and possibly -DLIKWID_DIR=~/likwid/install/
# to also compile other implementations for comparison pass -DOTHER_LIBS=true
make -j 8
```

Now you can generate some restrictions and test an implemenation on them:
```shell
$ mkdir out && ./make_restrictions_planarity out -n 1000 -m 3000 -p -s 0
GRAPH:{"edges":3000,"id":"G-n1000-m3000-s0-p1","matrices":["out/G-n1000-m3000-s0-p1_0",...],
  "matrices_count":573,"nodes":1000,"planar":true,"planarity_success":true,"seed":0}
$ ./test_restrictions -t UFPC out/G-n1000-m3000-s0-p1_0
{
    "cols": 61,
    "complete": true,
    "errors": [],
    "exp_possible": true,
    "fraction": 0.09562841530054644,
    "id": "UFPC/G-n1000-m3000-s0-p1_0",
    "idx": 0,
    "init_time": 67180,
    "last_restriction": {
        "c_nodes": 0,
        "cleanup_time": 0,
        "exp_fingerprint": "8759265",
        "exp_possible": true,
        "exp_uid": "65:[57, 59, 60, 58, 64:(63:(61:(48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0), 50, 49), 52, 51), 62:[55, 54], 56, 53]",
        "fingerprint": "8759265",
        "p_nodes": 6,
        "possible": true,
        "size": 4,
        "time": 108152,
        "tpLength": 4,
        "uid": "65:[57, 59, 60, 58, 64:(63:(61:(48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0), 50, 49), 52, 51), 62:[55, 54], 56, 53]"
    },
    "max_fraction": 0.19672131147540983,
    "max_size": 12,
    "name": "UFPC",
    "node_idx": 5,
    "num_ones": 35,
    "parent_id": "G-n1000-m3000-s0-p1",
    "rows": 6,
    "total_cleanup_time": 0,
    "total_restrict_time": 242549,
    "total_time": 309729,
    "tree_type": "UFPC",
    "valid": true
}
```

Or test planarity on a whole graph:
```shell
$ ./test_planarity -t UFPC ../evaluation/graphs/graphs2n/graphn100000e200000s2i8planar.gml
{"c_nodes":52,"file":"graphn100000e200000s2i8planar.gml","fingerprint":"27393384","index":1000,"leaves":491,
 "name":"UFPC","p_nodes":105,"result":true,"size":5,"time":246484,"tp_length":5,"type":"UFPC","uid":"..."}
...
{"c_nodes":5,"file":"graphn100000e200000s2i8planar.gml","index":99810,"leaves":70,
 "name":"UFPC","p_nodes":12,"result":true,"size":30,"time":525288,"tp_length":2,"type":"UFPC"}
$ ./test_planarity_performance -i ../evaluation/graphs/graphs2n/graphn100000e200000s2i8planar.gml
{
    "edges": 200000,
    "id": "graphn100000e200000s2i8planar.gml",
    "nodes": 100000,
    "repetition": 0,
    "results": {
        "BoothLueker::doTest": true,
        "BoothLueker::isPlanarDestructive": true,
        "BoyerMyrvold::isPlanarDestructive": true,
        "HsuPC::isPlanar": true,
        "UFPC::isPlanar": true,
        "stNumbering": 100000
    },
    "times": {
        "BoothLueker::doTest":               217639127,
        "BoothLueker::isPlanarDestructive":  520770152,
        "BoyerMyrvold::isPlanarDestructive": 411274012,
        "HsuPC::isPlanar":                   105999142,
        "UFPC::isPlanar":                     95510479,
        "stNumbering":                        87719342
    }
}
```

If you have a SLURM cluster and a MongoDB, you can also run our complete evaluation suite:

```shell
# Set up evaluation (needs slurm to run jobs and a mongoDB, see evaluation.py) 
cd ../evaluation/
python3 -m venv .venv
source .venv/bin/activate
pip3 install click pymongo sh matplotlib numpy pandas seaborn tabulate tqdm
python3 evaluation.py compile # --local --clean

# Run all the evaluations on the slurm cluster
# pass --help for more information
python3 evaluation.py --help
python3 evaluation.py test-planarity --help
python3 evaluation.py batch-test-planarity --checkpoint=50000 -t types graphs/graphs2n/*.gml
python3 evaluation.py batch-test-planarity --checkpoint=50000 -t types graphs/graphs3n/*.gml
python3 evaluation.py batch-make-restrictions --planar
python3 evaluation.py batch-test-restrictions -q "{\"planar\": true}" -t types
python3 evaluation.py batch-make-restrictions --nodes-to=20000 --nodes-step=10
python3 evaluation.py batch-test-restrictions -q "{\"planar\": false}" -t types

# Generate the plots (update credentials in plots/common.py)
python3 plots/restrictions.py
python3 plots/planarity.py
python3 plots/performance.py
```