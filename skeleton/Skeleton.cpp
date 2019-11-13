#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

// tips copy from the doc top webpage
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
using namespace llvm;

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
    errs()<<x<<"\n";
    int e = istwopower(x);
    errs()<<e<<"\n";
    IRBuilder<> builder(bop);
    Value* shift = builder.CreateLShr(v, e);
    for (auto& U: bop->uses()){
        User* user = U.getUser();
        user->setOperand(U.getOperandNo(), shift);
    }
    bop->eraseFromParent();
}

namespace {
  struct SkeletonPass : public FunctionPass {
    static char ID;
    SkeletonPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      errs() << "I saw a function called " << F.getName() << "!\n";
      bool ret = false;
      for (auto &B : F){
          errs() << "a block\n";
          //for (auto &I : B){
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
