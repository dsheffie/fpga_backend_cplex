#include "systemParam.h"

using namespace std;
using namespace llvm;

const static string opNames [] = 
  {"ALU","MUL","DIV","ADDR","MEM","FADD",
   "FMUL","FDIV","RET","PHI","BRANCH","VMISC",
   "NOAP","DUMMY"};

systemParam::systemParam(bool memIsPipelined)
{
  this->memIsPipelined =
    memIsPipelined;

  dataPath = (fuBase**)NULL;
  fuTable = (fuBase***)NULL;
  for(int i=0; i < (DUMMY-ALU); i++) 
    {
      unitCounts[i] = -1;
      unitLatencies[i] = -1;
    }
}

systemParam::~systemParam() 
{
  if(fuTable)
    {
      for(int i = 0; i <(DUMMY-ALU); i++)
	{
	  delete [] fuTable[i];
	}
	  delete [] fuTable;
    }
}

bool systemParam::is_pipelined(op_t t)
{
  if(memIsPipelined)
    return true;
  else
    return (t == MEM) ? false : true;
}

void systemParam::set(op_t t, int count, int latency) 
{
  unitCounts[t] = count;
  unitLatencies[t] = latency;
  
  if(t == ALU && latency != 1) {
    unitLatencies[t] = 1;
    llvm::errs() << "ALU latency must be 1!\n";
  }
  else if(t == BRANCH && count > 1) {
    unitCounts[t] = 1;
    llvm::errs() << "Only one branch unit supported\n";
  }
  else if(t == RET && count > 1) {
    unitCounts[t] = 1;
      llvm::errs() << "Only one return unit supported\n";
  }
  /*
  else if(t == MEM && count > 1) {
    unitCounts[t] = 1;
    llvm::errs() << "Only one memory port currently supported\n";
  }
  */
}

int systemParam::getNumUnits() 
{
  int amt = 0;
  for(int i = 0; i < (DUMMY-ALU); i++)
    {
      amt += (unitCounts[i] >= 0) ? unitCounts[i] : 0;
    }
  return amt;
}


string systemParam::get_fu_str(op_t t)
{
  return opNames[t];
}

fuBase **systemParam::getDataPathObjs(int &amt,	     
				      map<int, list<Value* > > *statePlan,
				      map<Value*, int> *regMap  
				      )
{
  fuBase **fb;
  amt = 0;
  
  for(int i = 0; i < (DUMMY-ALU); i++) {
    for(int j = 0; j < unitCounts[i]; j++) {
      amt++;
    }
  }

  assert(amt == getNumUnits());
  
  fb = new fuBase*[amt];
  this->dataPath = fb;
  int c=0;
  fuTable = new fuBase**[(DUMMY-ALU)];
  for(int i = 0; i < (DUMMY-ALU); i++) 
    {
      int n = unitCounts[i];
      if(n < 0) {
	fuTable[i] = (fuBase**)NULL;
      } else {
	fuTable[i] = new fuBase*[n];
	for(int j = 0; j < n; j++) {
	  fuBase *b = NULL;
	  memFU *m = NULL;
	  switch(((op_t)i))
	    {
	    case ALU:
	      b = new aluFU(this, c, ALU ,statePlan, regMap);
	      break;
	    case MUL:
	      b = new imulFU(this, c, MUL, statePlan, regMap);
	      break;
	    case RET:
	      b = new retFU(this, c, RET, statePlan, regMap);
	      break;
	    case PHI:
	      b = new phiFU(this, c, PHI, statePlan, regMap);
	      break;
	    case BRANCH:
	      b = new branchFU(this, c, BRANCH, statePlan, regMap);
	      break;
	    case ADDR:
	      b = new addrFU(this,c,ADDR,statePlan,regMap);
	      break;
	    case MEM:
	      m = new memFU(this,c,MEM,statePlan,regMap);
	      m->setMemPortId(j);
	      b = m;
	      break;
	    case FADD:
	      b = new faddFU(this,c,FADD,statePlan,regMap);
	      break;
	    case FMUL:
	      b = new fmulFU(this,c,FMUL,statePlan,regMap);
	      break;
	    default:
	      b = new fuBase(this, c, ((op_t)i) ,statePlan, regMap);
	      break;
	    }
	  fb[c] = b;
	  fuTable[i][j] = b;
	  c++;
	}
      }
    }

  assert(c ==  amt);

  return fb;
}


