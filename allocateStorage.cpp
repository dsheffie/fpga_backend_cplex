#include "allocateStorage.h"

using namespace std;
using namespace llvm;

static bool checkArguments(map<Value*, int> &regMap,
			   map<int, Value*> &invRegMap,
			   list<int> &freeRegList,
			   systemParam *sP,
			   Value *V)
{
  Instruction *ins = dyn_cast<Instruction>(V);
  assert(ins);

  //  if(ins->getOpcode()==Instruction::PHI)
  //  return true;

  for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); 
      op != ope; op++) 
    {
      Value *O = *op;
      if(dyn_cast<Instruction>(O))
	{
	  /* check if argument has been allocated a register */
	  if(regMap.find(O) == regMap.end()) 
	    {
	      errs() << *ins << " missing operand in regMap!!!\n";
	      errs() << *O << " is missing!\n";
	      exit(-1);
	      return false;
	    } 
	  else 
	    {
	      /* check that argument register has not been added
	       * back to the freelist */
	      int rd = regMap[O];
	      for(list<int>::iterator lit = freeRegList.begin();
		  lit != freeRegList.end(); lit++)
		{
		  int freeRD = *lit;
		  if(rd == freeRD)
		    {
		      errs() << *ins << " has argument in the freelist!!!\n";
		      errs() << *O << " is in the freelist!!\n";
		      exit(-1);
		      return false;
		    }
		}
	    }

	  /* ensure registers hold proper value */
	  int fwdRD = regMap[O];
	  Value *revValue = invRegMap[fwdRD];
	  
	  if(O != revValue)
	    {
	      errs() << *O << " mapped to " << fwdRD << " but rev mapping shows to " << *revValue << " \n";
	      exit(-1);
	      return false;
	    }

	} /*end if(dyn_cast<>) */
    }
  


  return true;
}

static void handlePHIallocation(Instruction *ins,
				map<Value*, int> &regMap,
				map<int, Value*> &invRegMap,
				list<int> &freeRegList,
				bool &ranOutOfRegs)
{
  /* only fussing with PHIs right now */
  if(ins->getOpcode() != Instruction::PHI)
    return;

  /* iterate over phi's arguments */
  for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); op != ope; op++) 
    {
      Value *O = *op;
      Instruction *oIns = dyn_cast<Instruction>(O);
      
      if(!oIns)
	continue;

      if(oIns == ins) {
	errs() << "Phi feeding into itself...\n";
	exit(-1);
      }
      
      
      /* Oh sweet, we haven't allocated a register yet */
      if(regMap.find(O) == regMap.end())
	{
	  /* we can always run out of state */
	  if(freeRegList.size()==0) {
	      ranOutOfRegs=true;
	      return;
	  }

	  //errs() << "allocated state for phi " << *ins << " argument : " << *oIns << "\n";

	  int regNum = freeRegList.front();
	  freeRegList.pop_front();
	  regMap[O] = regNum;
	  invRegMap[regNum] = O;
	}
    }
}


static bool doesntWriteDestReg(Instruction *ins)
{
  switch(ins->getOpcode())
    {
    case Instruction::Br:
      return true;
      break;
    case Instruction::Store:
      return true;
      break;
    default:
      return false;
    }
}

static void buildUseChains(Function &F,
			   map<Value*, set<Value*> > &useMap)
{
  /* iterate over all instructions in function */
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
    {
      Instruction *producerIns = &(*I);
      for (Value::use_iterator i = producerIns->use_begin(), 
	     e = producerIns->use_end(); i != e; ++i)
	{
	  Instruction *consumerIns = dyn_cast<Instruction>(*i);
	  assert(consumerIns);
	  useMap[producerIns].insert(consumerIns);
	}
    }
}

static void manageFreeList(Instruction *ins,
			   bool reuseRegisters,
			   map<Value*, set<Value*> > &useMap,
			   map<Value*, int> &regMap,
			   map<int, Value*> &invRegMap,
			   list<int> &freeRegList)
{
  bool insIsPhi = (ins->getOpcode() == Instruction::PHI);
  bool foundPhiUse = false;


  /* TODO: this is horribly broken if register reuse
   * is enabled...setting foundPhiUse will disable
   * any form of register recycling */
  foundPhiUse=!reuseRegisters;

  /* Note: PHI node register allocation is subpar currently */
  if(ins->getOpcode() == Instruction::PHI)
    {
      return;
    }


  for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); op != ope; op++) 
    {
      Value *O = *op;
      Instruction *oIns = dyn_cast<Instruction>(O); 
      if(oIns)
	{
	  if(oIns->getOpcode() == Instruction::PHI) {
	    //errs() << "found phi use of " << *oIns << "\n";
	    //errs() << "reg " << regMap[O] << " can't go back to free list\n";
	    foundPhiUse=true;
	  }

	  /* current instruction uses Value* O */
	  useMap[O].erase(ins);
	  bool noUsesRemain = (useMap[O].size()==0);
	  bool parentInBB = (ins->getParent() == oIns->getParent());
	      


	  if(parentInBB && noUsesRemain && !(insIsPhi || foundPhiUse))
	    {
	      //errs() << *oIns << " can go back to freelist now\n";
	      
	      int regNum = regMap[O];
	      invRegMap[regNum] = (Value*)NULL;
	      freeRegList.push_back(regNum);
	    }
	}

    }
}

