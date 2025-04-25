
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Instructions.h"
#include "llvm/Argument.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/CommandLine.h"
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

#include "schedulerBase.h"
#include "cplexScheduler.h"
#include "systemParam.h"
#include "dataFlowAlgos.h"
#include "allocateStorage.h"
#include "generateVerilogPreamble.h"
#include "util.h"

using namespace llvm;
using namespace std;

static cl::opt<unsigned> numMemoryUnits("numMemPipes", cl::init(1), cl::Hidden,
					cl::desc("Number of load/store units"));

static cl::opt<bool> useILPsched("useILP", cl::init(true), cl::Hidden,
				     cl::desc("Use CPLEX"));

static cl::opt<bool> regRecycle("reuseRegisters", cl::init(true), cl::Hidden,
				cl::desc("Reuse registers"));

static BasicBlock* findDeepestBasicBlock(LoopInfo &LI, Function &F)
{
  BasicBlock *deepestBlock = NULL;
  unsigned maxDepth = 0;

  for (inst_iterator I = inst_begin(F), 
	 E = inst_end(F); I != E; ++I) 
    {
      Instruction *i = &(*I);
      BasicBlock *BB = i->getParent();
      
      unsigned d = LI.getLoopDepth(BB);
    
      if(d > maxDepth) {
	maxDepth = d;
	deepestBlock = BB;
      }

    }
  return deepestBlock;
}


namespace {
  struct schedPass : public FunctionPass {
    
