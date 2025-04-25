#ifndef __GENVERILOGPREAMBLE__
#define __GENVERILOGPREAMBLE__

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Instructions.h"
#include "llvm/Argument.h"
#include "llvm/Analysis/LoopInfo.h"

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

#include "schedulerBase.h"
#include "cplexScheduler.h"
#include "systemParam.h"
#include "dataFlowAlgos.h"
#include "allocateStorage.h"
#include "util.h"

std::string emitRTLPreamble(llvm::Function &F, 
			    int numStates,
			    int numBasicBlocks,
			    systemParam *sP,
			    std::map<llvm::Value*, scalarArgument*> &sArgMap);
#endif
