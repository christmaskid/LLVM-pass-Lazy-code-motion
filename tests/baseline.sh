cd tests/test-suite-build
lit -v -j 1 -o ../../baseline.json -Dmem2reg="mem2reg" .
cd ../../
tests/test-suite/utils/compare.py baseline.json