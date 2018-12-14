//The pass file for final project of EECS583 in Fall 2018
//@auther: Shenghao Lin, Xumiao Zhang

#include "llvm/IR/Module.h"
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

    struct PLCAnalyzer : public ModulePass {
        
        static char ID; 

        PLCAnalyzer() : ModulePass(ID) {}

        bool runOnModule(Module &M) override {
		
			for (Module::iterator f = M.begin(); f != M.end(); f ++) {
                //MemorySSAWrapperPass *MSSA_pass = &getAnalysis<MemorySSAWrapperPass>(*f);
                //MemorySSA *MSSA = &(MSSA_pass -> getMSSA());
				//functionMemoryDependence(*f, MSSA);
                Function *F = &(*f);
                string s = F -> getName();
                if (s.find("llvm.dbg") != 0) {
                    functionMemoryDependence(*f);
                }
			}
			return false;
        }


        bool functionMemoryDependence(Function &F) {

            MemorySSA *MSSA = &getAnalysis<MemorySSAWrapperPass>(F).getMSSA();
			std::vector<Instruction *> store_list;	

			errs() << "\n\n";
			errs().write_escaped(F.getName()) << "\n";

			for (Function::iterator b = F.begin(); b != F.end(); b++) {
				
				BasicBlock *bb = &(*b);
				
				for (BasicBlock::iterator i_iter = bb -> begin(); i_iter != bb -> end(); ++ i_iter) {
					Instruction *I = &(*i_iter);
					if (I -> getOpcode() == Instruction::Store) {
						store_list.push_back(I);						
					}
					if (I -> getOpcode() == Instruction::Call) {
						if (CallInst *call_inst = dyn_cast<CallInst>(I)) {
							if (string("llvm.dbg.declare").compare(call_inst -> getCalledFunction() -> getName()) == 0)
								continue;

						}	
					}
				}
			}

			for (int i = 0; i < store_list.size(); i ++) {
				set<Value *> queried;
				vector<pair<Value *, Value *>> to_query;

				vector<pair<Value *, Value *>> stack;
				
				MemoryDef *MA = dyn_cast<MemoryDef>(MSSA -> getMemoryAccess(store_list[i]));

				if (MA && MA -> getID()){
					errs().write('\n') << MA -> getID();
					if (store_list[i] -> getOperand(1) -> hasName()) {
						errs() << " " << store_list[i] -> getOperand(1) -> getName() << ": \n";
					}
					else {
						errs() << " " << getOriginalName(store_list[i] -> getOperand(1), &F) << ": \n"; 
					}
				}

				to_query.push_back(make_pair<Value *, Value *>(NULL, dyn_cast<Value>(MA)));

				while (to_query.size()) {
					pair<Value *, Value *> tmp = to_query.back();
					Value *dd = tmp.second;

					to_query.pop_back();

					if (queried.find(dd) != queried.end()) continue;
					queried.insert(dd);

					while (stack.size() && stack.back().second != tmp.first) {
						stack.pop_back();
					}

                    stack.push_back(tmp);

					if (MemoryPhi *phi = dyn_cast<MemoryPhi>(dd)) {
						for (auto &op : phi -> operands()) {
							to_query.push_back(make_pair<Value *, Value *>(dyn_cast<Value>(dd), dyn_cast<Value>(op)));
						}

						errs() << "phi\n";
					}
					
					else if (MemoryDef *def = dyn_cast<MemoryDef>(dd)) {

						if (def) {

							to_query.push_back(make_pair(dd, def -> getMemoryInst()));
							
						}
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
							
								if (GlobalValue *gv = dyn_cast<GlobalValue>(CI)) {

									printPath(stack, &F);
									errs() << "related global value: " << gv -> getName() << "\n";
                                    
								}
								else if (CI) {
                                	printPath(stack, &F);

									errs() << "related constant: " << CI -> getUniqueInteger() << "\n";
                                    //errs() << "\n";
								}
							
								continue;
							
							}
							else if (dyn_cast<Instruction>(v) == NULL) {

								printPath(stack, &F);
								
                                errs() << "related input value: " << getOriginalName(d -> getOperand(1), &F) << "\n";

							}
							else {

								to_query.push_back(make_pair(dd, dyn_cast<Instruction>(v)));
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
                                
								printPath(stack, &F);
								errs() <<"related global value: " << gv ->getName() << "\n";

								continue;
							}	
							MemoryUse *MU = dyn_cast<MemoryUse>(MSSA -> getMemoryAccess(d));

							MemoryAccess *UO = MU -> getDefiningAccess();
							
							if (MemoryDef *MD = dyn_cast<MemoryDef>(UO)) {

								to_query.push_back(make_pair(dd, MD));

							}
							
							else {
								to_query.push_back(make_pair(dd, UO));
							}
						}

						else {

							for (int j = 0; j < d -> getNumOperands(); ++ j) {

								Value *v = d -> getOperand(j);
							
								if (Constant *c = dyn_cast<Constant>(v)) {
							
									if (GlobalValue *gv = dyn_cast<GlobalValue>(c)) {
                                		
                                        printPath(stack, &F);

										errs() << "related global value: " << gv -> getName() << "\n";

									}
									else {
                                        
									    printPath(stack, &F);
	                                    errs() << "related constant: " << c -> getUniqueInteger() << "\n";
									}
							
									continue;
							
								}
							
								else if (Instruction *inst = dyn_cast<Instruction>(v)) {
									to_query.push_back(make_pair(dd, inst));
								}
							}
						}
					}

					
				}
			}

			return false;
        }

        void printPath(vector<pair<Value *, Value *>> &v, Function *F) {
        	for (auto tmp : v) {
        		Value *val = tmp.second;
        		if (MemoryDef *MD = dyn_cast<MemoryDef>(val)) {
        			errs() << MD -> getID() << ": " << getOriginalName(MD -> getMemoryInst() -> getOperand(1), F) << " <- ";
        		}
        	}
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
			if (V -> hasName()) return V -> getName();
			MDNode* Var = findVar(V, F);
  			if (!Var) return "UNKNOWN";

  			return dyn_cast<DIVariable>(Var) -> getName();
		}

        void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<MemorySSAWrapperPass>();
        }
    };

}

char PLCAnalyzer::ID = 0;
static RegisterPass<PLCAnalyzer> X("plcanalyzer", "PLCAnalyzer");
