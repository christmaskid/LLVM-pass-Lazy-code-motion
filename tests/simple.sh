for testcase in tests/simple/*.c; do
    echo ${testcase}
    clang ${testcase}
    ./a.out > original.output
    rm -f ./a.out
    clang -emit-llvm -S \
        -fpass-plugin=`echo tests/llvm-pass-skeleton/build/LCM/LCMPass.so` \
        ${testcase} 2>/dev/null
    ./a.out > optimize.output
    diff original.output optimize.output > ${testcase}.compare
    if [ -z "echo $testcase.compare" ]; then
        echo "${testcase} pass."
    else
        echo "${testcase} did not pass."
        cat ${testcase}.compare
    fi
    rm -f original.output optimize.output ${testcase}.compare
done