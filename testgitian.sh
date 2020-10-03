make dist
cd fiveg-1.0.0
./autogen.sh
./configure --enable-tests=no
make -j8