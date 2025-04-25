
#include "dataPathBuilder.h"
#include "util.h"

using namespace std;
using namespace llvm;


constantPool::constantPool()
{

}

constantPool::~constantPool()
{
  for(list<scalarImmediate*>::iterator lit = allocImm.begin();
      lit != allocImm.end(); lit++)
    {
      scalarImmediate *r = *lit;
      delete r;
    }
}

scalarImmediate* constantPool::get(int val)
{
  if(intPool.find(val) != intPool.end())
    return intPool[val];
  else
    {
      scalarImmediate *s = new scalarImmediate(val);
      allocImm.push_back(s);
      intPool[val] = s;
      return s;
    }
}

scalarImmediate* constantPool::get(float val)
{
  scalarImmediate *s = new scalarImmediate(val);
  allocImm.push_back(s);
  return s;
}

fuBase::fuBase(systemParam *sP,	 
	       int id,
	       op_t fuType,
	       std::map<int, std::list<llvm::Value* > > *statePlan,
	       std::map<llvm::Value*, int> *regMap)
{
  this->sP=sP;
  this->id = id;
  this->fuType = fuType;
  this->statePlan=statePlan;
  this->regMap = regMap;
  fuLatency = sP->get_latency(fuType);
  fuName = sP->get_fu_str(fuType) + string("_id_") + int2string(id);
}

fuBase::~fuBase()
{

}

std::string fuBase::emitVerilogFwdDef()
{
  string s = string("");
  if(operationMap.size()!=0)
    {
      s = string("wire [31:0] ") + getName() + string(";\n");
    }
  return s;
}

void fuBase::elaborate(std::map<int, scalarRegister*> &sRegMap,
		       constantPool *cPool,
		       controlFSM *cFSM,
		       std::map<Value*, scalarArgument*> &sArgMap)
{
  if(operationMap.size() != 0)
    {
      errs() << "fu ( " << fuName << " )fires in " << 
	operationMap.size() << " states\n";
    }

  /* iterate over all states were operand fires */
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      /* Remember, not all instructions write
       * the register file */

      errs() << *ins << "\n";

      /* iterate over the operands this instruction uses */
      for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); 
	  op != ope; op++)
	{
	  Value *oV = *op;
	  Instruction *oIns = dyn_cast<Instruction>(oV);
	  if(oIns) {
	    errs() << "\t\treads register " << (*regMap)[oV] << " @ " << startState << "\n";
	  }
	}

      if(regMap->find(V) != regMap->end())
	{
	  rd = (*regMap)[V];
	  scalarRegister *sRD = sRegMap[rd];
	  assert(sRD);

	  /* one cycle before results are
	   * available (<= in Verilog) */
	  int writeState = (doneState-1);
	  assert(writeState >= startState);

	  sRD->addWriter(this, writeState);


	  errs() << "\t\tresults available in register " << rd << " @  " << doneState << "\n";
	} 
    }
}

void fuBase::addOperation(Value* V, int state)
{
  operationMap[state] = V;
}


string fuBase::asVerilog()
{
  string s;
  if(operationMap.size() == 0)
    return string("");

  s += string("/* WARNING: START DEFAULT RTL GENERATED */ \n");
  s += string("assign ") + getName() + string(" = 32'd0;\n\n");
  s += string("/* WARNING: END DEFAULT RTL GENERATED */ \n");

  errs() << "base class called for type: "  << sP->get_fu_str(fuType) << "\n";
 
  fuVerilog = s;
  return s;
}

