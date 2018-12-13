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
#include "llvm/IR/Value.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/IntrinsicInst.h"



#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"

#include <queue>
#include <vector>
#include <set>
#include <map>
#include <string>

using namespace std;

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

			std::vector<Instruction *> store_list;	

			errs() << "\n\n";
			errs().write_escaped(F.getName()) << "\n";

			for (Function::iterator b = F.begin(); b != F.end(); b++) {
				BasicBlock *bb = &(*b);
				
				MemoryPhi *phi = MSSA -> getMemoryAccess(bb);
				//if (phi != NULL) {
				//
				//	phi -> print(errs());
				//	errs().write('\n');
				//}

				for (BasicBlock::iterator i_iter = bb -> begin(); i_iter != bb -> end(); ++ i_iter) {
					Instruction *I = &(*i_iter);
					if (I -> getOpcode() == Instruction::Store) {
						//MemoryUseOrDef *ud = MSSA -> getMemoryAccess(I);
						//if (ud != NULL) {
						//
						//	ud -> print(errs());
						//	errs().write('\n');
						//}
						store_list.push_back(I);						
					}
				}
			}

			for (int i = 0; i < store_list.size(); i ++) {
				set<Value *> queried;
				queue<Value *> to_query;
				to_query.push(store_list[i]);
				
				MemoryDef *MA = dyn_cast<MemoryDef>(MSSA -> getMemoryAccess(store_list[i]));
				if (MA && MA ->getID()){
					errs().write('\n') << MA -> getID();
					if (store_list[i] -> getOperand(1) -> hasName()) {
						errs() << " " << store_list[i] -> getOperand(1) -> getName() << ": \n";
					}
					else {
						errs() << " " << getOriginalName(store_list[i] -> getOperand(1), &F) << ": \n"; 
					}
				}


				while (to_query.size()) {
					Value *dd = to_query.front();

					to_query.pop();

					if (dd == NULL) {
						errs() << "related with function input value\n";

						continue;
					}

					if (queried.find(dd) != queried.end()) continue;
					queried.insert(dd);
				

					if (MemoryPhi *phi = dyn_cast<MemoryPhi>(dd)) {
						for (const auto &op : phi -> operands()) {
							to_query.push(op);
						}

						errs() << "phi\n";
					}
					
					else if (MemoryDef *def = dyn_cast<MemoryDef>(dd)) {
						if (def)
						to_query.push(def -> getMemoryInst());
					}

					else if (Instruction* d = dyn_cast<Instruction>(dd)) {

						if (d -> getOpcode () == Instruction::Store) {
							
							MemoryDef *MD = dyn_cast<MemoryDef>(MSSA -> getMemoryAccess(d));
							errs() << "store " << MD -> getID();

							if (d -> getOperand(1) -> hasName()) {
								errs() << " " << d -> getOperand(1) -> getName() << "\n";
							}
							else {
								errs() << " " << getOriginalName(d -> getOperand(1), &F) << "\n";
							}

							Value *v = d -> getOperand(0);
							
							if (Constant* CI = dyn_cast<Constant>(v)) {
							
								if (GlobalValue *gv = dyn_cast<GlobalValue>(CI)) 
									errs() << "related global value: " << gv -> getName() << "\n";
								else if (CI)
									errs() << "related constant: " << CI -> getUniqueInteger() << "\n";
							
								continue;
							
							}
							
							else {
								to_query.push(dyn_cast<Instruction>(v));
							}
						}
						

						else if (d -> getOpcode() == Instruction::Load) {
							
							errs() << "load";
							if (d -> getOperand(0) -> hasName()) {
								errs() << " " << d -> getOperand(0) -> getName() << ": \n";
							}
							else {
								errs() << " " << getOriginalName(d -> getOperand(0), &F) << ": \n"; 
							}

							if (GlobalValue *gv = dyn_cast<GlobalValue>(d -> getOperand(0))){
								errs() <<"related global value: " << gv ->getName() << "\n";
								continue;
							}
							
							MemoryUse *MU = dyn_cast<MemoryUse>(MSSA -> getMemoryAccess(d));

							MemoryAccess *UO = MU -> getDefiningAccess();
							
							if (MemoryDef *MD = dyn_cast<MemoryDef>(UO)) {
								to_query.push(MD -> getMemoryInst());
							}
							
							else {
								to_query.push(UO);
							}
						}

						else {

							// errs() << "else\n";

							for (int j = 0; j < d -> getNumOperands(); ++ j) {

								Value *v = d -> getOperand(j);
							
								if (Constant *c = dyn_cast<Constant>(v)) {
							
									if (GlobalValue *gv = dyn_cast<GlobalValue>(c)) 
										errs() << "related global value: " << gv -> getName() << "\n";
									else
										errs() << "related constant: " << c -> getUniqueInteger() << "\n";
							
									continue;
							
								}
							
								else if (Instruction *inst = dyn_cast<Instruction>(v)) {
									to_query.push(inst);
								}
							}
						}
					}

					
				}
			}

			return false;
        }


		MDNode* findVar(Value* V, Function* F) {
  			for (auto iter = F -> begin(); iter != F -> end(); ++iter) {
				BasicBlock *bb = &*iter;
				for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++ iter_i){
				Instruction* I = &*iter_i;
				if (DbgDeclareInst* DbgDeclare = dyn_cast<DbgDeclareInst>(I)) {
					if (DbgDeclare->getAddress() == V) return DbgDeclare->getVariable();
				} else if (DbgValueInst* DbgValue = dyn_cast<DbgValueInst>(I)) {
					if (DbgValue->getValue() == V) return DbgValue->getVariable();
				}}
			}
			return NULL;
		}

		StringRef getOriginalName(Value* V, Function* F) {
			MDNode* Var = findVar(V, F);
  			if (!Var) return "UNKNOWN";

  			return dyn_cast<DIVariable>(Var) -> getName();
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
