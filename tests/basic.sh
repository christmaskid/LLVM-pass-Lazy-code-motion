cd tests/test-suite-build
lit -v -j 1 -o ../../basic.json -Dpasses="mem2reg,gvn,simplifycfg" .
cd ../../
tests/test-suite/utils/compare.py basic.json