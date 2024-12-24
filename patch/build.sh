#!/bin/bash

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
${PROJ_ROOT}/software/spike/configure --prefix=$RISCV --without-boost --without-boost-asio --without-boost-regex --enable-commitlog
make -j16
make install
