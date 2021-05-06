#!/bin/bash

set -e
set -x
set +f

git submodule update --init --recursive

# rm -rf */
# rm -rf ../evaluation/javaEvaluation/src/main/java/java_evaluation/*/
# rm -rf ../libraries/*

wget -O reisle.zip    https://github.com/creisle/pq-trees/archive/refs/heads/master.zip
wget -O gregable.zip  https://github.com/Gregable/pq-trees/archive/refs/heads/master.zip
wget -O graphset.zip  http://graphset.cs.arizona.edu/files/GraphSET_1_011.zip
wget -O zanetti.zip   https://github.com/jppzanetti/PQRTree/archive/refs/heads/master.zip
wget -O jgraphed.zip  https://www3.cs.stonybrook.edu/~algorith/implement/jgraphed/distrib/JGraphEd.zip
wget -O gtea.zip      https://github.com/rostam/GTea/archive/refs/heads/master.zip

wget -O bivoc.tar.gz  http://bioinformatics.cs.vt.edu/~murali/software/downloads/bivoc-1.2.tar.gz
wget -O sagemath.py   https://raw.githubusercontent.com/sagemath/sage/develop/src/sage/graphs/pq_trees.py

wget -O bigint.zip    https://github.com/kasparsklavins/bigint/archive/refs/heads/master.zip
wget -O json.hpp      https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

unzip -q reisle.zip   -d reisle
unzip -q gregable.zip -d gregable
unzip -q graphset.zip -d graphset
unzip -q zanetti.zip  -d zanetti
unzip -q jgraphed.zip -d jgraphed
unzip -q gtea.zip     -d gtea

mkdir -p bivoc
tar -xaf bivoc.tar.gz -C bivoc

unzip -q bigint.zip   -d bigint


mkdir -p \
	../evaluation/sagemath/ \
	../libraries/bivocPQ/ \
	../libraries/creislePQ/ \
	../libraries/gregablePQ/ \
	../libraries/graphSetPQ/ \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jppzanetti/ \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jgraphed/ \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/graphtea/ \
	../libraries/bigInt/

cp sagemath.py \
	../evaluation/sagemath/pq_trees.py

cp bivoc/bivoc-1.2/src/*.h \
	../libraries/bivocPQ/

cp reisle/pq-trees-master/src/* \
	../libraries/creislePQ/

cp gregable/pq-trees-master/* \
	../libraries/gregablePQ/

cp graphset/GraphSET_1_011/*ode.{cpp,h} \
	../libraries/graphSetPQ/
cp graphset/GraphSET_1_011/PQtree.{cpp,h} \
	../libraries/graphSetPQ/

cp zanetti/PQRTree-master/PQRTree/src/pqrtree/* \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jppzanetti/

cp jgraphed/dataStructure/pqTree/* \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jgraphed/
cp jgraphed/dataStructure/Queue.java \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jgraphed/

cp gtea/GTea-master/src/main/java/graphtea/extensions/reports/planarity/planaritypq/pqtree/*.java \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/graphtea/
cp gtea/GTea-master/src/main/java/graphtea/extensions/reports/planarity/planaritypq/pqtree/*/*.java \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/graphtea/

cp bigint/bigint-master/src/* \
	../libraries/bigInt/

cp json.hpp \
	../libraries/


pushd ../libraries/
clang-format -style=file -i bivocPQ/*.h creislePQ/*.{h,cpp} gregablePQ/*.{h,cc} graphSetPQ/*.{h,cpp}
popd

pushd ../evaluation/javaEvaluation/src/main/java/java_evaluation
clang-format -style=file -i **/*.java

sed -i -- 's#graphtea\.extensions\.reports\.planarity\.planaritypq\.pqtree\(\.helpers\|\.exceptions\|\.pqnodes\)\?#java_evaluation.graphtea#g' graphtea/*.java
sed -i -- 's#dataStructure\.pqTree#java_evaluation.jgraphed#g' jgraphed/*.java
sed -i -- 's#pqrtree#java_evaluation.jppzanetti#g' jppzanetti/*.java
popd

cd ..
git apply --check --apply --whitespace=fix download/0001-apply-manual-changes-to-java-libs.patch

pushd download
bash ./convert.sh
popd

git apply --check --apply --whitespace=fix download/0002-apply-manual-changes-to-cpp-libs.patch
git apply --check --apply --whitespace=fix download/0003-apply-manual-changes-to-python-files.patch

echo "Done"
