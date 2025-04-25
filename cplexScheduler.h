#ifndef __LISTSCHED__
#define __LISTSCHED__

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


using namespace std;
using namespace llvm;

#include "systemParam.h"
#include "dataPathBuilder.h"
#include "schedulerBase.h"

class cplexScheduler : public schedulerBase
{
public:
  /* data structures */

  int maxInstLatency;

  /* keep track of all instructions that have
   * dependences satisfied */
  vector<Value*> readyQueues[DUMMY-ALU];
  /* keep track of FU status */
  int **fuBusyTable;

  /* table of references to each functional
   * unit in the design */
  fuBase ***fuTable;

  /* keep track of instructions currently 
   * in-flight */
  list<Value*> **instInFlight;

  /* cycle instruction starts */
  map<Value*, int> schedMap;
  /* set to true when instruction
   * finishes execution */
  map<Value*, bool> instrDone;
  list<Value*> unschedInst;
  
  map<int, list<Value* > > listPlan;
  
  
  bool useILP;
  void setUseILP(bool useILP) {
    this->useILP = useILP;
  }

  /* methods */
  cplexScheduler(systemParam *sP, int *stateCounter,
		 map<int, list<Value* > > *statePlan,
		 llvm::AliasAnalysis *AA,
		 bool memBlocksIssue);
  ~cplexScheduler();
  void updateFUTable();
  void updateInstQueue();
  virtual void schedule(BasicBlock *BB, bool isDeepest);
  /* all dependences satisfied */
  bool isReady(Value *ins);
  int getInstructionNum(BasicBlock *BB, Instruction *ins);
  bool addToReadyQueue(Value *V);
  void tryToStartExec(int q);
  bool allInstRetired();
  size_t numInstInFlight();
  void displayStatus();
  size_t numMemInReadyQueue();
  size_t numNonMemInReadyQueue();
  bool memHazInReadyQueue(Value *V);
  bool memInstInFlight();
};


#endif


