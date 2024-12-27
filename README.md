# Advanced Compiler HW4: LLVM Pass - Lazy Code Motion

## Environment Setup
### LLVM
Install LLVM, CMake, Clang: Directly use the **ws1** work station environment.

Versions:
- LLVM: 18.1.8
- clang: 18.1.8
- CMake: 3.31.1

If your environment does not have these setups, download them by:
```shell
$ wget https://apt.llvm.org/llvm.sh
$ chmod +x llvm.sh
$ sudo ./llvm.sh 18
$ sudo apt-get install -y llvm-18 llvm-18-dev llvm-18-runtime clang-18
$ sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-18 100
$ sudo update-alternatives --install /usr/bin/lli lli /usr/bin/lli-18 100
$ sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100
$ sudo apt install cmake
```

### llvm-test-suite
Download ```lit``` as requirement:
```shell
$ python3 -m venv .venv
$ . .venv/bin/activate
$ pip install git+https://github.com/llvm/llvm-project.git#subdirectory=llvm/utils/lit
$ lit --version
lit 20.0.0dev
```

## Build Instructions
For backbone, use the ```llvm-pass-skeleton``` project. 

**The backbone should already be included inside the ```test``` folder in this zip file.**

To build up the pass:
```shell
$ bash tests/build_skeleton.sh
```

If the folder does not exists, clone it by:
```shell
$ cd tests
$ git clone git@github.com:sampsyo/llvm-pass-skeleton.git
```
And replace all ```SkeletonPass``` by ```LCMPass``` in the files.

## Running the Pass
To test if the pass is built up successfully, compile some ```.c``` with the LCMPass:
```shell
$ clang -fpass-plugin=`echo tests/llvm-pass-skeleton/build/LCM/LCMPass.so` tests/hello.c
```

## Testing
### How to run the test suite

**The needed benchmarks should already be downloaded in this zip file.** 

Build the benchmarks: In this folder, 
```shell
$ bash tests/build_test_suite.sh
```
On wsl workstation, ```/sbin/clang``` is the path to ```clang```. You may need to change it if you're not under this environment.

If the folder does not exist, please clone the ```llvm-test-suite``` by:
```shell
$ cd tests
$ git clone https://github.com/christmaskid/llvm-test-suite.git test-suite
```

### Commands to reproduce performance measurements

#### Simple testcases
In this folder,
```shell
$ bash tests/simple.sh
```
And the results will be displayed on the standard output. 

If you want to see the ```diff``` result file, feel free to comment out sections of the bash script.

#### Complex scenario: Benchmark
In this folder,
```shell
$ bash tests/baseline.sh # baseline results: mem2reg only
$ bash tests/basic.sh # basic LLVM passes: mem2reg, gvn, simplifycfg
$ bash tests/lcm.sh # experiment: mem2reg, LCMPass
$ bash tests/basic_lcm.sh # basic + experiment: mem2reg, gvn, simplifycfg, LCMPass
$ bash tests/clang3.sh # -O3
```
For each experiment, the corresponding json file of results will be in this folder (baseline.json, basic.json, etc.).

The result organized by ```tests/test-suite/utils/compare.py``` will be displayed on the standard output.