    static char ID;
    schedPass() : FunctionPass(ID) {}
   
    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfo>();
      AU.addRequiredTransitive<AliasAnalysis>();

    }
    virtual bool runOnFunction(Function &F) 
    {
      LoopInfo &LI = getAnalysis<LoopInfo>();
      AliasAnalysis &AA = getAnalysis<AliasAnalysis>();

      bool reuseRegisters = regRecycle ? true : false;

      bool memCanStall = true;
      bool memIsPipelined = (memCanStall) ? false : true;
      int numMemPipelines = numMemoryUnits;
      bool useILP = useILPsched ? true : false;
      int stateCounter = 0;

      //errs() << F << "\n";
      

      /* key-value store that maps
       * what instructions fire in each 
       * state */
      map<int, list<Value* > > statePlan;

      /* live variable analysis data-structures */
      map<Value*, set<Value*> > liveIn;
      map<Value*, set<Value*> > liveOut;
     
      map<BasicBlock*, int> blockStateMap;
      map<Value*, set<Value*> > diesAtValue;

      constantPool *cPool = new constantPool();

      /* Step 0: generate HW menu */

      /* create system configuration */
      systemParam *sp = new systemParam(memIsPipelined);
      sp->set(ALU, 4, 1);
      sp->set(MUL, 2, 6);
      sp->set(ADDR, numMemPipelines, 1);
      //sp->set(MEM, numMemPipelines, 3);
      sp->set(MEM, numMemPipelines, 3);
      sp->set(FADD, 1, 5);
      sp->set(FMUL, 1, 6);
      sp->set(RET, 1, 1);
      sp->set(PHI, 2, 1);
      sp->set(BRANCH,1,1);
      /* "no-op" units...these are
       * just wires */
      sp->set(NAOP,2,0);
      /* number of "splatter" units */
      sp->set(VMISC,1,1);
      
      int postAllocRegNum = 0;
      map<Value*, int> regMap;
      map<Value*, scalarArgument*> sArgMap;


  for(Function::arg_iterator I = F.arg_begin(), E = F.arg_end();
      I != E; ++I)
    {
      Argument *a = I;
      scalarArgument *sR = new scalarArgument(a);
      sArgMap[a] = sR;
    }
  

 
  int numFUs;
  fuBase ** dataPath = sp->getDataPathObjs(numFUs, &statePlan,
					   &regMap);
  errs() << numFUs << " total functional units\n";


  BasicBlock *deepestBasicBlock = 
    findDeepestBasicBlock(LI, F);
  /* arguments to function will be "live" into the 
   * function */
  
  /* Step 1: schedule each basic block */
  
  for (Function::iterator i = F.begin(), e = F.end(); 
       i != e; ++i) 
    {
      BasicBlock *BB = &(*i);
      bool isDeepest = (BB == deepestBasicBlock);
      blockStateMap[BB] = stateCounter;
      cplexScheduler *lS = new cplexScheduler(sp,&stateCounter,
					      &statePlan,&AA,
					      memCanStall);
      lS->setUseILP(useILP);
      lS->schedule(BB, isDeepest);
      delete lS;
    }
      
      errs() << "scheduling function took " << stateCounter << " cycles\n";
      
      size_t totalSched = 0;

      for(map<int, list<Value*> >::iterator it = statePlan.begin();
	  it != statePlan.end(); it++) 
	{
	  //errs() << "State " << it->first << ":\n";
	  totalSched += (it->second).size();
	  /*
	  for(list<Value*>::iterator lit = (it->second).begin();
	      lit != (it->second).end(); lit++)
	    {
	      Value *V = *lit;
	      errs() << "\t" << *V << "\n";
	    }
	  */
	}
      
      errs() << totalSched << " instructions scheduled\n";

      /* Step 2: perform dataflow analysis to determine the maximum
       * number of registers (state) needed */


      doLiveValues(F,liveIn,liveOut);
           
      computePlaceOfDeath(liveIn,liveOut, diesAtValue);

      size_t numRegisters = maxLiveValues(F,liveIn,liveOut);
      
      errs() << numRegisters << " registers needed to hold all state\n";

      simpleScalarAllocation(F, 
			     reuseRegisters,
			     statePlan,
			     diesAtValue,
			     numRegisters,
			     regMap,
			     sp, 
			     postAllocRegNum);


      map<int, scalarRegister*> sRegMap;
     
      scalarRegister **scalarRegFile = 
	new scalarRegister*[postAllocRegNum];

      for(int i = 0; i < postAllocRegNum; i++)
	{
	  scalarRegFile[i] = new scalarRegister(i);
	  sRegMap[i] = scalarRegFile[i];
	}
      
      /* generate the FSM for control from the previously
       *  computed schedule */
      controlFSM *cFSM = new controlFSM(&F, 
					memCanStall,
					sp,
					&regMap,
					&sArgMap,
					stateCounter, 
					&blockStateMap,
					&statePlan);
      
      cFSM->saveScheduleToDisk();

      for(int i = 0; i < numFUs; i++) {
	dataPath[i]->elaborate(sRegMap,cPool,cFSM,sArgMap);
      }
      

      /* emit the whole module functName(...); */
      string asVerilog = emitRTLPreamble(F,stateCounter,
					 cFSM->getNumBasicBlocks(),
					 sp,
					 sArgMap);

      /* emit forward definitions for wires used 
       * for the outputs of the functional units */
      for(int i = 0; i < numFUs; i++) {
	asVerilog += dataPath[i]->emitVerilogFwdDef();
      }

      /* emit forward definitions for the registers 
       * used in the design */
      for(int i = 0; i < postAllocRegNum; i++) {
	asVerilog += scalarRegFile[i]->emitVerilogFwdDef();
      }

      /* emit RTL for control FSM */
      asVerilog += cFSM->asVerilog();
      
      for(int i = 0; i < postAllocRegNum; i++) {
	asVerilog += scalarRegFile[i]->asVerilog();
      }
      
      for(int i = 0; i < numFUs; i++) {
	asVerilog += dataPath[i]->asVerilog();
      }
      
     

    for(int i = 0; i < numFUs; i++)
      delete dataPath[i];
    delete [] dataPath;

      //assert(totalSched == F.size());
      delete sp;

      for(int i = 0; i < postAllocRegNum; i++)
	{
	  delete scalarRegFile[i];
	}
      delete [] scalarRegFile;

      //errs() << lg2(63) << "\n";

      for(map<Value*, scalarArgument*>::iterator mit = sArgMap.begin();
	  mit != sArgMap.end(); mit++)
	{
	  scalarArgument *sR = mit->second;
	  delete sR;
	}

      delete cFSM;
      delete cPool;

      asVerilog += string("endmodule\n");
      string fname = F.getNameStr() + string("_ilp.v");
      FILE *fp = fopen(fname.c_str(), "w");
      fprintf(fp, "%s", asVerilog.c_str());
      fclose(fp);

      std::string enILPString = useILP ? "enabled\n" : "disabled\n";
      std::string enRegString = reuseRegisters ? "enabled\n" : "disabled\n";

      errs() << "\n\nILP scheduler was " << enILPString;
      errs() << int2string(numMemPipelines) << " memory pipelines\n";
      errs() << "reuse regs was " << enRegString;
      return false;
    }
  };
  
  char schedPass::ID = 0;
  static RegisterPass<schedPass> 
  X("schedPass", "Modify basic blocks", true, false);
}
