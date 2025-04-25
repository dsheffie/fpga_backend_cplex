#ifndef __ALLOCATESTORAGE__
#define __ALLOCATESTORAGE__

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/Debug.h"
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

#include "systemParam.h"

void simpleScalarAllocation(llvm::Function &F,
			    bool reuseRegisters,
			    std::map<int, std::list<llvm::Value*> > &functSched,
			    std::map<llvm::Value*, std::set<llvm::Value*> > &diesAtValue,
			    int numScalarRegs,
			    std::map<llvm::Value*, int> &regMap,
			    systemParam *sP,
			    int &totalRegisters
			    );


#endif
