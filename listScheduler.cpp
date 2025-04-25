#include "listScheduler.h"
#include <cassert>


listScheduler::listScheduler(systemParam *sP, 
			     int *stateCounter,
			     map<int, list<Value*> > *statePlan,
			     AliasAnalysis *AA,
			     bool memBlocksIssue) :
  schedulerBase(sP,stateCounter,statePlan,AA,memBlocksIssue)
{

  maxInstLatency = -1;
  for(int i = 0; i < (DUMMY-ALU); i++) {
    int lat = sP->get_latency((op_t)i);
    if(lat > maxInstLatency) {
      maxInstLatency = lat;
    }
  }
  assert(maxInstLatency != -1);
  instInFlight = new list<Value*>*[maxInstLatency];
  for(int i = 0; i < maxInstLatency; i++) {
    instInFlight[i] = new list<Value*>;
  }

  fuBusyTable = new int*[(DUMMY-ALU)];
  for(int i = 0; i < (DUMMY-ALU); i++) {
    int n = sP->get_count((op_t)i);
    n = (n < 1) ? 1 : n;
    fuBusyTable[i] = new int[n];
    for(int j = 0; j < n; j++) {
      /* zero means FU is ready */
      fuBusyTable[i][j] = 0;
    }
  }

  fuTable = sP->get_fu_table();
}

listScheduler::~listScheduler() {
  for(int i = 0; i < (DUMMY-ALU); i++) {
    delete [] fuBusyTable[i];
  }
  delete [] fuBusyTable;
  for(int i = 0; i < maxInstLatency; i++) {
    delete instInFlight[i];
  }
  delete [] instInFlight;
}

/* all dependences satisfied */
bool listScheduler::isReady(Value *v)
{
  Instruction *ins = dyn_cast<Instruction>(v);
  assert(ins);
  bool isReady = true;

  bool memInFlight = (instInFlight[MEM]->size() != 0);

  bool isMemIns = (ins->getOpcode() == Instruction::Load ||
		   ins->getOpcode() == Instruction::Store);

  /* alright, if we're supporing stalling memory the naive thing
   * to do is ensure the pipeline is drained when a memory
   * instruction begins and disallow any other instructions
   * from beginning execution while a memory instruction is
   * in flight */
  if(memBlocksIssue) 
    {
      if(memInFlight)
	return false;
      else if(isMemIns && (numInstInFlight() != 0))
	return false;
    }
  

  /* we require returns and branches to execute after all
   * other instructions in the basic block 
   * have completed exection */
  if(ins->getOpcode() == Instruction::Ret || 
     ins->getOpcode() == Instruction::Br ) 
    {
    for(map<Value*, bool>::iterator it = instrDone.begin();
	it != instrDone.end(); it++) {
      /* for all instructions other than this
       * return instruction */
      if(it->first != v ) {
	/* if any instructions haven't completed exection,
	 * we can't exec */
	if(it->second == false) {
	  return false;
	}
      }
    }
  }

  /* TODO:::: this is probably wrong ... but
  * ...we assume that the sematics of PHIs combined
  * with guarantee that all instructions from previous
  * basic blocks will have retired by the time current
  * block executes to make this statement */
  if(ins->getOpcode() == Instruction::PHI)
    {
      return true;
    }


  for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); 
      op != ope; op++) 
    {
      Instruction *oIns = dyn_cast<Instruction>(*op);
      /* arguments are by definition ready */
      if(dyn_cast<Argument>(*op)) {
	continue;
      }
      else if(oIns) {
	/* the way our scheduling works, instructions from
	 * other basic blocks must have already completed
	 * before this block executes */
	
	/* this checks if the instruction is from a 
	 * different basic block
	 * if instruction is from the current scheduling
	 * basic block, use internal data-structures 
	 * to determine if execution can start */
	
	if(oIns->getParent() == ins->getParent()) {
	  /* instruction has not finished execution */
	  if(instrDone[oIns] == false) {
	    isReady = false;
	    break;
	  }
	}
	
      }
      else 
	{
	  //errs() << **op << "\n";
	  // errs() << "not argument or instruction, abort!\n";
	  //exit(-1);
	  continue;
	}
  }

  //if(isReady) errs() << *ins << " is ready\n";
  return isReady;
}

