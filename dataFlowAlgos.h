#ifndef __DATAFLOWALGOS__
#define __DATAFLOWALGOS__

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Instructions.h"
#include "llvm/Argument.h"
#include "llvm/User.h"
#include "llvm/Value.h"

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


void doLiveRanges(llvm::Function &F, 
		  std::map<llvm::Value*, std::set<llvm::Value*> > &liveRanges);

void doLiveValues(llvm::Function &F, 
		  std::map<llvm::Value*, std::set<llvm::Value*> > &liveIn,
		  std::map<llvm::Value*, std::set<llvm::Value*> > &liveOut);

size_t maxLiveValues(llvm::Function &F,  
		     std::map<llvm::Value*, std::set<llvm::Value*> > &liveIn,
		     std::map<llvm::Value*, std::set<llvm::Value*> > &liveOut);

void computePlaceOfDeath(std::map<llvm::Value*, std::set<llvm::Value*> > &liveIn,
			 std::map<llvm::Value*, std::set<llvm::Value*> > &liveOut,
			 std::map<llvm::Value*, std::set<llvm::Value*> > &diesAtValue);

			 

#endif