static scalarValue* findScalarRef(Value *V, 
				  constantPool *cPool,
				  map<Value*, int> *regMap,
				  map<int, scalarRegister*> &sRegMap,
				  map<Value*, scalarArgument*> &sArgMap)
{
  Instruction *ins = dyn_cast<Instruction>(V);
  Argument *arg = dyn_cast<Argument>(V);
  ConstantInt* cint = dyn_cast<ConstantInt>(V); 
  ConstantFP *fval = dyn_cast<ConstantFP>(V);
  if(ins)
    {
      assert(regMap->find(V) != regMap->end());
      int rs = (*regMap)[V];
      return sRegMap[rs];
    }
  else if(arg)
    {
      if(sArgMap.find(V) == sArgMap.end())
	return NULL;
      else
	return sArgMap[V];
    }
  else if(cint)
    {
      int ival = cint->getZExtValue();
      return cPool->get(ival);
    } 
  else if(fval)
    {
      APFloat afloat = fval->getValueAPF();
      float f = afloat.convertToFloat();
      return cPool->get(f);
    }
  else
    {
      errs() << "Immediate WTF in findScalarRef\n";
      exit(-1);
      return NULL;
    }
}

void phiFU::elaborate( std::map<int, scalarRegister*> &sRegMap,
		       constantPool *cPool,
		       controlFSM *cFSM,
		       std::map<Value*, scalarArgument*> &sArgMap)
{
  this->cFSM = cFSM;
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;
      int rd;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      PHINode *phi = dyn_cast<PHINode>(ins);
      if(phi==0) {
	errs() << "no phi in phiFU::elaborate!!!\n";
	exit(-1);
      }
      int numIncoming = (int)phi->getNumIncomingValues();
      for(int i = 0; i < numIncoming; i++)
	{
	  BasicBlock *BB = phi->getIncomingBlock(i);
	  Value *pV = phi->getIncomingValue(i);
	  
	  int blkNum = cFSM->getBlockNum(BB);
	  scalarValue *sV = findScalarRef(pV,
					  cPool,
					  regMap,
					  sRegMap,
					  sArgMap);
	  
	  pair<int, scalarValue*> pPair(blkNum, sV);
	  phiOperands[startState].push_back(pPair);	  
	  
	}

      if(regMap->find(V) == regMap->end()) {
	errs() << *ins << " phi isn't writing a register???\n";
	exit(-1);
      }
      rd = (*regMap)[V];
      scalarRegister *sRD = sRegMap[rd];
      assert(sRD);
      
      /* one cycle before results are
       * available (<= in Verilog) */
      int writeState = (doneState-1);
      assert(writeState >= startState);
      
      sRD->addWriter(this, writeState);

    }
}

string phiFU::asVerilog()
{
  string s = string("\n\nassign ") + getName() + " = \n";
  for(map<int,list<pair<int, scalarValue*> > >::iterator mit =
	phiOperands.begin(); mit != phiOperands.end(); mit++)
    {
      string stateStr = int2string(mit->first);
      string muxStr = string("( ");
      for(list<pair<int, scalarValue*> >::iterator lit = (mit->second).begin();
	  lit != (mit->second).end(); lit++)
	{
	  pair<int, scalarValue*> pPair = *lit;
	  muxStr += string("(r_lbb == ") + int2string(pPair.first) + string(" ) ? ") +
	    (pPair.second)->getName() + string(" : ");
	}
      muxStr += string("32'd0 )");
      //errs() << muxStr << string("\n");

      s += string("(state == ") + stateStr + string(" ) ? ") +
	muxStr + string(" : \n"); 
    }
  s += string("32'd0;\n");
  fuVerilog = s;
  return s;
}

string branchFU::emitVerilogFwdDef()
{
  string s;
  return s;
}


void branchFU::elaborate( std::map<int, scalarRegister*> &sRegMap,
			  constantPool *cPool,	
			  controlFSM *cFSM,
			  std::map<Value*, scalarArgument*> &sArgMap)
{
 for(map<int, Value*>::iterator mit = operationMap.begin();
     mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      BranchInst *Br = dyn_cast<BranchInst>(ins);
      if(Br==0) {
	errs() << "no branch in branchFU::elaborate\n";
	exit(-1);
      }

      if(Br->isConditional())
	{
	  Value *C = Br->getCondition();
	  
	  scalarValue *v0 = findScalarRef(C,
					  cPool,
					  regMap,
					  sRegMap,
					  sArgMap);
	  operandA[startState] = v0;
	}
    }

}

