#!/bin/sh
export PATH=/work/tools/gcc-10.3.0/bin:$PATH
export LD_LIBRARY_PATH=/work/tools/gcc-10.3.0/lib64:$LD_LIBRARY_PATH
rm -rf build
mkdir build
cd build
echo "start configuring ..."
../configure --prefix=$(pwd)/../libs --without-boost --without-boost-asio --without-boost-regex
echo "start make ..."
make -j32
echo "start install ..."
g++ -v
make install

cp libriscv.a ../libs/lib
cp libfdt.a ../libs/lib
cp libfesvr.a ../libs/lib
cp libdisasm.a ../libs/lib
cp libsoftfloat.a ../libs/lib


rm -rf ../libs/lib/libriscv.so
rm -rf ../libs/lib/libsoftfloat.so

