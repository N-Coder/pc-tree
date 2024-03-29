# PC-Trees

This is the code accompanying the paper ["Experimental Comparison of PC-Trees and PQ-Trees"](https://arxiv.org/abs/2106.14805).
The source code for the union-find based PC-tree *UFPC* can be found in the "include" and "src" directories,
where only the top-level files are part of the implementation,
the other directories contain various utilities.
The code for *HsuPC* can be found in the [HsuPCSubmodule](https://github.com/N-Coder/pc-tree/tree/HsuPCSubmodule) branch.
Compiling the code requires cmake and a working OGDF installation.
For an example how to use the trees, see `src/test/construction.cpp` and `src/exec/test_planarity.cpp`.

The source code of other implementations we compared against is not included due to copyright concerns.
It can be downloaded automatically, though, using the script `download/download.sh`.
Note that the conversion of Zanetti to C++ and applying our minor changes to other implementations
needs `clang` and `clang-format`, preferably version 11.

Running the evaluation automatically needs a [Slurm](https://slurm.schedmd.com/) cluster, esp. the `sbatch` command, to run all jobs in parallel.
Furthermore, credentials for a [MongoDB](https://www.mongodb.com/) database and collection for the evaluation results need to be entered in `evaluation.py` and `plots/common.py`.
We provide a preconfigured [Docker container](#via-docker) that provides this environment on a single machine without a runtime overhead.

## Installation

First, you need to check out the project and its submodules.
```shell
git clone https://github.com/N-Coder/pc-tree.git pc-tree
cd pc-tree/
git submodule update --init --recursive
```

Now you can directly build the project:
```shell
# Optionally download libraries for comparison (needs clang and clang-format 11)
cd download/
./download.sh
cd ..

# Build everything 
mkdir build-release
cd build-release/
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DOGDF_DIR=/path/to/ogdf/build-release
# to enable LIKWID also pass -DLIKWID_PERFMON=true and possibly -DLIKWID_DIR=/path/to/likwid/install/
# to also compile other implementations for comparison pass -DOTHER_LIBS=true
make -j 8
```

(Optional) To build the project for evaluating the other java implementations, run the following commands (note that this requires running `download.sh` beforehand):
```shell
cd evaluation/javaEvaluation
./gradlew build
``` 

### via Docker

Alternatively, you can use our Docker container with an environment that is prepared for reproducing our experiments. For this, it suffices to download the `Dockerfile` we provide.
```shell
# Create a virtual network and start a MongoDB instance 
docker network create pc-tree-net
docker run --name pc-tree-mongo --network pc-tree-net --publish 27017:27017 --detach mongo

# Build the image and start the container
docker build --no-cache --pull --tag pc-tree-image .
docker run --name pc-tree-slurm --network pc-tree-net --publish 8888:8888 --tty --interactive pc-tree-image /bin/bash

# now, within the container (e.g. root@9b8368ef788c:~# )
cd /root/pc-tree
# build the binaries (including other libraries for comparison) optimized for your machine. Pass the flag -l to skip the download and compilation of the other libraries to only compare HsuPC, UFPC, and OGDF.
./docker-install.sh
# start the slurm daemon (this needs to be done every time you re-enter the container)
./docker-start.sh
```

To start a jupyter notebook within the container, run the following command and open the printed link to `localhost` in your browser.
```
# jupyter notebook --allow-root --ip=0.0.0.0 --port=8888
```
To see the contents of the MongoDB, point a mongo client (e.g. [MongoDB Compass](https://www.mongodb.com/products/compass)) to `localhost:27017`.
Alternatively, you can also request a `mongo` shell by running the following on the host machine:
```
$ docker exec -ti pc-tree-mongo mongosh
```
If you accidentally exited the `pc-tree-slurm` bash shell, you can re-enter the container (with any previous changes retained) by running:
```
$ docker start -ai pc-tree-slurm
# ./docker-start.sh # to restart slurmd within the container
```

#### Failed Downloads

If downloading one of the other libraries fails, manually applying the following changes in the docker container ensures that the evaluation still compiles and runs for the remaining libraries.  
If one of the C++ libraries is not present, the `CMakeLists.txt` should detect this and only build the remaining targets, thus no action should be necessary.  
If one of the Java libraries `JGraphEd` or `GraphTea` is not present, delete the corresponding adapter file in `evaluation/javaEvaluation/src/main/java/java_evaluation` and comment out the invocation of this adapter in `evaluation/javaEvaluation/src/main/java/java_evaluation/TestRestrictions.java`.
For example, for `GraphTea`, this would mean deleting the file `GraphTeaAdapter.java` and removing the lines
```Java
case GraphTea:
    tree = new GraphTeaAdapter(numLeaves);
    break;
```
in `TestRestrictions.java`.
Subsequently, rerunning `./docker-install.sh` as described above should successfully compile the project.

If you want to skip the entire build process for the java libraries, additionally add the flag `--nojava` to the line
```shell
python3 evaluation.py compile ...
```
in `docker-install.sh`.
 
## Running the Evaluation Code

Once the code is compiled, you can generate some restrictions and test an implementation on them.
The following commands will also work without Slurm and MongoDB.
```shell
$ cd /root/pc-tree/build-release && mkdir out
$ ./make_restrictions_planarity out -n 1000 -m 3000 -p -s 0
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
$ ./make_graphs 100000 200000 1 1 1 out
$ ./test_planarity -t UFPC out/graphn100000e200000s1i0planar.gml
{"c_nodes":16,"file":"out/graphn100000e200000s1i0planar.gml","index":442,"leaves":252,"name":"UFPC",
    "p_nodes":45,"result":true,"size":31,"time":3026,"tp_length":12,"type":"UFPC"}
{"c_nodes":40,"file":"out/graphn100000e200000s1i0planar.gml","fingerprint":"53655202","index":1000,
    "leaves":405,"name":"UFPC","p_nodes":99,"result":true,"size":1,"time":30,"tp_length":0,
    "type":"UFPC","uid":"..."}
...
{"c_nodes":3,"file":"out/graphn100000e200000s1i0planar.gml","index":99908,"leaves":70,"name":"UFPC",
    "p_nodes":16,"result":true,"size":31,"time":1282,"tp_length":1,"type":"UFPC"}
$ ./test_planarity_performance -i out/graphn100000e200000s1i0planar.gml
{
    "edges": 200000,
    "id": "out/graphn100000e200000s1i0planar.gml",
    "nodes": 100000,
    "repetition": 0,
    "results": {
        "BoothLueker::doTest": true,
        "BoothLueker::isPlanarDestructive": true,
        "BoyerMyrvold::isPlanarDestructive": true,
        "CppZanetti::isPlanar": false,
        "HsuPC::isPlanar": true,
        "UFPC::isPlanar": true,
        "stNumbering": 100000
    },
    "times": {
        "BoothLueker::doTest":               183143247,
        "BoothLueker::isPlanarDestructive":  470191620,
        "BoyerMyrvold::isPlanarDestructive": 351698185,
        "CppZanetti::isPlanar":                1763263, // reports non-planar graph (see "results")
        "HsuPC::isPlanar":                    86491828,
        "UFPC::isPlanar":                     72982740,
        "stNumbering":                        82167932
    }
}
```

The following commands (i.e. `evaluation.py batch-*` and the `plots/`) require Slurm and MongoDB.
To generate a small test data set that can be used to roughly reproduce our results within a few hours:
```shell
# in /root/pc-tree/evaluation
mkdir -p out/graphs2n
mkdir -p out/graphs3n
for i in {100000..1000000..200000}; do ../build-release/make_graphs $i $((2*i)) 1 1 1 out/graphs2n; done
for i in {100000..1000000..200000}; do ../build-release/make_graphs $i $((3*i)) 1 1 1 out/graphs3n; done
python3 evaluation.py batch-make-restrictions --nodes-to=15000 --nodes-step=2000 --planar
python3 evaluation.py batch-make-restrictions --nodes-to=15000 --nodes-step=100
python3 evaluation.py batch-make-restrictions-matrix --min-size=10 --max-size=500 --start-seed=1 --count=100
```

The full input data set used by the paper can be obtained with the following configuration, but be aware that running the evaluation will take quite some time:
```shell
for i in {100000..1000000..100000}; do ../build-release/make_graphs $i $((2*i)) 1 1 1 out/graphs2n; done
for i in {100000..1000000..100000}; do ../build-release/make_graphs $i $((3*i)) 1 1 1 out/graphs3n; done
for i in {100000..1000000..100000}; do ../build-release/make_graphs $i $((2*i)) 9 2 1 out/graphs2n; done
for i in {100000..1000000..100000}; do ../build-release/make_graphs $i $((3*i)) 9 2 1 out/graphs3n; done
python3 evaluation.py batch-make-restrictions --planar # uses default args from evaluation.py
python3 evaluation.py batch-make-restrictions --nodes-to=20000 --nodes-step=10
python3 evaluation.py batch-make-restrictions-matrix --min-size=10 --max-size=500 --start-seed=1 --count=1000
```

To run the evaluation and generate the plots from the paper:
```shell
python3 evaluation.py batch-test-planarity --checkpoint=50000 out/graphs2n/*.gml
python3 evaluation.py batch-test-planarity --checkpoint=50000 out/graphs3n/*.gml
python3 evaluation.py batch-test-restrictions
python3 evaluation.py batch-test-matrices --batchsize=100

python3 plots/restrictions.py
python3 plots/planarity.py
```
