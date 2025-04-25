#ifndef __SCHEDBASE__
#define __SCHEDBASE__

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Instructions.h"
#include "llvm/Argument.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"

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

#include "systemParam.h"
/* When the scheduler completes,
 * the firing schedule will be placed in the
 * state plan key-value map
 *
 * The global state counter is 
 * held in the pointer "stateCounter".
 *
 * Note that the stateCounter is not reset
 * each invocation of the scheduling function.
 * This allows global allocation of state ids
 * across basic blocks.
 *
 * The deliverable from this class is the map
 * "statePlan". The state plan holds a list of
 * all operations that commence execution in a given
 * state.
 *
 */
class schedulerBase
{
 public:
  bool memBlocksIssue;
  systemParam *sP;
  std::map<int, std::list<llvm::Value* > > *statePlan;
  int *stateCounter;
  llvm::AliasAnalysis *AA;
  schedulerBase(systemParam *sP, int *stateCounter,
		std::map<int, std::list<llvm::Value* > > *statePlan,
		llvm::AliasAnalysis *AA,
		bool memBlocksIssue)
    { 
      this->sP = sP;
      this->stateCounter = stateCounter;
      this->statePlan = statePlan;
      this->memBlocksIssue = memBlocksIssue;
      this->AA = AA;
    }
  ~schedulerBase()
    { }
  virtual void schedule(llvm::BasicBlock *BB, bool isDeepest) {
    llvm::errs() << "scheduler base class called, aborting\n";
    exit(-1);
  };
  bool checkIfMemOpsAlias(llvm::Instruction *insA, 
			  llvm::Instruction *insB);

  llvm::GEPOperator* getPointerOperand(llvm::Instruction *ins);
};


#endif

