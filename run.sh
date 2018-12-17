# convert source code to bitcode (IR)
clang -emit-llvm -O0 -g -c $1.c -o $1.bc
# Apply the PLCAnalyzer pass
opt -pgo-instr-use -pgo-test-profile-file=pgo.profdata -load ./PLCAnalyzer/PLCAnalyzer/PLCAnalyzer.so -plcanalyzer < $1.bc > /dev/null 
