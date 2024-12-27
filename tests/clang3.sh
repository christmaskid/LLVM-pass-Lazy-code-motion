cd tests/test-suite-build
lit -v -j 1 -o ../../clang3.json -DOPT_FLAGS="-O3" .
cd ../../
tests/test-suite/utils/compare.py clang3.json