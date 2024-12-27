cd tests/test-suite-build
lit -v -j 1 -o ../../basic_lcm.json  -Dpasses="mem2reg,gvn,simplifycfg" \
        -DLOAD_PASS="opt -load-pass-plugin tests/llvm-pass-skeleton/build/LCM/LCMPass.so -passes=LCMPass" .
cd ../../
tests/test-suite/utils/compare.py basic_lcm.json