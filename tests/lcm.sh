cd tests/test-suite-build
lit -v -j 1 -o ../../lcm.json -Dmem2reg="mem2reg" \
        -DLOAD_PASS="opt -load-pass-plugin tests/llvm-pass-skeleton/build/LCM/LCMPass.so -passes=LCMPass" .
cd ../../
tests/test-suite/utils/compare.py lcm.json