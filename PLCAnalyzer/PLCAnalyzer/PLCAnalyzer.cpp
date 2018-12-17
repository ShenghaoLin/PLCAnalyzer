//The pass file for final project of EECS583 in Fall 2018
//@auther: Shenghao Lin, Xumiao Zhang

#include "llvm/IR/GlobalVariable.h"
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
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/LoopInfo.h"

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

        //critical_paths include all paths that is critical inside a function
        map<Function*, map<Value*, vector<vector<pair<Value *, int>>>>> critical_paths;

        //potential_paths contains all paths that is related with the function input,
        //which is pontential to be critical
        map<Function*, map<int, vector<vector<pair<Value *, int>>>>> potential_paths;

        //Values that are determined to be critical
        map<Function*, map<Value*, bool>> critical_values;

        //Global variable analysis
        set<Value *> writable_global_vars;
        map<Value *, Function *> readable_global_vars;
        map<Value *, Function *> global_to_local_vars;

        //A temperory stack used to pass paths among recursions
        vector<Transition> temp_stack;

        PLCAnalyzer() : ModulePass(ID) {}

        bool runOnModule(Module &M) override {
            
            // Find out depenences for each function
            // Fill critical_paths and potential_paths
            for (Module::iterator f = M.begin(); f != M.end(); f ++) {

                Function *F = &(*f);
                string s = F -> getName();
                if (s.find("llvm.dbg") != 0) {
                    functionMemoryDependence(*f);
                }
            }

            errs() << "\n\n----------------------------------------\n";
            errs() << "----------------------------------------\nSummary:\n";

            globalLocalAnalysis(&M);

            printGlobalVarsAnalysis();

            errs() << "\nIntra-cell critical paths:\n";

            // Critical Paths inside a function(cell)
            for (Module::iterator f = M.begin(); f != M.end(); f ++) {

                Function *F = &(*f);
                string s = F -> getName();
                if (s.find("llvm.dbg") != 0) {
                    errs() << "\n" << s << ": \n";

                    for (auto iter = critical_paths[F].begin(); iter != critical_paths[F].end(); ++iter) {
                        
                        Value *v = iter -> first;
                        vector<vector<pair<Value *, int>>> paths = iter -> second;

                        critical_values[F][v] = true;
                        
                        for (auto path : paths) {
                            for (int i = paths.size() - 1; i >= 0; i --) {
                                auto element = path[i];
                                errs() << getOriginalName(element.first, F) << " " << element.second << " -> ";
                            }
                            errs() << getOriginalName(v, F) << "\n";
                        }

                    }
                }
            }

            errs() << "\n\nInter-cell critical paths:\n\n";

            // Critical paths connecting by calls 
            for (Module::iterator f = M.begin(); f != M.end(); f ++) {

                Function *F = &(*f);
                string s = F -> getName();

                if (s.find("llvm.dbg") != 0) {
                    for (auto iter = F -> begin(); iter != F -> end(); ++iter) {
                        BasicBlock* bb = &(*iter);

                        for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++iter_i) {
                            Instruction* inst = &(*iter_i);

                            //Call instructions
                            if (CallInst* call_inst = dyn_cast<CallInst>(inst)) {

                                //All arguments in the call instruction
                                for (int i = 0; i < call_inst -> getNumArgOperands(); ++ i) {
                                   Value *arg = call_inst -> getArgOperand(i);

                                   for (auto path : critical_paths[F][arg]) {

                                       for (int i = path.size() - 1; i >= 0; i --) {
                                           temp_stack.push_back(Transition(F, path[i].first, path[i].second));
                                       }

                                       //Recursive call dataflow dependence
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

            errs() << "\n\nPossible critical control flows:\n";

            //Call control flow analysis for each function
            for (Module::iterator f = M.begin(); f != M.end(); f ++) {

                Function *F = &(*f);
                string s = F -> getName();
                if (s.find("llvm.dbg") != 0) {
                    errs() << "\n" << F -> getName() << ": \n\n";
                    controlFlowDependence(F);
                }
            }

            return false;
        }


        // Print wrapper of global variable analysis
        void printGlobalVarsAnalysis() {
            errs() << "\nGlobal variables usage analysis:";

            // Writable global variables
            if (writable_global_vars.size()) {
                errs() << "\n\nThe following global variables should remain writable:\n";
                for (auto gv : writable_global_vars) {
                    errs() << gv -> getName() << "\n";
                }
            }

            // Readable global variables
            if (readable_global_vars.size()) {
                errs() << "\n\nThe following global variables can be set to readable:\n";
                for (auto gv_pair : readable_global_vars) {

                    if (gv_pair.second == NULL) {
                        errs() << gv_pair.first -> getName() << ": No function needs writing accesibility\n";
                    }

                    else {
                        errs() << gv_pair.first -> getName() << ": Writable by " << gv_pair.second -> getName() << "\n";
                    }
                }
            }

            // Localizable global variables
            if (global_to_local_vars.size()) {
                errs() << "\n\nThe following global variables can be set to local:\n";
                for (auto gv_pair : global_to_local_vars) {

                    if (gv_pair.second == NULL) {
                        errs() << gv_pair.first -> getName() << ": No function needs reading accesibility\n";
                    }
                    else {
                        errs() << gv_pair.first -> getName() << ": Local in " << gv_pair.second -> getName() << "\n";
                    }
                }
            }

            errs() << "\n\n";

        }


        // The analysis of global variable settings
        void globalLocalAnalysis(Module *M) {

            // Set of all global variable "mentioned" in the module
            map<Value *, set<Function *>> used_global_vars, defined_global_vars;

            // Iterate through every instruction
            for (auto f = M -> begin(); f != M -> end(); f ++) {

                Function *F = &(*f);
                string s = F -> getName();
                if (s.find("llvm.dbg") != 0) {

                    for (auto bb = F -> begin(); bb != F -> end(); ++ bb) {

                        BasicBlock *BB = &(*bb);

                        for (auto iter = BB -> begin(); iter != BB -> end(); ++ iter) {
                            Instruction *inst = &(*iter);

                            // Check the use of global var
                            for (int i = 0; i < inst -> getNumOperands(); i ++) {

                                Value *v = inst -> getOperand(i);

                                if (GlobalVariable *gv = dyn_cast<GlobalVariable>(v)) {
                                    used_global_vars[gv].insert(F);
                                }

                            }

                            // check the def of global var
                            if (StoreInst *store_inst = dyn_cast<StoreInst>(inst)) {

                                Value *v = store_inst -> getOperand(1);

                                if (GlobalVariable *gv = dyn_cast<GlobalVariable>(v)) {
                                    defined_global_vars[gv].insert(F);
                                }
                            }
                        }
                    }
                }
            }

            // writable = defined in different cells
            for (auto gv_pair : defined_global_vars) {
                if ((gv_pair.second).size() > 1) {
                    writable_global_vars.insert(gv_pair.first);
                }
            }

            // global = used in different cells
            // readable = global - writable
            for (auto gv_pair : used_global_vars) {
                if (gv_pair.second.size() > 1 && writable_global_vars.count(gv_pair.first) == 0) {

                    if (defined_global_vars[gv_pair.first].size() == 0) {
                        readable_global_vars[gv_pair.first] = NULL;
                    }
                    else {
                        readable_global_vars[gv_pair.first] = (Function *) (*defined_global_vars[gv_pair.first].begin());
                    }
                }
            }

            // localizable = all - global
            for (auto iter = M -> getGlobalList().begin(); iter != M -> getGlobalList().end(); iter ++) {
                GlobalVariable *gv = &(*iter);

                if (used_global_vars[gv].size() == 0) {
                    global_to_local_vars[gv] = NULL;
                }
                else if (used_global_vars[gv].size() == 1) {
                    global_to_local_vars[gv] = (Function *) (*used_global_vars[gv].begin());
                }
            }

        }

        // Function that computes the post dominators for all basic blocks in a function
        // (LLVM library post dominator tree should also work)
        map<BasicBlock *, set<BasicBlock *>> postDominatorsAnalysis(Function *F) {

            map<BasicBlock *, set<BasicBlock *>> ret;
            map<BasicBlock *, int> block_number;

            BasicBlock *exit_block = NULL;

            // Initialization
            for (Function::iterator b = F -> begin(); b != F -> end(); b++) {
                
                BasicBlock *bb = &(*b);

                vector<BasicBlock *> bb_path;
                set<BasicBlock *> post_dom;
                set<BasicBlock *> visited;

                int i = 0;

                for (Function::iterator iter = F -> begin(); iter != F -> end(); iter++) {

                    BasicBlock *bb_temp = &(*iter);
                    ret[bb].insert(bb_temp);

                    block_number[bb_temp] = i ++;
                }

                if (dyn_cast<TerminatorInst>(bb -> getTerminator()) -> getNumSuccessors() == 0){
                    ret[bb] = set<BasicBlock *>();
                    exit_block = bb;
                    ret[bb].insert(bb);
                }
            }

            // The algorithm introduced in our class
            bool change = true;

            while (change) {
                change = false;

                for (Function::iterator b = F -> begin(); b != F -> end(); b++) {
                    
                    BasicBlock *bb = &(*b);

                    if (bb == exit_block) continue;

                    TerminatorInst *term_inst = dyn_cast<TerminatorInst>(bb -> getTerminator());

                    set<BasicBlock *> temp;

                    //set intersection
                    for (Function::iterator iter = F -> begin(); iter != F -> end(); iter++) {

                        temp.insert(&(*iter));
                    }

                    for (int i = 0; i < term_inst -> getNumSuccessors(); ++ i) {
                        BasicBlock *suc_bb = term_inst -> getSuccessor(i);

                        set<BasicBlock *> to_erase;

                        for (auto pdom : temp) {
                            if (ret[suc_bb].count(pdom) == 0) {
                                to_erase.insert(pdom);
                            }
                        }

                        for (auto pdom : to_erase) {
                            temp.erase(pdom);
                        }
                    }

                    temp.insert(bb);

                    // Compare sets
                    if (!set_compare(temp, ret[bb])) {
                        change = true;
                        ret[bb] = temp;
                    }
                }

            }

            return ret;
        }


        // Compare if 2 sets are identical
        bool set_compare(set<BasicBlock *> &a, set<BasicBlock *> &b) {

            for (auto element : b) {
                if (a.count(element) == 0) {
                    return false;
                }
            }
            for (auto element : a) {
                if (b.count(element) == 0) {
                    return false;
                }
            }

            return true;
        }


        //Control flow analysis: try to find out the control flows that are related with
        //critical values, and the values that may be affected by those control flows.
        //i.e. if flag is critical, in the following example, v is affected:
        //if (flag) v ++;
        void controlFlowDependence(Function *F) {

            //map of anti-control-flow (aka preccessors)
            map<BasicBlock *, vector<BasicBlock *>> anti_flow;

            for (Function::iterator b = F -> begin(); b != F -> end(); b++) {
                
                BasicBlock *bb = &(*b);
                TerminatorInst *term_inst = dyn_cast<TerminatorInst>(bb -> getTerminator());

                for (int i = 0; i < term_inst -> getNumSuccessors(); ++i) {
                    anti_flow[term_inst -> getSuccessor(i)].push_back(bb);
                }
            }

            map<BasicBlock *, set<BasicBlock *>> post_dom = postDominatorsAnalysis(F);
            MemorySSA *MSSA = &getAnalysis<MemorySSAWrapperPass>(*F).getMSSA();

            int bb_num = 0;

            //Iterate through all basic blocks
            for (Function::iterator b = F -> begin(); b != F -> end(); b++) {

                BasicBlock *bb = &(*b);

                // the values we are going to find out for the basic block:
                // (1) any value stored in this basic block
                // (2) any related critical values in the contro flow
                set<Value *> effected_vars;
                set<Value *> related_critical_values;
                
                //stack construction
                set<BasicBlock *> visited;
                vector<pair<BasicBlock *, BasicBlock *>> block_stack;
                vector<pair<BasicBlock *, BasicBlock *>> search_stack;

                // keep track of the newest post dominator in the path
                vector<BasicBlock *> post_dominators;

                block_stack.push_back(make_pair<BasicBlock *, BasicBlock *>(NULL, &(*b)));
                post_dominators.push_back(&(*b));

                //DFS using stack
                while (block_stack.size()) {

                    pair<BasicBlock *, BasicBlock *> cur_pair = block_stack.back();
                    block_stack.pop_back();
                    BasicBlock *cur_block = cur_pair.second;

                    // Recursion simulator
                    // Determine the current path
                    while (search_stack.size() && search_stack.back().second != cur_pair.first) {

                        //pop the corresponding post_dominators as well
                        if (post_dominators.size() && 
                            search_stack.back().second == post_dominators.back()) {
                            post_dominators.pop_back();
                        }

                        search_stack.pop_back();
                    }

                    search_stack.push_back(cur_pair);

                    // all preccessors
                    for (auto precessor : anti_flow[cur_block]) {
                        
                        //visited node
                        if (visited.count(precessor) > 0) continue;
                        else visited.insert(precessor);

                        block_stack.push_back(make_pair(cur_block, precessor));

                        BranchInst *br_inst = dyn_cast<BranchInst>(precessor -> getTerminator());

                        //If a basic block is post dominated by our current bb, its branch does not affect the execution of 
                        //current bb, so we will ignore this case.
                        //Otherwise, we should consider if the branch condition is related with any critical values.
                        //And put the new basic block as our next "current" bb
                        if ((br_inst && br_inst -> isConditional()) && post_dom[precessor].count(post_dominators.back()) == 0) {

                            post_dominators.push_back(precessor);

                            if (br_inst -> isConditional()) {
                                set<Value *> temp = criticalValuesFromV(br_inst -> getCondition(), F, MSSA);

                                for (auto v : temp) {
                                    related_critical_values.insert(v);
                                }
                            }
                        }
                    }
                }

                // effected values check
                for (auto iter = bb -> begin(); iter != bb -> end(); ++ iter) {

                    Instruction *inst = &(*iter);
                    
                    if (StoreInst *store_inst = dyn_cast<StoreInst>(inst)) {

                        effected_vars.insert(store_inst -> getOperand(1));
                    }
                }

                //Output generation
                if (related_critical_values.size()&& effected_vars.size()) {
                    errs() << "BasicBlock #" << bb_num << ": \nRelated Critical Values: ";
                    for (auto v : related_critical_values) {
                        if (StoreInst * store_inst = dyn_cast<StoreInst>(v)) {
                            errs() << getOriginalName(store_inst -> getOperand(1), F) << " ";
                        }
                        else {
                            errs() << getOriginalName(v, F) << " ";
                        }
                    }
                    errs() << "\nRelated Variables: ";

                    for (auto v : effected_vars) 
                        errs() << getOriginalName(v, F) << " ";

                    errs() << "\n\n";

                }
                bb_num++;

            }

        }

        //The function used to determine if a value(most time it is a register reference) is
        //related to a critical value.
        set<Value *> criticalValuesFromV(Value *v, Function *F, MemorySSA *MSSA) {

            set<Value *> related_value;

            vector<pair<Value *, Value *>> stack;
            vector<Value *> load_stack;
            vector<pair<Value *, Value *>> search_stack;
            set<Value *> queried;

            stack.push_back(make_pair<Value *, Value *>(NULL, dyn_cast<Value>(v)));

            //Using DFS, similar to what we did to detect critical paths
            //Stop when a load/constant/global variable is detected
            while (stack.size()) {
                Value *pre = stack.back().first;
                Value *cur = stack.back().second;
                stack.pop_back();

                if (queried.count(cur) != 0) continue;
                else queried.insert(cur);

                while (search_stack.size() && search_stack.back().second != pre) {
                    
                    if (dyn_cast<LoadInst>(search_stack.back().second))
                        load_stack.pop_back();

                    search_stack.pop_back();
                }

                search_stack.push_back(make_pair(pre, cur));
                
                // Memory Phi
                if (MemoryPhi *phi = dyn_cast<MemoryPhi>(cur)) {
                    for (auto &op : phi -> operands()) {
                        stack.push_back(make_pair(cur, dyn_cast<Value>(op)));
                    }
                }
                else if (MemoryDef *def = dyn_cast<MemoryDef>(cur)) {

                    // Live on entry
                    if (!(def -> getID())) {
                        continue;
                    }

                    // the correct store related with our target load
                    if (def && def -> getMemoryInst() -> getOperand(1) == load_stack.back()) {

                        if (critical_values[F][def -> getMemoryInst()]) {
                            related_value.insert(def -> getMemoryInst());
                        }   
                    }

                    // MemorySSA does not ensure phi always to point to the correct store instruction
                    // In this case, we should check the reaching definition.
                    else {
                        stack.push_back(make_pair(cur, def -> getDefiningAccess()));
                    }
                }

                else if (LoadInst *load_inst = dyn_cast<LoadInst>(cur)) {

                    //Global variable
                    if (GlobalValue *gv = dyn_cast<GlobalValue>(load_inst -> getOperand(0))){

                        related_value.insert(gv);
                    }    

                    load_stack.push_back(load_inst -> getOperand(0));

                    MemoryUse *MU = dyn_cast<MemoryUse>(MSSA -> getMemoryAccess(load_inst));
                    MemoryAccess *UO = MU -> getDefiningAccess();

                    stack.push_back(make_pair(cur, UO));
                }

                else if (Instruction *inst = dyn_cast<Instruction>(cur)) {
                    for (int j = 0; j < inst -> getNumOperands(); ++ j) {

                        Value *v = inst-> getOperand(j);
                    
                        if (Constant *c = dyn_cast<Constant>(v)) {
                    
                            //global variable
                            if (GlobalValue *gv = dyn_cast<GlobalValue>(c)) {    
                                related_value.insert(gv);
                            }
                            
                            //A real constant otherwise
                            continue;
                        }
                    
                        else if (Instruction *next_inst = dyn_cast<Instruction>(v)) {
                        
                            stack.push_back(make_pair(inst, next_inst));
                        }

                        // Related with a input value 
                        // (should not go to this step if dataflow analysis is properly executed)
                        else {
                            related_value.insert(v);
                        }
                    }
                }
            }

            return related_value;
        }


        // Recursively solve potential path to critical path problem
        // Called when the arg_num-th argument for function F is critical
        void callDependence(Function *F, int arg_num) {

            // All potential paths connected to the critical argument can form
            // new critical paths. 
            for (auto path : potential_paths[F][arg_num]) {

                if (path.front().second == -1) {

                    continue;
                }

                for (auto t : temp_stack) {
                    errs() << t.F -> getName() << ": " << t.k << " " << getOriginalName(t.V, t.F) << " -> ";
                }

                for (int i = path.size() - 1; i >= 0; i --) {
                    if (path[i].second < 0) continue;
                    errs() << F -> getName() << ": " << path[i].second << " " << getOriginalName(path[i].first, F);
                    if (i != 1) errs() << " -> ";
                }

                critical_values[F][path.front().first] = true;

                errs() << "\n";
                
            }


            // A potential path from the critical argument to a function call.
            // Recursively call this function to explore
            for (auto iter = F -> begin(); iter != F -> end(); ++iter) {
                BasicBlock* bb = &(*iter);

                for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++iter_i) {
                    Instruction* inst = &(*iter_i);

                    if (CallInst* call_inst = dyn_cast<CallInst>(inst)) {
                        for (int i = 0; i < call_inst -> getNumArgOperands(); ++ i) {

                            Value *arg = call_inst -> getArgOperand(i);

                            // Iterate all potential paths to find all valid ones
                            for (auto path : potential_paths[F][arg_num]) {

                                // Check if the potential path ends with the call argument
                                if (path.front().first != arg) continue;

                                for (int i = path.size() - 1; i >= 0; i --) {
                                    if (path[i].second < 0) continue;
                                    temp_stack.push_back(Transition(F, path[i].first, path[i].second));
                                }

                                // To the function which is called by the instruction.
                                callDependence(call_inst -> getCalledFunction(), i);

                                for (int i = path.size() - 1; i >= 0; i --) {
                                    if (path[i].second < 0) continue;
                                    temp_stack.pop_back();
                                }
                            }
                        }

                    }
                }
            }                
            
        }


        // The main function used to find out the intra-function dependence
        bool functionMemoryDependence(Function &F) {

            MemorySSA *MSSA = &getAnalysis<MemorySSAWrapperPass>(F).getMSSA();
            std::vector<Value *> store_list;    

            errs() << "\n\n";
            errs().write_escaped(F.getName()) << "\n";

            map<Value *, int> args;

            // Determine arguments and their names
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

                    // Values should be paid attention to: store(direct/critical) and call(inderect/potential)
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

                           }

                           errs() << " ";
                        }

                    }
                }

            }

            bool call_arg_flag = false;

            // Iterate through all notable values that we stored
            for (int i = 0; i < store_list.size(); i ++) {

                // Use pair<Value *, Value *> instead of Value * to retrieve the information 
                // of connection

                call_arg_flag = false;
                set<Value *> queried;
                vector<pair<Value *, Value *>> to_query;
                vector<pair<Value *, Value *>> stack;

                // Specially designed load_stack, used to check the nearest load instruction,
                // helping us to determine if a store is correctly pointed to the value we 
                // are querying
                vector<Value *> load_stack;

                // print the value info
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

                // DFS all the related values with the given value
                while (to_query.size()) {

                    pair<Value *, Value *> tmp = to_query.back();
                    Value *dd = tmp.second;

                    to_query.pop_back();

                    if (queried.find(dd) != queried.end()) continue;
                    queried.insert(dd);

                    // The stack is used to simulate a recursive function,
                    // so that the path info can be retrieved
                    // When a connection is not found, which means a "go back" in a recursion
                    // pop the value in the stack
                    while (stack.size() && stack.back().second != tmp.first) {

                        //also pop the corresponding load
                        if (dyn_cast<LoadInst>(stack.back().second)) {
                            load_stack.pop_back();
                        }

                        stack.pop_back();
                    }

                    stack.push_back(tmp);

                    // Memory phi: add all possible defs to to_query
                    if (MemoryPhi *phi = dyn_cast<MemoryPhi>(dd)) {

                        for (auto &op : phi -> operands()) {
                            to_query.push_back(make_pair<Value *, Value *>(dyn_cast<Value>(dd), dyn_cast<Value>(op)));
                        }
                    }
                    
                    // Memory Def
                    else if (MemoryDef *def = dyn_cast<MemoryDef>(dd)) {

                        // LiveOnEntry
                        if (!(def -> getID())) {
                            continue;
                        }

                        // Correct store related to our load instruction
                        if (def && def -> getMemoryInst() -> getOperand(1) == load_stack.back()) {

                            to_query.push_back(make_pair(dd, def -> getMemoryInst()));   
                        }

                        // Not the correct store to the given load
                        // Check its reaching definition
                        else {
                            to_query.push_back(make_pair(dd, def -> getDefiningAccess()));
                        }
                    }

                    // Normal value
                    else if (Instruction* d = dyn_cast<Instruction>(dd)) {

                        // Normal store instruction
                        if (d -> getOpcode () == Instruction::Store) {
                            
                            MemoryDef *MD = dyn_cast<MemoryDef>(MSSA -> getMemoryAccess(d));
                            Value *v = d -> getOperand(0);
                            
                            // The value to store is of general constant tpye
                            if (Constant* CI = dyn_cast<Constant>(v)) {
                            
                                // Global variable (critical)
                                if (GlobalValue *gv = dyn_cast<GlobalValue>(CI)) {

                                    vector<pair<Value *, int>> critical_path = printPath(stack, &F);

                                    errs() << "related global value: " << gv -> getName() << "\n";

                                    critical_path.push_back(make_pair(v, 0));
                                    critical_paths[&F][dyn_cast<Value>(store_list[i])].push_back(critical_path);
                                }

                                // A real constant
                                else if (CI) {
                                    printPath(stack, &F);

                                    errs() << "related constant: " << CI -> getUniqueInteger() << "\n";
                                }
                            
                                continue;
                            
                            }

                            // to store a register value, which is not a instruction
                            // it implies that it is an input value
                            else if (dyn_cast<Instruction>(v) == NULL) {

                                vector<pair<Value *, int>> potential_path;

                                if (call_arg_flag) {
                                    potential_path.push_back(make_pair(store_list[i], -1));
                                }
                                else {
                                    potential_path.push_back(make_pair(store_list[i], -2));
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
                        
                        // Load Instruction
                        else if (d -> getOpcode() == Instruction::Load) {

                            // Load dierectly from a global variable
                            if (GlobalValue *gv = dyn_cast<GlobalValue>(d -> getOperand(0))){
                                
                                vector<pair<Value *, int>> critical_path = printPath(stack, &F);

                                critical_path.push_back(make_pair(d -> getOperand(0), 0));
                                critical_paths[&F][dyn_cast<Value>(store_list[i])].push_back(critical_path);

                                errs() <<"related global value: " << gv ->getName() << "\n";

                                continue;
                            }    

                            // Otherwise, use MemroySSA to find all its reaching definitions
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

                        // Other instructions. Simply check all operands mentioned in the instruction.
                        else {

                            // Iterate through all operands
                            for (int j = 0; j < d -> getNumOperands(); ++ j) {

                                Value *v = d -> getOperand(j);
                            
                                // A general Constant tpye
                                if (Constant *c = dyn_cast<Constant>(v)) {
                                    
                                    // A global vairable
                                    if (GlobalValue *gv = dyn_cast<GlobalValue>(c)) {
                                        
                                        vector<pair<Value *, int>> critical_path = printPath(stack, &F);

                                        errs() << "related global value: " << gv -> getName() << "\n";

                                        critical_path.push_back(make_pair(v, 0));
                                        critical_paths[&F][dyn_cast<Value>(store_list[i])].push_back(critical_path);

                                    }

                                    // A real constant
                                    else {
                                        
                                        printPath(stack, &F);
                                        errs() << "related constant: " << c -> getUniqueInteger() << "\n";
                                    }
                            
                                    continue;
                            
                                }
                            
                                // A normal instruction
                                else if (Instruction *inst = dyn_cast<Instruction>(v)) {
                                
                                    to_query.push_back(make_pair(dd, inst));
                                    
                                }

                                // A register that is not a instruction: must be an input value.
                                else {
                                    vector<pair<Value *, int>> potential_path;

                                    if (call_arg_flag) {
                                        potential_path.push_back(make_pair(store_list[i], -1));
                                    }
                                    else {
                                        potential_path.push_back(make_pair(store_list[i], -2));
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


        // Print the paths, and return a vector representing the value and its definition number (from Memory SSA) in the path.
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


        //Used by getOriginalName, get the dbg information of the given value
        MDNode* findVar(Value* V, Function* F) {
            for (auto iter = F -> begin(); iter != F -> end(); ++iter) {

                BasicBlock *bb = &(*iter);
                
                for (auto iter_i = bb -> begin(); iter_i != bb -> end(); ++ iter_i){
                
                    Instruction* I = &*iter_i;
                    
                    // llvm.debug.delare
                    if (DbgDeclareInst* DbgDeclare = dyn_cast<DbgDeclareInst>(I)) {
                
                        if (DbgDeclare->getAddress() == V) return DbgDeclare -> getVariable();
                    } 

                    // llvm.debug.value
                    else if (DbgValueInst* DbgValue = dyn_cast<DbgValueInst>(I)) {

                        if (DbgValue->getValue() == V) return DbgValue -> getVariable();
                    }
                }
            }
            return NULL;
        }


        //Find the original name of a variable using debugi information
        StringRef getOriginalName(Value* V, Function* F) {

            // Global var
            if (GlobalValue *gv = dyn_cast<GlobalValue>(V)) return gv -> getName();

            // with name
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