string branchFU::asVerilog()
{
  string s = "\n\nassign w_take_branch = \n";
  
  for(map<int, scalarValue*>::iterator mit = operandA.begin();
      mit != operandA.end(); mit++)
    {
      string stateStr = int2string(mit->first);
      s += string("(state == ") + stateStr + string(" ) ? ") +
	(mit->second)->getName() + string("[0] : \n"); 
    }
  s += string("1'b0;\n"); 
  fuVerilog = s;
  return s;
}


void imulFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
		       constantPool *cPool,
		       controlFSM *cFSM,
		       std::map<Value*, scalarArgument*> &sArgMap)
{
  
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      
      int numOperands = ins->getNumOperands();
      assert(numOperands == 2);

      scalarValue *v0 = findScalarRef(ins->getOperand(0),
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
      scalarValue *v1 = findScalarRef(ins->getOperand(1),
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
	operandA[startState] = v0;
	operandB[startState] = v1;
	
      /* If we can't find ourselves in the
       * register map, something has gone
       * horribly wrong because all ALU
       * instructions write their state */
      if(regMap->find(V) == regMap->end()) {
	errs() << *ins << " isn't writing a register???\n";
	exit(-1);
      }

      rd = (*regMap)[V];
      scalarRegister *sRD = sRegMap[rd];
      assert(sRD);
      
      /* one cycle before results are
       * available (<= in Verilog) */
      int writeState = (doneState-1);
      assert(writeState >= startState);
      
      sRD->addWriter(this, writeState);
    
    }
}

string imulFU::asVerilog()
{
  if(operationMap.size() == 0)
    return string("");

  string p0,p1,m;
  string tName = string("t_") + getName();

  string p0Name = string("p0_") + getName();
  string p1Name = string("p1_") + getName();

  
  /* emit verilog for operand muxs */

  p0 = string("wire [31:0] p0_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandA[startState];
      p0 += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  p0 += string("\t\t32'd0;\n");
  
  p1 = string("wire [31:0] p1_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandB[startState];
      p1 += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  p1 += string("\t\t32'd0;\n");


  m = string("reg [31:0] ") + tName + string(" [") + 
    int2string(fuLatency-2) + string(":0];\n");
  
  /* emit RTL for multiplier */
  m += string("always@(posedge clk)\n");
  m += string("begin\n");
  
  m += tName + string("[0] <= ") + 
    p0Name + string(" *  ") +
    p1Name + ";\n";
  
  for(int i = 1; i < (fuLatency-1); i++)
    {
      m += tName + string("[") + int2string(i) + string("] <= ") + 
	tName + string("[") + int2string(i-1) + string("];\n"); 
    }
  

  m += string("end\n");  

  m += string("assign ") + getName() + " = " + tName + string("[") + 
    int2string(fuLatency-2) +  string("];\n");  

  fuVerilog = (p0+p1+m);
  
  return fuVerilog;
}


string retFU::asVerilog()
{
  string s = string("assign w_ret = \n");
  
  for(map<int, scalarValue*>::iterator mit = operandA.begin();
      mit != operandA.end(); mit++)
    {
      string state = int2string(mit->first);
      s += string("(state == ") + state + string(") ? ") +
	(mit->second)->getName() + string(" : ");
    }
  s +=  string("32'd0;\n");
  fuVerilog = s;
  return fuVerilog;
}


string retFU::emitVerilogFwdDef()
{
  string s;
  return s;
}


void retFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
		      constantPool *cPool,
		      controlFSM *cFSM,
		      std::map<Value*, scalarArgument*> &sArgMap)
{
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      
      int numOperands = ins->getNumOperands();
      assert(numOperands <= 1);

      if(numOperands == 1) {
	scalarValue *v0 = findScalarRef(ins->getOperand(0),
					cPool,
					regMap,
					sRegMap,
					sArgMap);
	operandA[startState] = v0;
      }
    }
}

				  
void aluFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
		      constantPool *cPool,
		      controlFSM *cFSM,
		      std::map<Value*, scalarArgument*> &sArgMap)
{
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      
      int numOperands = ins->getNumOperands();
      assert(numOperands >= 1);
      assert(numOperands <= 2);

      aluOps[ins->getOpcode()].push_back(ins);

      if(numOperands == 1) {
	scalarValue *v0 = findScalarRef(ins->getOperand(0),    
					cPool,
					regMap,
					sRegMap,
					sArgMap);
	operandA[startState] = v0;
      }
      else if(numOperands == 2) {
	scalarValue *v0 = findScalarRef(ins->getOperand(0),
					cPool,
					regMap,
					sRegMap,
					sArgMap);

	scalarValue *v1 = findScalarRef(ins->getOperand(1),
					cPool,
					regMap,
					sRegMap,
					sArgMap);
       
	operandA[startState] = v0;
	operandB[startState] = v1;
      }
      
      /* If we can't find ourselves in the
       * register map, something has gone
       * horribly wrong because all ALU
       * instructions write their state */
      if(regMap->find(V) == regMap->end()) {
	errs() << *ins << " isn't writing a register???\n";
	exit(-1);
      }

      rd = (*regMap)[V];
      scalarRegister *sRD = sRegMap[rd];
      assert(sRD);
      
      /* one cycle before results are
       * available (<= in Verilog) */
      int writeState = (doneState-1);
      assert(writeState >= startState);
      
      sRD->addWriter(this, writeState);
    
    }
}

