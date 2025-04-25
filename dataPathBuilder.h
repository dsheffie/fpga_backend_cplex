#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Instructions.h"
#include "llvm/Argument.h"
#include <sstream>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <list>
#include <queue>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <cstring>

#ifndef __DATAPATHBUILDER__
#define __DATAPATHBUILDER__

#include "systemParam.h"
#include "controlFSMBuilder.h"

class fuBase;
//class controlFSM;

class scalarValue
{
 public:
  std::string name;
  scalarValue() 
    { }
  virtual std::string getName() {
    llvm::errs() << "BASE CLASS!\n";
    exit(-1);
    return name;
  }
};

class scalarArgument : public scalarValue
{
 public:
  llvm::Value *arg;
  scalarArgument(llvm::Value *arg);
  virtual std::string getName();
};

class scalarImmediate : public scalarValue
{
 public:
  llvm::Value *immedValue;
  
  scalarImmediate(llvm::Value *immedValue);
  scalarImmediate(int val);
  scalarImmediate(float val);

  virtual std::string getName();
};

class constantPool
{
 public:
  std::list<scalarImmediate*> allocImm;
  std::map<int, scalarImmediate*> intPool;
  scalarImmediate *get(int val);
  scalarImmediate *get(float val);
  constantPool();
  ~constantPool();
};


class scalarRegister : public scalarValue
{
 public:
  int id;
  std::string name;

  std::map<int, fuBase*> writeMap;
  scalarRegister(int id);
  void addWriter(fuBase* b, int state);
  std::string asVerilog();
  virtual std::string emitVerilogFwdDef();
  virtual std::string getName() 
    { return name; }
};

class fuBase
{
 public:
  systemParam *sP;
  op_t fuType;
  int fuLatency;
  int id;
  std::string fuName;
  
  /* keep a copy of the generated RTL */
  std::string fuVerilog;
  
  std::map<int, llvm::Value*> operationMap;

  std::map<int, std::list<llvm::Value* > > *statePlan;
  std::map<llvm::Value*, int> *regMap;
  
  fuBase(systemParam *sP,
	 int id,
	 op_t fuType,
	 std::map<int, std::list<llvm::Value* > > *statePlan,
	 std::map<llvm::Value*, int> *regMap);
  ~fuBase();
  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);
 
  void addOperation(llvm::Value* V, int state);
  std::string getName() { return fuName; }

  virtual std::string emitVerilogFwdDef();
  virtual std::string asVerilog();
};

class aluFU : public fuBase
{
 public:
  std::map<int, scalarValue*> operandA;
  std::map<int, scalarValue*> operandB;
  std::map<int, std::vector<llvm::Instruction*> > aluOps;

  aluFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == ALU);
    }  

  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};

class imulFU : public fuBase
{
 public:
  std::map<int, scalarValue*> operandA;
  std::map<int, scalarValue*> operandB;
  
  imulFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == MUL);
    }  

  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};




class retFU : public fuBase
{
 public:
  std::map<int, scalarValue*> operandA;
  
  retFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == RET);
    }  

  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string emitVerilogFwdDef();
  virtual std::string asVerilog();
};

class addrFU : public fuBase
{
 public:
  std::map<int, scalarValue*> baseOperand;
  std::map<int, scalarValue*> offsetOperand;

 addrFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == ADDR);
    }  

  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};

class memFU : public fuBase
{
 public:
  std::map<int, scalarValue*> addressOperand;
  std::map<int, scalarValue*> valueOperand;
  int memPortId;

 memFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      memPortId = -1;
      assert(fuType == MEM);
    }  

  void setMemPortId(int memPortId) {
    this->memPortId = memPortId;
  }

  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};


class branchFU : public fuBase
{
 public:
  std::map<int, scalarValue*> operandA;
 branchFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == BRANCH);
    }  

  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);
  virtual std::string emitVerilogFwdDef();
  virtual std::string asVerilog();
};

class phiFU : public fuBase
{
 public:
  controlFSM *cFSM;
  
  std::map<int, std::list< std::pair<int, scalarValue*> > > phiOperands;
  
 phiFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      cFSM=0;
      assert(fuType == PHI);
    }  
  
  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};

class faddFU : public fuBase
{
 public:
  std::map<int, scalarValue*> operandA;
  std::map<int, scalarValue*> operandB;
  
 faddFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == FADD);
    }  
  
  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};

class fmulFU : public fuBase
{
 public:
  std::map<int, scalarValue*> operandA;
  std::map<int, scalarValue*> operandB;
  
 fmulFU(systemParam *sP,
	int id,
	op_t fuType,
	std::map<int, std::list<llvm::Value* > > *statePlan,
	std::map<llvm::Value*, int> *regMap) :
  fuBase(sP,id,fuType, statePlan, regMap)
    {
      assert(fuType == FMUL);
    }  
  
  virtual void elaborate(std::map<int, scalarRegister*> &sRegMap,
			 constantPool *cPool,
			 controlFSM *cFSM,
			 std::map<llvm::Value*, scalarArgument*> &sArgMap);

  virtual std::string asVerilog();
};


#endif
