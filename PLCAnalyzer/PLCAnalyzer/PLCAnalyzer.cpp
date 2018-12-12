//The pass file for final project of EECS583 in Fall 2018
//@auther: Shenghao Lin, Xumiao Zhang

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Dominators.h"
#include "llvm/PassAnalysisSupport.h"

#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"

using namespace llvm;

#define DEBUG_TYPE "plcanalyzer"

namespace {

    struct PLCAnalyzer : public FunctionPass {
        
        static char ID; 

        PLCAnalyzer() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override {

			DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
			AliasAnalysis *AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
			MemorySSA *MSSA = &getAnalysis<MemorySSAWrapperPass>().getMSSA();

			return false;
        }

        void getAnalysisUsage(AnalysisUsage &AU) const {
 
			AU.addRequired<DominatorTreeWrapperPass>();
			AU.addRequired<BranchProbabilityInfoWrapperPass>();
            AU.addRequired<BlockFrequencyInfoWrapperPass>();
			AU.addRequired<AAResultsWrapperPass>();
			AU.addRequired<MemorySSAWrapperPass>();
        }
    };

}

char PLCAnalyzer::ID = 0;
static RegisterPass<PLCAnalyzer> X("plcanalyzer", "PLCAnalyzer");