string aluFU::asVerilog()
{
  if(operationMap.size() == 0)
    return string("");
  
  string p0,p1,m;
  string tName = string("t_") + getName();
 
  string temp = string("\n\nreg [31:0] ") + tName + string(";\n");
  string p0Name = string("p0_") + getName();
  string p1Name = string("p1_") + getName();


  if(fuLatency != 1) {
    errs() << "RTL generate assumes simple ALU has lat==1\n";
    exit(-1);
  }
  
  /* emit verilog for one port */

  p0 = string("wire [31:0] p0_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandA[startState];
      p0 += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  p0 += string("\t\t32'd0;\n");
  

  p1 = string("wire [31:0] p1_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      if(operandB.find(startState) == operandB.end())
	{
	  p1 += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	    string("32'd0") + string(" : \n");
	}
      else
	{
	  scalarValue *sV = operandB[startState];
	  p1 += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	    sV->getName() + string(" : \n");
	}
    }
  p1 += string("\t\t32'd0;\n");


  for(map<int, vector<Instruction*> >::iterator mit=aluOps.begin(); 
	mit != aluOps.end(); mit++)
    {
      Instruction *oIns = (mit->second)[0];
      if(oIns->getNumOperands()==2 && 
	 oIns->getOpcode() != Instruction::ICmp)
	{
	  string o = string("wire [31:0] ") + string(oIns->getOpcodeName()) + getName() + string(" = ");
	  o += p0Name + string(" ") + sP->get_operator(oIns) + string(" ") + p1Name + string(";\n");
	  m += o;
	}
    }

  /* emit RTL for ALU */
  m += string("always@(*)\n");
  m += string("begin\n");
  m += tName + string("=32'd0;\n");
  m += "case(state)\n";
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      Instruction *ins = dyn_cast<Instruction>(mit->second);
      string oName = string(ins->getOpcodeName()) + getName();

      m += int2string(startState) + string(":\n");
      m += string("\t") + tName + string(" = "); 

      /* binary operations */
      if(operandB.find(startState) != operandB.end())
	{
	  if(ins->getOpcode() != Instruction::ICmp)
	    m += oName;	
	  else
	    m += p0Name + string(" ") + sP->get_operator(ins) + string(" ") + p1Name;

	}
      /* unary operations */
      else
	{
	  /*TODO: this is not really correct */
	  m += sP->get_operator(ins) + p0Name;
	}
      m += string(";\n");
    }

  m += "endcase\n";
  m += string("end\n");


  m += string("assign ") + getName() + " = " + tName + ";\n"; 
  fuVerilog = temp+p0+p1+m; 
  return fuVerilog;
}

