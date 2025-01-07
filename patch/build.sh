#!/bin/bash
export PATH=/work/tools/gcc-10.3.0/bin:$PATH
export LD_LIBRARY_PATH=/work/tools/gcc-10.3.0/lib64:$LD_LIBRARY_PATH

set -euxo pipefail

if [ -z ${PROJ_ROOT+x} ]; 
then 
  echo "PROJ_ROOT env not set";
else
  echo "PROJ_ROOT env is '$PROJ_ROOT'";
fi

rm -rf build
mkdir build
cd build
../../software/spike/configure --prefix=$RISCV --without-boost --without-boost-asio --without-boost-regex --enable-commitlog
make -j32
make install
