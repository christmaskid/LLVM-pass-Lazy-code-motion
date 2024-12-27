cd tests
rm -r test-suite-build
mkdir test-suite-build
cd test-suite-build
cmake -DCMAKE_C_COMPILER=/sbin/clang \
      -C../test-suite/cmake/caches/O0.cmake \
      ../test-suite
make