/* arguments in to the program */
scalarArgument::scalarArgument(Value *arg) : scalarValue()
{
  this->arg = arg;
  Argument *aVal = dyn_cast<Argument>(arg);
  if(aVal) {
    name = string("arg_") + aVal->getNameStr();
  } 
  else {
    errs() << "things have gone wrong with a scalar argument\n";
    exit(-1);
  }
}

string scalarArgument::getName()
{
  return name;
}


scalarImmediate::scalarImmediate(Value *immedValue) : scalarValue()
{
  this->immedValue = immedValue;
}

scalarImmediate::scalarImmediate(int val) : scalarValue()
{
  this->immedValue = 0;
  name = int2string(val);
}

scalarImmediate::scalarImmediate(float val) : scalarValue()
{
  this->immedValue = 0; 
  unsigned asUnsigned = *((unsigned*)(&val));
  //errs() << "scalarImmediate::scalarImmediate(float val)\n";
  //errs() << "is nearly 100 percent incorrect\n";
  //exit(-1);
  name = int2string(asUnsigned);
}


string scalarImmediate::getName()
{
  return name;
}

scalarRegister::scalarRegister(int id) : scalarValue() 
{
  this->id = id;
  name = string("reg_") + int2string(id);
}

void scalarRegister::addWriter(fuBase *b, int state)
{
  /* sanity check: we can't have two writers in a single state */
  assert(writeMap.find(state) == writeMap.end());
  /* write occurs at this state */
  writeMap[state] = b;
}

string scalarRegister::emitVerilogFwdDef()
{
  string s = string("reg [31:0] reg_") + int2string(id) + string(";\n");
  return s;
}


string scalarRegister::asVerilog()
{
  string fullName = string("reg_") + int2string(id); 
    
  string s = string("always@(posedge clk)\n");
  s += string("begin\n\t");
  s += fullName + string(" <=\n");

  for(map<int, fuBase*>::iterator mit = writeMap.begin();
      mit != writeMap.end(); mit++)
    {
      s += string("\t\t(state == ") + int2string(mit->first) + string(") ? ") + 
	(mit->second)->getName() + string(" : \n");
    }
  s += string("\t\t") + fullName + string(";\n");
  s += string("end\n\n");
  return s;
}



void addrFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap)
{ 
  /* If our program doesn't have any memory operations,
   * then we don't need to worry about generating 
   * addresses */
  if(operationMap.size() == 0) 
    return;


  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);

      int numOperands = ins->getNumOperands();
      if(numOperands != 2) {
	errs() << "addrFU::elaborate GEP does not have 2 operands!!!!\n";
	exit(-1);
      }
      
      /* Op0 should be the base address:
       * For example in A[i] the base will
       * value with the address of A in it */
      Value *op0 = ins->getOperand(0);
      /* The offset */
      Value *op1 = ins->getOperand(1);

      scalarValue *v0 = findScalarRef(op0,
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
      scalarValue *v1 = findScalarRef(op1,
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);

      baseOperand[startState] = v0;
      offsetOperand[startState] = v1;

      if(regMap->find(V) == regMap->end()) {
	errs() << *ins << " GEP isn't writing a register???\n";
	exit(-1);
      }

      int rd = (*regMap)[V];
      scalarRegister *sRD = sRegMap[rd];
      assert(sRD);
      
      /* one cycle before results are
       * available (<= in Verilog) */
      int writeState = (doneState-1);
      assert(writeState >= startState);
      
      sRD->addWriter(this, writeState);
    }

  if(baseOperand.size() !=
     offsetOperand.size())
    {
      errs() << "GEP port num mismatch!\n";
      exit(-1);
    }

}

