#!/bin/bash

set -x
set +f

git pull
git submodule update --init --recursive

# rm -rf */
# rm -rf ../evaluation/javaEvaluation/src/main/java/java_evaluation/*/
# rm -rf ../libraries/*

wget -O reisle.zip    https://github.com/creisle/pq-trees/archive/7d059a5570f541065c1cf3307e14037f2fd08771.zip # https://archive.softwareheritage.org/swh:1:rev:7d059a5570f541065c1cf3307e14037f2fd08771;origin=https://github.com/creisle/pq-trees;visit=swh:1:snp:d4a67010824fb9c0d4a353107094e7d241fa28a0
wget -O gregable.zip  https://github.com/Gregable/pq-trees/archive/61c7ed7a4a40453a4816a04749ca6ae1229df42c.zip # https://archive.softwareheritage.org/swh:1:rev:61c7ed7a4a40453a4816a04749ca6ae1229df42c;origin=https://github.com/Gregable/pq-trees;visit=swh:1:snp:bc540751bcdc049824ae055ee423b7831c8b6ae5
wget -O graphset.zip  http://graphset.cs.arizona.edu/files/GraphSET_1_011.zip
wget -O zanetti.zip   https://github.com/jppzanetti/PQRTree/archive/6349e2eddbeb67b94e8093174ee9c6f43a822527.zip # https://archive.softwareheritage.org/swh:1:rev:6349e2eddbeb67b94e8093174ee9c6f43a822527;origin=https://github.com/jppzanetti/PQRTree;visit=swh:1:snp:69af8d06a54e26dd6bd1666d993e638e6ff4c5ad
wget -O jgraphed.zip  https://www3.cs.stonybrook.edu/~algorith/implement/jgraphed/distrib/JGraphEd.zip
wget -O gtea.zip      https://github.com/rostam/GTea/archive/8e6db4b0801c3a37ca31a241a6843f74980998e7.zip # https://archive.softwareheritage.org/swh:1:rev:8e6db4b0801c3a37ca31a241a6843f74980998e7;origin=https://github.com/rostam/GTea;visit=swh:1:snp:b0b98f1fb530ad6e08780898f04074ae2c5508ad

wget -O bivoc.tar.gz  http://bioinformatics.cs.vt.edu/~murali/software/downloads/bivoc-1.2.tar.gz
wget -O sagemath.py   https://raw.githubusercontent.com/sagemath/sage/5f756e06afc44ce7f5433fddeddc1e3b177b6f7a/src/sage/graphs/pq_trees.py

# wget -O bigint.zip    https://github.com/kasparsklavins/bigint/archive/0a6367f851895bbb95a4b093dc042f4ceb2c4c3c.zip
# wget -O json.hpp      https://raw.githubusercontent.com/nlohmann/json/v3.9.1/single_include/nlohmann/json.hpp

# md5sum *.zip *.tar.gz *.py *.hpp
# 7df01561c4a4aca0e4713644fa832db3  bigint.zip
# 75827c67e456a6bf7b39deda15410971  graphset.zip
# e554ccda882c8f3041fa566890a39482  gregable.zip
# a8e8a8b7f22314595e04cb0b2dbba61e  gtea.zip
# ce9c7c0951b4c372d42df04b6f0455f8  jgraphed.zip
# 1fcfe0581432c06111ab00134476a1f4  reisle.zip
# fc3c037a435625dbffeeeeb89c7e90d1  zanetti.zip
# 9301845ce1db849347cf728c9d2e5997  bivoc.tar.gz
# 2268d0567b356af74cc2978843eaaa5c  sagemath.py
# 0006494aa70845c788601c5906584d2d  json.hpp

unzip -o -q reisle.zip   -d reisle
unzip -o -q gregable.zip -d gregable
unzip -o -q graphset.zip -d graphset
unzip -o -q zanetti.zip  -d zanetti
unzip -o -q jgraphed.zip -d jgraphed
unzip -o -q gtea.zip     -d gtea

mkdir -p bivoc
tar -xaf bivoc.tar.gz -C bivoc

# unzip -q bigint.zip   -d bigint


mkdir -p \
	../evaluation/sagemath/ \
	../libraries/bivocPQ/ \
	../libraries/creislePQ/ \
	../libraries/gregablePQ/ \
	../libraries/graphSetPQ/ \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jppzanetti/ \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jgraphed/ \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/graphtea/
	# ../libraries/bigInt/

cp sagemath.py \
	../evaluation/sagemath/pq_trees.py

cp bivoc/bivoc-1.2/src/*.h \
	../libraries/bivocPQ/

cp reisle/pq-trees-*/src/* \
	../libraries/creislePQ/

cp gregable/pq-trees-*/* \
	../libraries/gregablePQ/

cp graphset/GraphSET_1_011/*ode.{cpp,h} \
	../libraries/graphSetPQ/
cp graphset/GraphSET_1_011/PQtree.{cpp,h} \
	../libraries/graphSetPQ/

cp zanetti/PQRTree-*/PQRTree/src/pqrtree/* \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jppzanetti/

cp jgraphed/dataStructure/pqTree/* \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jgraphed/
cp jgraphed/dataStructure/Queue.java \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/jgraphed/

cp gtea/GTea-*/src/main/java/graphtea/extensions/reports/planarity/planaritypq/pqtree/*.java \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/graphtea/
cp gtea/GTea-*/src/main/java/graphtea/extensions/reports/planarity/planaritypq/pqtree/*/*.java \
	../evaluation/javaEvaluation/src/main/java/java_evaluation/graphtea/

# cp bigint/bigint-*/src/* \
# 	../libraries/bigInt/

# cp json.hpp \
# 	../libraries/


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
git apply --check --apply --whitespace=fix download/0001-manual-changes-to-graphtea.patch
git apply --check --apply --whitespace=fix download/0002-manual-changes-to-jgraphed.patch
git apply --check --apply --whitespace=fix download/0003-manual-changes-to-jppzanetti.patch
git apply --check --apply --whitespace=fix download/0004-manual-changes-to-sagemath.patch
git apply --check --apply --whitespace=fix download/0005-manual-changes-to-bivocPQ.patch
git apply --check --apply --whitespace=fix download/0006-manual-changes-to-creislePQ.patch
git apply --check --apply --whitespace=fix download/0007-manual-changes-to-graphSetPQ.patch
git apply --check --apply --whitespace=fix download/0008-manual-changes-to-gregablePQ.patch

pushd download
bash ./convert.sh
popd

git apply --check --apply --whitespace=fix download/0009-manual-changes-to-autogenerated-cpp-zanetti.patch

echo "Done"
