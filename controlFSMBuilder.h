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




#ifndef __CONTROLFSMBUILDER__
#define __CONTROLFSMBUILDER__

class controlFSM;

#include "systemParam.h"
#include "dataPathBuilder.h"

class scalarArgument;

class controlFSM
{
 public:
  std::map<int, std::list<llvm::Value* > > *statePlan;
  llvm::Function *F;
  int numStates;
  int numBB;
  bool memCanStall;
  systemParam *sP;
  std::map<llvm::BasicBlock*, int> blockMap;
 
  std::map<llvm::BasicBlock*, int> *blockStateMap;
  std::set<int> branchStates;
  std::set<int> retStates;

  /* states were memory instructions retire */
  std::set<int> memRetireStates;
  
  /* always good to have */
  std::map<llvm::Value*, int> *regMap;
  std::map<llvm::Value*, scalarArgument*> *sArgMap;
  
  controlFSM(llvm::Function *F, 
	     bool memCanStall,
	     systemParam *sP,
	     std::map<llvm::Value*, int> *regMap,
	     std::map<llvm::Value*, scalarArgument*> *sArgMap,
	     int numStates,
	     std::map<llvm::BasicBlock*, int> *blockStateMap,
	     std::map<int, std::list<llvm::Value* > > *statePlan );
  std::string asVerilog();
  
  int getBlockNum(llvm::BasicBlock *BB);
  int getNumBasicBlocks();
  void saveScheduleToDisk();
};


#endif
