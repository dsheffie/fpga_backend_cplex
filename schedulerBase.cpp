#include "schedulerBase.h"

using namespace std;
using namespace llvm;

bool schedulerBase::checkIfMemOpsAlias(Instruction *insA, Instruction *insB)
{
  //Check if both loads....no aliasin'
  if( (insA->getOpcode() == Instruction::Load) &&
      (insB->getOpcode() == Instruction::Load) )
    {
      return false;
    }

  GEPOperator *ptrA = getPointerOperand(insA);
  GEPOperator *ptrB = getPointerOperand(insB);
  

  if(ptrA==0 ) {
    errs() << "passed null ptr A into checkIfMemOpsAlias\n";
    errs() << *insA << "\n";
    exit(-1);
  }

  if(ptrB == 0) {
    errs() << "passed null ptr B into checkIfMemOpsAlias\n";
    errs() << *insB << "\n";
    exit(-1);
  }

  AliasAnalysis::AliasResult AR = AA->alias(ptrA,ptrB);
  switch(AR)
    {
    case AliasAnalysis::NoAlias:
      return false;
      //errs() << "NoAlias\n";
      break;
    case AliasAnalysis::MayAlias:
      //errs() << "MayAlias\n";
      return true;
      break;
    case AliasAnalysis::MustAlias:
      //errs() << "MustAlias\n";
      return true;
      break;
    }

  return true;
}

GEPOperator* schedulerBase::getPointerOperand(Instruction *ins)
{
  Value *ptrOperand;
  Instruction *ptrIns;
  // const Type *memType = ins->getType();
  //if(memType->isVectorTy())
  //{
  //  errs() << *ins << " is a vector type\n";
  // }

  if(ins->getOpcode() == Instruction::Load)
    {
      LoadInst *lIns = dyn_cast<LoadInst>(ins);
      ptrOperand = lIns->getPointerOperand();
    }
  else if(ins->getOpcode() == Instruction::Store)
    {
      StoreInst *sIns = dyn_cast<StoreInst>(ins);
      ptrOperand = sIns->getPointerOperand();
    
    }
  else
    {
      errs() << "non memory operation passed into getPointerOperand()\n";
      exit(-1);
      return 0;
    }

  ptrIns = dyn_cast<Instruction>(ptrOperand);
  if(ptrIns==0) {
    errs() << "ptrIns == 0 in getPointerOperand\n";
    exit(-1);
  }
  
  if(ptrIns->getOpcode() == Instruction::BitCast) {
    ptrOperand = ptrIns->getOperand(0);
  }
    
  //errs() << "operand: " << *ptrOperand << "\n";
  return dyn_cast<llvm::GEPOperator>(ptrOperand);
  
}