string addrFU::asVerilog()
{
  string s;
  string p0Name = string("p0_") + getName();
  string p1Name = string("p1_") + getName();
  
  /* emit muxen */
  s += string("wire [31:0] p0_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = baseOperand[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n\n");
  

  s += string("wire [31:0] p1_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = offsetOperand[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
	
    }
  s += string("\t\t32'd0;\n\n");
  
  /* emit adder */
  s += string("assign ") + getName() + string(" = ") + 
    p0Name + string(" + ") + p1Name + string(";\n\n");

  fuVerilog = s;
  return s;
}


void memFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap)
{ 
  /* If our program doesn't have any memory operations,
   * then we don't need to worry about generating 
   * addresses */
  if(operationMap.size() == 0) 
    return;


  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      int numOperands = ins->getNumOperands();
      
      if(ins->getOpcode() == Instruction::Store)
	{
	  if(numOperands != 2) {
	    errs() << "numOperands wrong for store\n";
	    exit(-1);
	  }

	  Value *dataOperand = ins->getOperand(0);
	  Value *addrOperand = ins->getOperand(1);

	  scalarValue *v0 = findScalarRef(addrOperand,
					  cPool,
					  regMap,
					  sRegMap,
					  sArgMap);
	  
	  scalarValue *v1 = findScalarRef(dataOperand,
					  cPool,
					  regMap,
					  sRegMap,
					  sArgMap);
	  
	  addressOperand[startState] = v0;
	  valueOperand[startState] = v1;
	}

      else if(ins->getOpcode() == Instruction::Load)
	{
	  if(numOperands != 1) {
	    errs() << "numOperands wrong for load\n";
	    exit(-1);
	  }
	  Value *addrOperand = ins->getOperand(0);
	  scalarValue *v0 = findScalarRef(addrOperand,
					  cPool,
					  regMap,
					  sRegMap,
					  sArgMap);
	  addressOperand[startState] = v0;


	  if(regMap->find(V) == regMap->end()) {
	    errs() << *ins << " Load isn't writing a register???\n";
	    exit(-1);
	  }
	  
	  int rd = (*regMap)[V];
	  scalarRegister *sRD = sRegMap[rd];
	  assert(sRD);
	  
	  /* one cycle before results are
	   * available (<= in Verilog) */
	  int writeState = (doneState-1);
	  assert(writeState >= startState);
	  sRD->addWriter(this, writeState);

	}
      else
	{
	  errs() << "WTF is this doing in the memory unit:\n";
	  errs() << *ins << "\n";
	  exit(-1);
	}
    }
}