op_t systemParam::get_type(llvm::Value *v) 
{
  llvm::Instruction *ins = llvm::dyn_cast<llvm::Instruction>(v);
  assert(ins);
  switch(ins->getOpcode()) {
  case llvm::Instruction::Add:
    return ALU;
    break;
  case llvm::Instruction::Sub:
    return ALU;
    break;
  case llvm::Instruction::Or:
    return ALU;
      break;
  case llvm::Instruction::And:
    return ALU;
    break;
  case llvm::Instruction::Xor:
    return ALU;
    break;
  case llvm::Instruction::ICmp:
    return ALU;
    break;
  case llvm::Instruction::LShr:
    return ALU;
    break;
    
  case llvm::Instruction::Mul:
    return MUL;
    break;
  case llvm::Instruction::Load:
      return MEM;
      break;
  case llvm::Instruction::Store:
    return MEM;
    break;
  case llvm::Instruction::FAdd:
    return FADD;
    break;
 case llvm::Instruction::FSub:
   return FADD;
    break;
  case llvm::Instruction::FMul:
    return FMUL;
    break;
  case llvm::Instruction::Ret:
    return RET;
    break;
  case llvm::Instruction::PHI:
    return PHI;
    break;
  case llvm::Instruction::Br:
    return BRANCH;
    break;
  case llvm::Instruction::GetElementPtr:
    return ADDR;
    break;
  case llvm::Instruction::InsertElement:
    return VMISC;
    break;
  case llvm::Instruction::ZExt:
    return ALU;
    break;
  case llvm::Instruction::SExt:
    return ALU;
    break;
  case llvm::Instruction::Trunc:
    return ALU;
    break;
  case llvm::Instruction::BitCast:
    return ALU;
    break;
  default:
    llvm::errs() << "systemParam::get_type() " << *ins << "\n";
    llvm::errs() << "ins->getOpcode() = "<< ins->getOpcode() << "\n";
    exit(-1);
    return DUMMY;
    break;
  }
}

std::string systemParam::get_operator(Instruction *ins)
{  
  CmpInst *Ce = dyn_cast<CmpInst>(ins); 

  switch(ins->getOpcode()) {
  case llvm::Instruction::Add:
    return string("+");
    break;
  case llvm::Instruction::Sub:
    return string("-");
    break;
  case llvm::Instruction::Or:
    return string("|");
    break;
  case llvm::Instruction::And:
    return string("&");
    break;
  case llvm::Instruction::Xor:
    return string("^");
    break;

    /* TODO: Fix these because it
     * doesn't really belong here */
  case llvm::Instruction::ZExt:
    return string("");
    break;
  case llvm::Instruction::SExt:
    return string("");
    break;
  case Instruction::Trunc:
    return string("");
    break;


  case llvm::Instruction::ICmp:
    switch(Ce->getPredicate())
      { 
      case CmpInst::ICMP_EQ:
	return string(" == ");
	break;
      case CmpInst::ICMP_NE:
	return string(" != ");
	break;
      case CmpInst::ICMP_ULT:
	return string(" < ");
	break;
      case CmpInst::ICMP_ULE:
	return string(" <= ");
	break;
      case CmpInst::ICMP_UGT:
	return string(" > ");
	break;
      case CmpInst::ICMP_UGE:
	return string(" >= ");
	break;
      case CmpInst::ICMP_SGT:
	return string(" > ");
	break;
      case CmpInst::ICMP_SLT:
	return string(" < ");
	break;
      default:
	errs() << *Ce << "\n";
	errs() << "I don't know about this predicate \n";
	exit(-1);
	break;
      }
    break;
  case llvm::Instruction::LShr:
    return string("<<");
    break;
  default:
    errs() << "unknown operation for an ALU\n";
    return string("");
    break;
  }

}
