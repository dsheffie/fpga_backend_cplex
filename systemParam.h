#ifndef __SYSTEMPARAM__
#define __SYSTEMPARAM__

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

class systemParam;
class fuBase;

enum op_t {ALU=0,MUL,DIV,ADDR,MEM,
	   FADD,FMUL,FDIV,RET,PHI,BRANCH,VMISC,
	   NAOP, /* NOT AN OP, (casts,etc) */
	   DUMMY};

#include "dataPathBuilder.h"

class systemParam
{
 public:
  fuBase **dataPath;
  fuBase ***fuTable;
  int unitCounts[DUMMY-ALU];
  int unitLatencies[DUMMY-ALU];
  bool memIsPipelined;

  systemParam(bool memIsPipelined);
  ~systemParam();
  

  std::string get_fu_str(op_t t);
  void set(op_t t, int count, int latency);

  int get_latency(op_t t) {
    return unitLatencies[t];
  }
  int get_count(op_t t) {
    return unitCounts[t];
  }
  bool is_pipelined(op_t t);
  /* the total number of functional units
   * in the design */
  int getNumUnits();

  op_t get_type(llvm::Value *v);

  int get_latency(llvm::Value *V) {
    return get_latency(get_type(V));
  }


  std::string get_operator(llvm::Instruction *ins);
  
  fuBase **getDataPathObjs(int &amt,	     
			   std::map<int, std::list<llvm::Value* > > *statePlan,
			   std::map<llvm::Value*, int> *regMap  );
  
  fuBase ***get_fu_table() {
    return this->fuTable;
  }

};

#endif