string memFU::asVerilog()
{
  string s;
  string n = int2string(memPortId);
  if(memPortId == -1)
    {
      errs() << "INVALID MEMPORT ID\n";
      exit(-1);
    }

  /* mux address */
  s += string("assign w_addr") + n + string(" = \n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = addressOperand[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n\n");
  
  /* generate request signal */
  s += string("assign w_mem_valid") + n + string(" = \n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      s += string("(state == ") + int2string(startState) + string(")");
      s += string("|"); 
    }
  s += string("1'b0;\n\n");



  /* mux data out */
  s += string("assign w_dout") + n + string(" = \n\t");
  for(map<int, scalarValue*>::iterator mit = valueOperand.begin();
      mit != valueOperand.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = (mit->second);
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n\n");

  /* store signal */
  s += string("assign w_is_st") + n + string(" = \n\t");
  for(map<int, scalarValue*>::iterator mit = valueOperand.begin();
      mit != valueOperand.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = (mit->second);
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	string(" 1'b1 : \n");
    }
  s += string("\t\t1'b0;\n\n");

  s += string("assign ") + getName() + string(" = din") + n + string(";\n");


  fuVerilog = s;
  return fuVerilog;
}

void faddFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
		       constantPool *cPool,
		       controlFSM *cFSM,
		       std::map<Value*, scalarArgument*> &sArgMap)
{
  
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      
      int numOperands = ins->getNumOperands();
      assert(numOperands == 2);

      scalarValue *v0 = findScalarRef(ins->getOperand(0),
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
      scalarValue *v1 = findScalarRef(ins->getOperand(1),
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
	operandA[startState] = v0;
	operandB[startState] = v1;
	
      if(regMap->find(V) == regMap->end()) {
	errs() << *ins << " fadd isn't writing a register???\n";
	exit(-1);
      }

      rd = (*regMap)[V];
      scalarRegister *sRD = sRegMap[rd];
      assert(sRD);
      
      /* one cycle before results are
       * available (<= in Verilog) */
      int writeState = (doneState-1);
      assert(writeState >= startState);
      
      sRD->addWriter(this, writeState);
    
    }
}

/* this is really the same code as the fadd elaborate,
 * i should probably be more clever with source reuse */
void fmulFU::elaborate(std::map<int, scalarRegister*> &sRegMap,
		       constantPool *cPool,
		       controlFSM *cFSM,
		       std::map<Value*, scalarArgument*> &sArgMap)
{
  
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      int doneState = startState + fuLatency;

      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);
      
      int rd = -1;
      
      int numOperands = ins->getNumOperands();
      assert(numOperands == 2);

      scalarValue *v0 = findScalarRef(ins->getOperand(0),
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
      scalarValue *v1 = findScalarRef(ins->getOperand(1),
				      cPool,
				      regMap,
				      sRegMap,
				      sArgMap);
      
	operandA[startState] = v0;
	operandB[startState] = v1;
	
      if(regMap->find(V) == regMap->end()) {
	errs() << *ins << " fadd isn't writing a register???\n";
	exit(-1);
      }

      rd = (*regMap)[V];
      scalarRegister *sRD = sRegMap[rd];
      assert(sRD);
      
      /* one cycle before results are
       * available (<= in Verilog) */
      int writeState = (doneState-1);
      assert(writeState >= startState);
      
      sRD->addWriter(this, writeState);
    
    }
}


string faddFU::asVerilog()
{
  if(operationMap.size() == 0)
    return string("");

  string s;

  string p0Name = string("p0_") + getName();
  string p1Name = string("p1_") + getName();
  string subName = string("fsub_") + getName();
  
  /* emit verilog for operand muxs */

  s += string("wire [31:0] p0_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandA[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n");
  
  s += string("wire [31:0] p1_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandB[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n");

  

  s += string("wire [31:0] fsub_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      Value *V = mit->second;
      Instruction *ins = dyn_cast<Instruction>(V);
      if(ins->getOpcode() == Instruction::FSub)
	{
	  s += string("(state == ") + int2string(startState) + string(") | "); 
	}
    }
  s += string("\t\t1'b0;\n");


  s += string("sp_add float_add") + getName() + string(" (\n");
  s += string("\t .y(") + getName() + string("),\n");
  s += string("\t .a(") + p0Name + string("),\n");
  s += string("\t .b(") + p1Name + string("),\n");
  s += string("\t .sub(") + subName + string("),\n");
  s += string("\t .clk(clk)\n");
  s += string(");\n");


  fuVerilog = s;
  
  return fuVerilog;
}

string fmulFU::asVerilog()
{
  if(operationMap.size() == 0)
    return string("");

  string s;

  string p0Name = string("p0_") + getName();
  string p1Name = string("p1_") + getName();

  
  /* emit verilog for operand muxs */

  s += string("wire [31:0] p0_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandA[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n");
  
  s += string("wire [31:0] p1_") + getName() + string("=\n\t");
  for(map<int, Value*>::iterator mit = operationMap.begin();
      mit != operationMap.end(); mit++)
    {
      int startState = mit->first;
      scalarValue *sV = operandB[startState];
      s += string("\t\t(state == ") + int2string(startState) + string(") ? ") + 
	sV->getName() + string(" : \n");
    }
  s += string("\t\t32'd0;\n");

  s += string("sp_mul float_mul") + getName() + string(" (\n");
  s += string("\t .y(") + getName() + string("),\n");
  s += string("\t .a(") + p0Name + string("),\n");
  s += string("\t .b(") + p1Name + string("),\n");
  s += string("\t .clk(clk)\n");
  s += string(");\n");


  fuVerilog = s;
  
  return fuVerilog;
}