void listScheduler::schedule(BasicBlock *BB, bool isDeepest)
{
  int AtLeastOneCount = 0;
  
  for (BasicBlock::iterator i = BB->begin(), e = BB->end(); 
       i != e; ++i) {
    Instruction *ins = &(*i);
    schedMap[ins] = -1;
    instrDone[ins] = false;
    unschedInst.push_back(ins);
  }

  /* this while loop is the brains 
   * of the list scheduler */
  while(unschedInst.size() != 0) {
    /* mark function units available if instructions 
     * complete this cycle */
    updateFUTable();
    /* track progress of instructions in flight */
    updateInstQueue();
    /* find all instructions that could execute this
     * cycle. add them to the end of ready queues. 
     * this ignores availability of functional units */
    bool addedToReady = false;
    do
      {
	/*
	errs() << unschedInst.size() << " inst remain, " <<
	  numInstInFlight() << " in flight," << 
	  BB->size() << " instructions in BB\n";
	*/
	addedToReady = false;
	for(list<Value*>::iterator it = unschedInst.begin();
	    it != unschedInst.end(); it++)
	  {
	    Value *V = *it;
	    if(isReady(V)) {
	      errs() << "ready @ " << *stateCounter << 
		"   " << *(dyn_cast<Instruction>(V)) << "\n";
	      addedToReady = addToReadyQueue(V);
	      AtLeastOneCount = 0;
	      unschedInst.erase(it);
	      break;
	    }
	  }
      }
    while(addedToReady);
   
    if(addedToReady) {
      errs() << "fwd progress\n";

      AtLeastOneCount = 0;
    } else {
      //errs() << "no progress..." << AtLeastOneCount << "\n";
      AtLeastOneCount++;
      if(AtLeastOneCount > (100*maxInstLatency)) {
	errs() << "DEADLOCK!!!!\n"; 
	displayStatus();
	exit(-1);
      }
    }
 
    /* search through the ready queues to find instructions 
     * that can start executing given resource constraints */
    for(int q = 0; q < (DUMMY-ALU); q++)
      {
	tryToStartExec(q);
      }

    (*stateCounter)++;
  }

  /* all instructions have begun execution,
   * preserve invariants we must insure that
   * all instructions retire before we leave
   * this basic block */
  while(allInstRetired() == false)
    {
      updateFUTable();
      updateInstQueue();
      (*stateCounter)++;
    }


}

bool listScheduler::addToReadyQueue(Value *V) {
  op_t t = sP->get_type(V);

  for(vector<Value*>::iterator it = readyQueues[t].begin();
      it != readyQueues[t].end(); it++)
    {
      if( (*it) == V ) {
	return false;
      }
    }

  /* make sure this instruction hasn't been
   * already inserted in the ready queue */
  readyQueues[t].push_back(V);
  return true;
  
}


void listScheduler::tryToStartExec(int q) {
  bool canStart = false;
  op_t qq = (op_t)q;

  do {
    canStart = false;
    for(vector<Value*>::iterator it = readyQueues[qq].begin();
	it != readyQueues[qq].end(); it++) {
      int n = sP->get_count(qq);
      assert(n > 0);
      for(int i = 0; i < n; i++)
	{
	  /* if there's a zero in the fu busy
	   * table, the functional unit is ready
	   * for a new instruction. boo yea! */
	  if(fuBusyTable[q][i]==0) {
	    /* right now, we assume the memory
	     * subsystem is not pipelined. this
	     * will let our vector ops shine but
	     * make normal loads and stores look 
	     * kinda crappy */
	    int lat = sP->get_latency(qq);
	 
	    fuBusyTable[q][i] = (sP->is_pipelined(qq)) ? 1 :
	      lat;
	  
	    // errs() << "hot damn, starting exec \n";
	    Value *V = *it;
	    schedMap[V] = *stateCounter;

	    fuBase *b = fuTable[q][i];
	    assert(b != NULL);
	    assert(b->fuType == qq);

	    b->addOperation(V, *stateCounter);

	    canStart = true;
	    readyQueues[qq].erase(it);

	    (*statePlan)[(*stateCounter)].push_back(V);

	    //errs() << "pushed with lat = " << lat << "\n";
	    if(qq == NAOP) {
	      instrDone[V] = true;
	    } else {
	      instInFlight[lat-1]->push_back(V);
	    }
	    break;
	  } 
	  else {
	    errs() << "stalled with " << 
	      fuBusyTable[q][i] << "\n";
	  }
	}
      if(canStart) {
	break;
      }
    }
  } while(canStart);

}


void listScheduler::updateFUTable() {
  //errs() << "update FU\n";

  for(int i = 0; i < (DUMMY-ALU); i++) {
    int n = sP->get_count((op_t)i);
    if(n < 1) continue;

    for(int j = 0; j < n; j++) {
      /*
      if(fuBusyTable[i][j]==1) {
	errs() << "all done with FU at " << *stateCounter << "\n";
      }
      */

      if(fuBusyTable[i][j] > 0) {
	fuBusyTable[i][j]--;
      }
    }
  }
}


void listScheduler::updateInstQueue() {
  list<Value*> *Tmp = instInFlight[0];

  // errs() << "update Queue\n";

  for(list<Value*>::iterator it = instInFlight[0]->begin();
      it != instInFlight[0]->end(); it++) {
    Value *V = *it;
    //errs() << *V << " has retired @ " << *stateCounter << "\n";
    instrDone[V] = true;
  }
  instInFlight[0]->clear();
  for(int i = 1; i < maxInstLatency; i++)
    {
      /*
      errs() << "t " << i-1 << " has " << instInFlight[i-1]->size() <<
	"\n";
      */
      instInFlight[i-1] = instInFlight[i];
    }
  instInFlight[maxInstLatency-1] = Tmp;

}



bool listScheduler::allInstRetired() {
  size_t inFlightCount = 0;
  for(int i = 0; i < maxInstLatency; i++) {
    inFlightCount += instInFlight[i]->size();
  }
  return (inFlightCount == 0);
}

size_t listScheduler::numInstInFlight() {
  size_t inFlightCount = 0;
  for(int i = 0; i < maxInstLatency; i++) {
    inFlightCount += instInFlight[i]->size();
  }
  return inFlightCount;
}

void listScheduler::displayStatus() {
  for(map<Value*, int>::iterator it = schedMap.begin();
      it != schedMap.end(); it++)
    {
      errs() << *(it->first) << " @  " << it->second << "\n";
    }
}
