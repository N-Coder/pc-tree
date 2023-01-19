#!/bin/bash

set -x
set +f

OTHER_LIBS="true"
while getopts 'l' flag; do
  case "${flag}" in
    l) OTHER_LIBS='false' ;;
    *) exit 1 ;;
  esac
done

cat > /etc/slurm/slurm.conf <<EOF
ClusterName=slurm-in-docker
SlurmctldHost=$(hostname -s)
$(slurmd -C | grep NodeName) State=UNKNOWN
PartitionName=slurm-in-docker-partition Nodes=$(hostname -s) Default=YES MaxTime=INFINITE State=UP
MailProg=/bin/true
ProctrackType=proctrack/pgid
SelectType=select/cons_res
SelectTypeParameters=CR_Core
MaxArraySize=200000

EOF

# cd /opt/ogdf/build-debug
# cmake .. \
#     -DBUILD_SHARED_LIBS=ON \
#     -DCMAKE_BUILD_TYPE=Debug \
#     -DOGDF_USE_ASSERT_EXCEPTIONS=ON \
#     -DOGDF_USE_ASSERT_EXCEPTIONS_WITH_STACK_TRACE=ON_LIBUNWIND \
#     -DDOC_INSTALL=ON
# make -j $(nproc)

mkdir -p /opt/ogdf/build-release
cd /opt/ogdf/build-release
cmake .. \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DOGDF_MEMORY_MANAGER=POOL_NTS \
    -DOGDF_USE_ASSERT_EXCEPTIONS=OFF \
    -DDOC_INSTALL=OFF
make -j $(nproc)
chmod -R 755 /opt/ogdf

if [ $OTHER_LIBS = "true" ]
then
	cd /root/pc-tree/download/
	./download.sh
fi

mkdir -p /root/pc-tree/build-release
cd /root/pc-tree/build-release
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DOGDF_DIR=/opt/ogdf/build-release/ -DOTHER_LIBS=$OTHER_LIBS

cd ../evaluation
mkdir -p out
python3 evaluation.py compile --local $( [ $OTHER_LIBS = "false" ] && echo "--nojava" )