void simpleScalarAllocation(llvm::Function &F,
			    bool reuseRegisters,
			    std::map<int, std::list<llvm::Value*> > &functSched,
			    std::map<llvm::Value*, std::set<llvm::Value*> > &diesAtValue,
			    int numScalarRegs,
			    std::map<llvm::Value*, int> &regMap,
			    systemParam *sP,
			    int &totalRegisters)
{
  /* Register number to LLVM value */
  map<int, Value*> invRegMap;

  map<Value*, set<Value*> > useMap;

  bool ranOutOfRegs = false;
  int additionalRegs = 0;
  do
    {
      ranOutOfRegs = false;
      regMap.clear();
      invRegMap.clear();
      useMap.clear();

      assert(regMap.size() == 0);
      assert(invRegMap.size() == 0);
      assert(useMap.size() == 0);

      buildUseChains(F, useMap);


       /* initially add all registers to the free 
       * list */
      list<int> freeRegList;
      for(int i = 0; i < (numScalarRegs+additionalRegs); i++){
	freeRegList.push_back(i);
	invRegMap[i] = (Value*)NULL;
      }
      
      /* iterate over the schedule */
      for(map<int, std::list<llvm::Value*> >::iterator mit = functSched.begin();
	  mit != functSched.end(); mit++)
	{
	  /* allocate registers for every instruction that fires on this
	   * cycle */
	  for(list<Value*>::iterator lit = (mit->second).begin();
	      lit != (mit->second).end(); lit++)
	    {
	      Value *V = *lit;
	      Instruction *ins = dyn_cast<Instruction>(V);
	      assert(ins);

	      
	      if(doesntWriteDestReg(ins)) {
		continue;
	      }
	      
	      /* sanity check */
	      if(freeRegList.size() == 0) {
		//errs() << "we ran out of registers, must abort @ state " << mit->first << "\n";
		ranOutOfRegs=true;
		additionalRegs++;
		break;
	      }

	      /* Need to properly handle stupid phi instructions */
	      handlePHIallocation(ins, regMap, invRegMap,
				  freeRegList, ranOutOfRegs);

	      if(ranOutOfRegs)
		{ 
		  additionalRegs++;
		  break;
		}
	      
	      /* no register allocated yet */
	      if(regMap.find(V) == regMap.end())
		{
		  int regNum = freeRegList.front();
		  if(freeRegList.size()==0) {
		    ranOutOfRegs = true;
		    additionalRegs++;
		    errs() << "ranout of registers here for :" << *ins << "\n";
		    errs() << "additionalRegs = " << additionalRegs << "\n";
		    break;
		    //exit(-1);
		  }
		  freeRegList.pop_front();
		  regMap[V] = regNum;
		  invRegMap[regNum] = V;
		}
	      /* must be an argument of a phi function */
	      else
		{
		  //errs() << "used by phi? " << *ins << "\n";
		}
	      
	      bool isSane = checkArguments(regMap, invRegMap, freeRegList, sP, V); 
	      assert(isSane);

	    }
	  
	  if(ranOutOfRegs) {
	    break;
	  }
	  
	  /* Now that register allocation is complete for the
	   * current cycle, check if any registers can be
	   * added back to the free list */
	  for(list<Value*>::iterator lit = (mit->second).begin();
	      lit != (mit->second).end(); lit++)
	    {
	      Value *V = *lit;
	      Instruction *ins = dyn_cast<Instruction>(V);
	      assert(ins);
	      
	      
	      manageFreeList(ins,reuseRegisters,
			     useMap,regMap,
			     invRegMap,freeRegList);

	      /*
		errs() << *ins << ":\n";
	      for(set<Value*>::iterator sit = diesAtValue[V].begin();
		  sit != diesAtValue[V].end(); sit++)
		{
		  Value *O = *sit;
		  Instruction *oIns = dyn_cast<Instruction>(O);
		  assert(oIns);
		  
		  int regNum = regMap[*sit];
		  invRegMap[regNum] = (Value*)NULL;
		  freeRegList.push_back(regNum);

		  errs() << "\tdead:" << regNum << " -> " << *oIns << "\n";
		  }
	      */
	    }

	}
    } 
  while(ranOutOfRegs);

  for(map<int, list<Value*> >::iterator it = functSched.begin();
      it != functSched.end(); it++) 
    {
      (errs() << "State " << it->first << ":\n");
      for(list<Value*>::iterator lit = (it->second).begin();
	  lit != (it->second).end(); lit++)
	    {
	      Value *V = *lit;
	      Instruction *ins = dyn_cast<Instruction>(V);
	      assert(ins);
	      
	      int rd = -1;
	      if(doesntWriteDestReg(ins)==false)
		rd = regMap[V];

	      (errs() << "\t" << "dest " << rd 
		    << " : " <<*V << "\n");
	    }
    }

  totalRegisters = (numScalarRegs+additionalRegs);
  errs() << "Using " << totalRegisters << " 32-bit registers\n";

}
