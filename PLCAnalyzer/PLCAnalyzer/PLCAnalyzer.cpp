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

struct Transition{
	Function *F;
	Value *V;
	int k;

	Transition(Function *_F, Value *_V, int _k) : F(_F), V(_V), k(_k) {}
};

namespace {

    struct PLCAnalyzer : public ModulePass {
        
        static char ID;
        map<Function*, map<Value*, vector<vector<pair<Value *, int>>>>> critical_paths;
        map<Function*, map<int, vector<vector<pair<Value *, int>>>>> potential_paths;
        map<Function*, map<Value*, bool>> critical_values;

        vector<Transition> temp_stack;

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

                for (auto iter = critical_paths[F].begin(); iter != critical_paths[F].end(); ++iter) {
                	errs() << "\n";
                	Value *v = iter -> first;
                	vector<vector<pair<Value *, int>>> paths = iter -> second;
                	for (auto path : paths) {
                		errs() << v << ": ";
                		for (auto element : path) {
                			errs() << " <- " << getOriginalName(element.first, F) << " " << element.second;
                		}
                	}
                }
            }

            errs() << "\n";

            for (Module::iterator f = M.begin(); f != M.end(); f ++) {
            	Function *F = &(*f);
                string s = F -> getName();

                if (s.find("llvm.dbg") != 0) {
                	for (auto iter = F -> begin(); iter != F -> end(); ++iter) {
		        		BasicBlock* bb = &(*iter);

		        		for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++iter_i) {
		        			Instruction* inst = &(*iter_i);

		        			if (CallInst* call_inst = dyn_cast<CallInst>(inst)) {
		        				for (int i = 0; i < call_inst -> getNumArgOperands(); ++ i) {
	                           		Value *arg = call_inst -> getArgOperand(i);

	                           		for (auto path : critical_paths[F][arg]) {

	                           			for (int i = path.size() - 1; i >= 0; i --) {
	                           				temp_stack.push_back(Transition(F, path[i].first, path[i].second));
	                           			}

	                           			callDependence(call_inst -> getCalledFunction(), i);

	                           			for (int i = path.size() - 1; i >= 0; i --) {
	                           				temp_stack.pop_back();
	                           			}
	                           		}
	                           	}

		        			}
		        		}
		        	}
                }
            }

            return false;
        }

        void controlFlowDependence(Function *F) {

            map<BasicBlock *, vector<BasicBlock *>> anti_flow;

            for (Function::iterator b = F -> begin(); b != F -> end(); b++) {
                
                BasicBlock *bb = &(*b);
            
                
            }
        }

        void callDependence(Function *F, int arg_num) {

            // map<Value *, vector<Value*, int>> rec_paths;

        	for (auto path : potential_paths[F][arg_num]) {

                if (path.front().second == -1) {

                    continue;
                }

    			for (auto t : temp_stack) {
    				errs() << t.F -> getName() << ": " << t.k << " " << getOriginalName(t.V, t.F) << " -> ";
    			}

    			for (int i = path.size() - 1; i >= 0; i --) {
                    if (path[i].second == -1) continue;
    				errs() << F -> getName() << ": " << path[i].second << " " << getOriginalName(path[i].first, F);
    				if (i != 0) errs() << " -> ";
    			}
    			errs() << "\n";
        		
        	}


            for (auto iter = F -> begin(); iter != F -> end(); ++iter) {
                BasicBlock* bb = &(*iter);

                for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++iter_i) {
                    Instruction* inst = &(*iter_i);

                    if (CallInst* call_inst = dyn_cast<CallInst>(inst)) {
                        for (int i = 0; i < call_inst -> getNumArgOperands(); ++ i) {

                            Value *arg = call_inst -> getArgOperand(i);
                            for (auto path : potential_paths[F][arg_num]) {

                                if (path.front().first != arg) continue;

                                for (int i = path.size() - 1; i >= 0; i --) {
                                    if (path[i].second == -1) continue;
                                    temp_stack.push_back(Transition(F, path[i].first, path[i].second));
                                }
                                // errs() << "at least I'm here" << call_inst -> getCalledFunction() -> getName() << "\n";
                                callDependence(call_inst -> getCalledFunction(), i);

                                for (int i = path.size() - 1; i >= 0; i --) {
                                    if (path[i].second == -1) continue;
                                    temp_stack.pop_back();
                                }
                            }
                        }

                    }
                }
            }                
            
        }


        bool functionMemoryDependence(Function &F) {

            MemorySSA *MSSA = &getAnalysis<MemorySSAWrapperPass>(F).getMSSA();
            std::vector<Value *> store_list;    

            errs() << "\n\n";
            errs().write_escaped(F.getName()) << "\n";

            map<Value *, int> args;

            int num = 0;
            for (auto iter = F.arg_begin(); iter != F.arg_end(); ++iter) {
            	Argument *arg = *(&iter);
            	errs() << getOriginalName(arg, &F) << " " << arg -> getName() << " " ;
                args[arg] = num ++;
            }
            errs() << "\n";

            map<Function *, vector<Value *>> arguments;
            

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

                            errs() << call_inst -> getCalledFunction() -> getName() << "\n";

                            arguments[call_inst -> getCalledFunction()] = vector<Value *>();

                           	for (int i = 0; i < call_inst -> getNumArgOperands(); ++ i) {
                           		Value *arg = call_inst -> getArgOperand(i);
                           		arguments[call_inst -> getCalledFunction()].push_back(arg);

                           		store_list.push_back(arg);
                           	    //errs() << getOriginalName(arg, &F) << " ";
                           	}

                           	errs() << " ";

                        }

                    }
                }


            }

            bool call_arg_flag = false;

            for (int i = 0; i < store_list.size(); i ++) {
                call_arg_flag = false;
                set<Value *> queried;
                vector<pair<Value *, Value *>> to_query;
                vector<Value *> load_stack;
                vector<pair<Value *, Value *>> stack;

                if (StoreInst * store_inst = dyn_cast<StoreInst>(store_list[i])) {

                    load_stack.push_back(store_inst -> getOperand(1));
                
	                MemoryDef *MA = dyn_cast<MemoryDef>(MSSA -> getMemoryAccess(store_inst));

	                if (MA && MA -> getID()){
	                    errs().write('\n') << MA -> getID();
	                    if (store_inst -> getOperand(1) -> hasName()) {
	                        errs() << " " << store_inst -> getOperand(1) -> getName() << ": \n";
	                    }
	                    else {
	                        errs() << " " << getOriginalName(store_inst -> getOperand(1), &F) << ": \n"; 
	                    }
	                }

	                to_query.push_back(make_pair<Value *, Value *>(NULL, dyn_cast<Value>(MA)));
	            }

	            else {
                    call_arg_flag = true;

	            	errs().write('\n') << "call argument " << getOriginalName(store_list[i], &F) << ":\n";

	            	to_query.push_back(make_pair<Value *, Value *>(NULL, dyn_cast<Value>(store_list[i])));
	            }

                while (to_query.size()) {

                    pair<Value *, Value *> tmp = to_query.back();
                    Value *dd = tmp.second;

                    to_query.pop_back();

                    if (queried.find(dd) != queried.end()) continue;
                    queried.insert(dd);

                    while (stack.size() && stack.back().second != tmp.first) {
                        if (dyn_cast<LoadInst>(stack.back().second)) {
                            load_stack.pop_back();
                        }
                        stack.pop_back();
                    }

                    stack.push_back(tmp);

                    if (MemoryPhi *phi = dyn_cast<MemoryPhi>(dd)) {
                        for (auto &op : phi -> operands()) {
                            to_query.push_back(make_pair<Value *, Value *>(dyn_cast<Value>(dd), dyn_cast<Value>(op)));
                        }

                        // errs() << "phi\n";
                    }
                    
                    else if (MemoryDef *def = dyn_cast<MemoryDef>(dd)) {

                        if (!(def -> getID())) {
                            continue;
                        }

                        if (def && def -> getMemoryInst() -> getOperand(1) == load_stack.back()) {

                            to_query.push_back(make_pair(dd, def -> getMemoryInst()));   
                        }
                        else {
                            to_query.push_back(make_pair(dd, def -> getDefiningAccess()));
                        }
                    }

                    else if (Instruction* d = dyn_cast<Instruction>(dd)) {

                        if (d -> getOpcode () == Instruction::Store) {
                            
                            MemoryDef *MD = dyn_cast<MemoryDef>(MSSA -> getMemoryAccess(d));
                            // errs() << "store " << MD -> getID();

                            // if (d -> getOperand(1) -> hasName()) {
                            //     errs() << " " << d -> getOperand(1) -> getName() << "\n";
                            // }
                            // else {
                            //     errs() << " " << getOriginalName(d -> getOperand(1), &F) << "\n";
                            // }

                            Value *v = d -> getOperand(0);
                            
                            if (Constant* CI = dyn_cast<Constant>(v)) {
                            
                                if (GlobalValue *gv = dyn_cast<GlobalValue>(CI)) {

                                    vector<pair<Value *, int>> critical_path = printPath(stack, &F);
                                    errs() << "related global value: " << gv -> getName() << "\n";
                                    critical_path.push_back(make_pair(v, 0));
                                    critical_paths[&F][store_list[i]].push_back(critical_path);
                                }
                                else if (CI) {
                                    printPath(stack, &F);

                                    errs() << "related constant: " << CI -> getUniqueInteger() << "\n";
                                    //errs() << "\n";
                                }
                            
                                continue;
                            
                            }
                            else if (dyn_cast<Instruction>(v) == NULL) {

                                vector<pair<Value *, int>> potential_path;

                                if (call_arg_flag) {
                                    potential_path.push_back(make_pair(store_list[i], -1));
                                }

                                vector<pair<Value *, int>> tmp_vect = printPath(stack, &F);

                                for (auto d : tmp_vect) {
                                    potential_path.push_back(d);
                                }

                                errs() << "related input value: " << getOriginalName(d -> getOperand(1), &F);

                                if (args.count(v) != 0) {
                                    errs() << " argument #" << args[v] << "\n";
                                }

                                potential_paths[&F][args[v]].push_back(potential_path);
                            }
                            else {

                                to_query.push_back(make_pair(dd, dyn_cast<Instruction>(v)));
                            }
                        }
                        

                        else if (d -> getOpcode() == Instruction::Load) {
                            
                            // errs() << "load";
                            // if (d -> getOperand(0) -> hasName()) {
                            //     errs() << " " << d -> getOperand(0) -> getName() << ": \n";
                            // }
                            // else {
                            //     errs() << " " << getOriginalName(d -> getOperand(0), &F) << ": \n"; 
                            // }

                            if (GlobalValue *gv = dyn_cast<GlobalValue>(d -> getOperand(0))){
                                
                                vector<pair<Value *, int>> critical_path = printPath(stack, &F);

                                critical_path.push_back(make_pair(d -> getOperand(0), 0));
                                critical_paths[&F][store_list[i]].push_back(critical_path);

                                errs() <<"related global value: " << gv ->getName() << "\n";

                                continue;
                            }    

                            load_stack.push_back(dyn_cast<LoadInst>(d) -> getOperand(0));

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
                                        
                                        vector<pair<Value *, int>> critical_path = printPath(stack, &F);

                                        errs() << "related global value: " << gv -> getName() << "\n";

                                        critical_path.push_back(make_pair(v, 0));
                                        critical_paths[&F][store_list[i]].push_back(critical_path);

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
                                else {
                                    vector<pair<Value *, int>> potential_path;

                                    if (call_arg_flag) {
                                        potential_path.push_back(make_pair(store_list[i], -1));
                                    }

                                    vector<pair<Value *, int>> tmp_vect = printPath(stack, &F);

                                    for (auto d : tmp_vect) {
                                        potential_path.push_back(d);
                                    }

                                	errs() << "related input value: " << getOriginalName(v, &F);

	                                if (args.count(v) != 0) {
	                                    errs() << " argument #" << args[v] << "\n";
	                                }

	                                potential_paths[&F][args[v]].push_back(potential_path);
                                }
                            }
                        }
                    }

                    
                }
            }

            return false;
        }

        vector<pair<Value *, int>> printPath(vector<pair<Value *, Value *>> &v, Function *F) {
        	vector<pair<Value *, int>> ret;
            for (auto tmp : v) {
                Value *val = tmp.second;
                if (MemoryDef *MD = dyn_cast<MemoryDef>(val)) {
                    errs() << MD -> getID() << ": " << getOriginalName(MD -> getMemoryInst() -> getOperand(1), F) << " <- ";

                    ret.push_back(make_pair(MD -> getMemoryInst() -> getOperand(1), MD -> getID()));
                }
            }

            return ret;
        }


        MDNode* findVar(Value* V, Function* F) {
              for (auto iter = F -> begin(); iter != F -> end(); ++iter) {
                BasicBlock *bb = &*iter;
                for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++ iter_i){
                    Instruction* I = &*iter_i;
                    if (DbgDeclareInst* DbgDeclare = dyn_cast<DbgDeclareInst>(I)) {
                        if (DbgDeclare->getAddress() == V) return DbgDeclare -> getVariable();
                    } 
                    else if (DbgValueInst* DbgValue = dyn_cast<DbgValueInst>(I)) {
                        if (DbgValue->getValue() == V) return DbgValue -> getVariable();
                    }
                }
            }
            return NULL;
        }

        StringRef getOriginalName(Value* V, Function* F) {
        	if (GlobalValue *gv = dyn_cast<GlobalValue>(V)) return gv -> getName();
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
