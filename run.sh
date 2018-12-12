# convert source code to bitcode (IR)
clang -emit-llvm -c $1.c -o $1.bc
# instrument profiler
opt -pgo-instr-gen -instrprof $1.bc -o $1.prof.bc
# generate binary executable with profiler embedded
clang -fprofile-instr-generate $1.prof.bc -o $1.prof
# run the proram to collect profile data. After this run, a filed named default.profraw is created, which contains the profile data
./$1.prof $2
# convert the profile data so that we can use it in LLVM
llvm-profdata merge -output=pgo.profdata default.profraw
# use the profile data as input, and apply the HW1 pass
opt -pgo-instr-use -pgo-test-profile-file=pgo.profdata -load ./PLCAnalyzer/PLCAnalyzer/PLCAnalyzer.so -plcanalyzer $1.bc > /dev/null 

