#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"

#include <iostream>
#include <string>
#include <vector>

// tips copy from the doc top webpage
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
using namespace llvm;

// costs is vector of costs of +/-, *, /, <</>>
std::vector<unsigned int> costs = {3,5,10,1};
int mul_reduction(unsigned int factor) 
{
    std::vector <std::pair <std::string, int> > result;

    unsigned int cost_plus = 0;
    unsigned int cost_minus = 0;

    std::vector <int> plus, minus;
    unsigned int len = 0;
    for (int k = 0, f = factor; f; k ++, f >>= 1) {
        if (f & 1) {
            plus.push_back(k);
            if (k == 0) cost_plus -= costs[3];
        }
        len ++;
    }

    for (int k = 0, f = (1 << len) - factor; f; k ++, f >>= 1) {
        if (f & 1) {
            minus.push_back(k);
            if (k == 0) cost_minus -= costs[3];
        }
    }

    cost_plus += plus.size() * (costs[0] + costs[3]) - costs[0];
    cost_minus += minus.size() * (costs[0] + costs[3]) + costs[3];

    /*
    std::cerr << cost_plus << std::endl;
        std::cerr << "<< " << plus[0] << std::endl;
    for (int i = 1; i < plus.size(); i ++) 
        std::cerr << "+ << " << plus[i] << std::endl;
    
    std::cerr << cost_minus << std::endl;
    std::cerr << "<< " << len << std::endl;
    for (int i = 0; i < minus.size(); i ++) 
        std::cerr << "- << " << minus[i] << std::endl;
    */

    if (cost_plus <= cost_minus) {
        result.push_back(std::make_pair("cost", cost_plus));
        result.push_back(std::make_pair("=", plus[0]));
        for (int i = 1; i < plus.size(); i ++)
            result.push_back(std::make_pair("+", plus[i]));
    }
    else {
        result.push_back(std::make_pair("cost", cost_minus));
        result.push_back(std::make_pair("=", len));
        for (int i = 0; i < plus.size(); i ++)
            result.push_back(std::make_pair("-", plus[i]));
    }

    int ret = costs[1] - result[0].second;
    return (ret > 0)? ret : 0;
}

int istwopower(int x){
    if(x>0 &&((x&(x-1))==0)){
        int i=0;
        while(!(x&1)){
            ++i;
            x>>=1;
        }
        return i;
    }
    return -1;
}

void strengthReduction(BinaryOperator *bop, Constant *c, Value *v){
    int x = c->getUniqueInteger().getLimitedValue();
    int savings = mul_reduction(x);

    errs()<<x<<"\n";
    bool neg=false;
    if(x<0){
        neg = true;
        x = -x;
    }
    int e = istwopower(x);
    errs()<<e<<"\n";
    IRBuilder<> builder(bop);
    Value *shift = builder.CreateShl(v, e);
    if(neg)
        shift = builder.CreateNeg(shift);
    for (auto& U: bop->uses()){
        User* user = U.getUser();
        user->setOperand(U.getOperandNo(), shift);
    }
    bop->eraseFromParent();
}

namespace {
    // https://github.com/thomaslee/llvm-demo/blob/master/main.cc
    static Function* printf_prototype(LLVMContext& ctx, Module *mod) {
        std::vector<Type*> printf_arg_types;
        printf_arg_types.push_back(Type::getInt8PtrTy(ctx));

        FunctionType* printf_type = FunctionType::get(Type::getInt32Ty(ctx), printf_arg_types, true);
        Function *func = mod->getFunction("printf");
        if(!func)
            func = Function::Create(printf_type, Function::ExternalLinkage, Twine("printf"), mod);
        func->setCallingConv(CallingConv::C);
        return func;
    }
    struct SkeletonPass : public FunctionPass {
        static char ID;
        LLVMContext *Context;
        GlobalVariable *bbCounter = NULL;
        GlobalVariable *BasicBlockPrintfFormatStr = NULL;
        Function *printf_func = NULL;
        SkeletonPass() : FunctionPass(ID) {}
        void addFinalPrintf(BasicBlock& BB, LLVMContext *Context, GlobalVariable *bbCounter, GlobalVariable *var, Function *printf_func) {
            IRBuilder<> builder(BB.getTerminator()); // Insert BEFORE the final statement
            std::vector<Constant*> indices;
            Constant *zero = Constant::getNullValue(IntegerType::getInt32Ty(*Context));
            indices.push_back(zero);
            indices.push_back(zero);
            Constant *var_ref = ConstantExpr::getGetElementPtr(var->getType(), var, indices);

            Value *bbc = builder.CreateLoad(bbCounter);
            std::vector<Value *> print_args;
            print_args.push_back(var_ref);
            print_args.push_back(bbc);
            CallInst *call = builder.CreateCall(printf_func, print_args);
            call->setTailCall(false);
            
        }
        bool doInitialization(Module &M) {
            errs() << "\n---------Starting BasicBlockDemo---------\n";
            Context = &M.getContext();
            bbCounter = new GlobalVariable(M, Type::getInt32Ty(*Context), false, GlobalValue::InternalLinkage, ConstantInt::get(Type::getInt32Ty(*Context), 0), "bbCounter");
            const char *finalPrintString = "BB Count: %d\n";
            Constant *format_const = ConstantDataArray::getString(*Context, finalPrintString);
            BasicBlockPrintfFormatStr = new GlobalVariable(M, llvm::ArrayType::get(llvm::IntegerType::get(*Context, 8), strlen(finalPrintString)+1), true, llvm::GlobalValue::PrivateLinkage, format_const, "BasicBlockPrintfFormatStr");
            printf_func = printf_prototype(*Context, &M);

            errs() << "Module: " << M.getName() << "\n";

            return true;
        }

        virtual bool runOnFunction(Function &F) {
            errs() << "I saw a function called " << F.getName() << "!\n";
            bool ret = false;
            for (auto &B : F){
                errs() << "a block\n";
                if(F.getName().equals("main") && isa<ReturnInst>(B.getTerminator())) { // major hack?
                    addFinalPrintf(B, Context, bbCounter, BasicBlockPrintfFormatStr, printf_func);
                }
                for (BasicBlock::iterator DI = B.begin(); DI != B.end(); ) {
                    Instruction *I = &*DI++;
                    errs() << "an instruction: " << *I << "\n";
                    BinaryOperator *bop = dyn_cast<BinaryOperator>(I);
                    if(bop){
                        errs() << "it's binary operator\n";
                        if (bop->getOpcode() == Instruction::Mul){
                            errs() << "it's mul operator\n";

                            Value *lhs = bop->getOperand(0);
                            Value *rhs = bop->getOperand(1);
                            errs() << *lhs << "\n";
                            errs() << *rhs << "\n";

                            Constant *C;
                            C=dyn_cast<Constant>(rhs);
                            if(C){
                                errs()<<"rhs is constant\n";
                                strengthReduction(bop, C, lhs);
                                ret = true;
                            }
                            C=dyn_cast<Constant>(lhs);
                            if(C){
                                errs()<<"lhs is constant\n";
                                strengthReduction(bop, C, rhs);
                                ret = true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    };
}

char SkeletonPass::ID = 0;

// Register the pass so `opt -skeleton` runs it.
static RegisterPass<SkeletonPass> X("skeleton", "a useless pass");
