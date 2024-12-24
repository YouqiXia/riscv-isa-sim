#!/bin/sh

target="stable"
if [ $# == 1 ]; then
    target=$1
fi
echo "target is: $target"
export PATH=/work/tools/gcc-10.3.0/bin:$PATH
export LD_LIBRARY_PATH=/work/tools/gcc-10.3.0/lib64:$LD_LIBRARY_PATH
rm -rf build_tmp
mkdir build_tmp
cd build_tmp
echo "start configuring ..."
../configure --prefix=/workspace/perf_model/tools/spike-release/$target --without-boost --without-boost-asio --without-boost-regex
echo "start make ..."
make -j32
echo "start install ..."
g++ -v
make install

cd ..
rm build_tmp -rf
