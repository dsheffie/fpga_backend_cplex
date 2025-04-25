#include "cplexScheduler.h"
#include <cassert>
#include "util.h"
#include <ilcplex/ilocplex.h>


static int filenum = 0;

cplexScheduler::cplexScheduler(systemParam *sP, 
			       int *stateCounter,
			       map<int, list<Value*> > *statePlan,
			       llvm::AliasAnalysis *AA,
			       bool memBlocksIssue) :
  schedulerBase(sP,stateCounter,statePlan,AA,memBlocksIssue)
{
  this->sP = sP;
  this->stateCounter = stateCounter;
  this->statePlan = statePlan;
  this->memBlocksIssue = memBlocksIssue;
  useILP = true;

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

cplexScheduler::~cplexScheduler() {
  for(int i = 0; i < (DUMMY-ALU); i++) {
    delete [] fuBusyTable[i];
  }
  delete [] fuBusyTable;
  for(int i = 0; i < maxInstLatency; i++) {
    delete instInFlight[i];
  }
  delete [] instInFlight;
}

bool cplexScheduler::memInstInFlight()
{
  for(int i = 0; i < maxInstLatency; i++) {
    for(list<Value*>::iterator lit = instInFlight[i]->begin();
	lit != instInFlight[i]->end(); lit++) {
      Value *V = *lit;
      Instruction *ins = dyn_cast<Instruction>(V);
      if(ins->getOpcode()==Instruction::Load ||
	 ins->getOpcode()==Instruction::Store)
	{
	  return true;
	}
    }
  }

  return false;
}

size_t cplexScheduler::numMemInReadyQueue()
{
  return readyQueues[MEM].size();
}


size_t cplexScheduler::numNonMemInReadyQueue() 
{
  size_t s = 0;
  for(int i = 0; i < (DUMMY-ALU); i++)
    {
      if(((op_t)i) == MEM)
	continue;
      
      s += readyQueues[i].size();
    }
  return s;
}

bool cplexScheduler::memHazInReadyQueue(Value *V)
{
  Instruction *cIns = dyn_cast<Instruction>(V);

  for(int i = 0; i < readyQueues[MEM].size(); i++)
    {
      Instruction *oIns = 
	dyn_cast<Instruction>(readyQueues[MEM][i]);
      
      if(oIns->getOpcode() == Instruction::Load ||
	 oIns->getOpcode() == Instruction::Store)
	{
	  if(checkIfMemOpsAlias(cIns,oIns))
	    return true;
	}
    }
  return false;
}


/* all dependences satisfied */
bool cplexScheduler::isReady(Value *v)
{
  Instruction *ins = dyn_cast<Instruction>(v);
  assert(ins);
  bool isReady = true;

  bool memInFlight = 
    memInstInFlight() ||
    (numMemInReadyQueue() != 0);

  bool isMemIns = (ins->getOpcode() == Instruction::Load ||
		   ins->getOpcode() == Instruction::Store);

  /* We need to check for memory aliasing because
   * we care about getting correct answer....
   * imagine that! */
  if(isMemIns)
    {
      if(memHazInReadyQueue(v)) {
	return false;
      }  
    }

  /* alright, if we're supporing stalling memory the naive thing
   * to do is ensure the pipeline is drained when a memory
   * instruction begins and disallow any other instructions
   * from beginning execution while a memory instruction is
   * in flight */
 
  if(memBlocksIssue) 
    {
      if(isMemIns)
	{
	  if(numInstInFlight() != 0)
	    {
	      return false;
	    }
	  else if(numNonMemInReadyQueue() == 0)
	    {
	      if(numMemInReadyQueue()==0)
		{
		  //(errs() << "multiple memory launch @ " << 
		  // *stateCounter << "\n");
		  return true;
		}
	    }
	  else
	    {
	      return false;
	    }
	}
      else if(memInFlight)
	{
	  return false;
	}
    }
  
  /*
  if((*stateCounter)==39)
    {
      errs() << "num mem in ready queue = " << numMemInReadyQueue() 
	     << "\n";
      errs() << "num nonmem in ready queue = " << numNonMemInReadyQueue() 
	     << "\n";

    }

  if((*stateCounter)>39) exit(-1);
  */

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




int cplexScheduler::getInstructionNum(BasicBlock *BB, Instruction *ins) {
  int inum = 0;
  for(BasicBlock::iterator it = BB->begin(), e = BB->end() ;
      it != e; it++, inum++) {
    Instruction * cins = &(*it);
    if(ins == cins) return inum;
  }
  return -1;
}

void cplexScheduler::schedule(BasicBlock *BB, bool isDeepest)
{

  int oldStateCounter = *stateCounter;
  
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
	      //errs() << "ready @ " << *stateCounter << 
	      //	"   " << *(dyn_cast<Instruction>(V)) << "\n";
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
  
  IloEnv env;
  try {
    // Temporary
    //errs() << *BB << "\n";

    int ncycles = (*stateCounter - oldStateCounter);

    // The problem
    IloModel model(env, "Minimizing latency given functional units");

    /*
    * Objective function. Minimize total latency (which is defined below)
    */
    IloNumVar totalLatency(env, 0, IloInfinity, IloNumVar::Int, "total_latency");
    model.add(IloMinimize(env, totalLatency));

    /*
    * Declare the decision variables xij. The first dimension, i, is the number
    * of instructions needing to be scheduled. The j dimension ranges over the 
    * cycles that instructions can be started. Each instruction only has
    * one cycle in which it begins. However, multiple instructions can begin
    * in one cycle due to the avaibility of multiple functional units. 
    */
    typedef IloArray<IloBoolVarArray> BoolVarMatrix;
    BoolVarMatrix x(env, BB->size());
    for(int i = 0 ; i < BB->size(); i++)
    {
      x[i] = IloBoolVarArray(env);
      for(int j = 0 ; j < ncycles; j++) {
        string varname = "x" + int2string(i) + int2string(j);
        x[i].add( IloBoolVar(env, varname.c_str()) );
      }
    }

    /*
    * These constraints enforce that in one cycle we can only start
    * as many instructions as functional units, for each type of functional
    * unit. 
    */
    for(int j = 0 ; j < ncycles ; j++) {
      for(int fu = 0 ; fu < (DUMMY-ALU) ; fu++) {
        IloExpr sumExpr(env);
	int i = 0;
	bool valid = false;
        for(BasicBlock::iterator it = BB->begin(), e = BB->end() ;
	    it != e; it++, i++) {
	  Instruction *ins = &(*it);
	  if(sP->get_type(ins) == fu) {
	    valid = valid | true;
            sumExpr += x[i][j];
	  }
        }
	if(valid) {
          string nm = sP->get_fu_str((op_t)fu) + "_Cycle" + int2string(j);
	  int nunits = sP->get_count((op_t)fu);
	  IloConstraint con = (sumExpr <= nunits);
	  con.setName(nm.c_str());
          model.add( con);
	}
      }
    }

    /*
    * These constraints specify that each operation can only be scheduled
    * on one cycle. There is no need to schedule an operation twice
    */
    for(int i = 0 ; i < BB->size() ; i++) {
      IloExpr sumExpr(env);
      for(int j = 0 ; j < ncycles ; j++) {
        sumExpr += x[i][j];
      }
      string constraintName = "Instruction" + int2string(i);
      model.add( IloRange(env, 1, sumExpr, 1, constraintName.c_str()) );
    }

    /*
    * These are the DAG constraints. This ensures that any instruction
    * that is scheduled must complete before the consumers of this 
    * instruction.
    */

    for(BasicBlock::iterator it = BB->begin(), e = BB->end() ; 
        it != e ; it++)
    {
      Instruction *ins = &(*it);
      for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); 
          op != ope; op++) 
      {
        Instruction *oIns = dyn_cast<Instruction>(*op);
	bool valid_edge = true;
        if(oIns) {
	  op_t usetype = sP->get_type(ins);
	  op_t deftype = sP->get_type(oIns);
	  int use_i = getInstructionNum(BB,ins);
	  int def_i = getInstructionNum(BB,oIns);
	  valid_edge = valid_edge && (oIns->getParent() == ins->getParent());
	  valid_edge = valid_edge && (use_i >= 0) && (def_i >= 0);
	  valid_edge = valid_edge && (usetype != PHI);
          if(valid_edge) {
            // Add a constraint 
	    IloExpr defExpr(env);
	    IloExpr useExpr(env);
	    int latency = (sP->get_latency(deftype));
	    for(int j = 0 ; j < ncycles ; j++) {
              defExpr += j*x[def_i][j];
              useExpr += j*x[use_i][j];
	    }
	    string nm = "edge" + int2string(def_i) + int2string(use_i);
	    /* DBS must be greater than latency... */
	    IloConstraint con = (useExpr - defExpr >= latency);
	    con.setName(nm.c_str());
            model.add( con);
          }
        }
      }
    }

    /* 
    * These are special constrains for branches and return statements. 
    * Everything must complete before the branch starts executing. 
    */
    int branch_i = -1;
    for(BasicBlock::iterator it1 = BB->begin(), e = BB->end() ; 
        it1 != e ; it1++)
    {
      Instruction *ins = &(*it1);
      op_t instype = sP->get_type(ins);
      if(instype == BRANCH) {
        branch_i = getInstructionNum(BB, ins);
      }
    }
    int def_i = 0;
    for(BasicBlock::iterator it1 = BB->begin(), e = BB->end() ; 
        (it1 != e) && def_i < branch_i ; it1++, def_i++)
    {
      Instruction *ins = &(*it1);
      if(ins) {
        op_t instype = sP->get_type(ins);
        IloExpr defExpr(env);
        IloExpr useExpr(env);
        int latency = sP->get_latency(instype);
        for(int j = 0 ; j < ncycles ; j++) {
          defExpr += j*x[def_i][j];
          useExpr += j*x[branch_i][j];
        }
	string nm = "branch" + int2string(def_i) + int2string(branch_i);
	IloConstraint con = (useExpr - defExpr >= latency);
	con.setName(nm.c_str());
        model.add( con);
      }
    }

    /*
    * This constraint sets the total latency to be the last finishing
    * instruction
    */ 
    def_i = 0;
    for(BasicBlock::iterator it1 = BB->begin(), e = BB->end() ; 
        (it1 != e) ; it1++, def_i++)
    {
      Instruction *ins = &(*it1);
      IloExpr latencyExpr(env);
      for(int j = 0 ; j < ncycles ; j++) {
        latencyExpr += x[def_i][j] * j;
      }
      op_t instype = sP->get_type(ins);
      int unit_latency = sP->get_latency(instype);
      string nm = "total_latency" + int2string(def_i);
      IloConstraint con = (latencyExpr + unit_latency <= totalLatency);
      con.setName(nm.c_str());
      model.add(con);
    }

    //errs() << "SCHEDULE " << int2string(filenum) << "\n\n";
    //errs() << *BB << "\n\n";
    IloCplex cplex(model);

    //cplex.setParam(IloCplex::TiLim, 20);

    string filename = "schedule" + int2string(filenum++) + ".lp";
    cplex.exportModel(filename.c_str());
    if( !cplex.solve() ) {
      errs() << "Found no feasible solutions to LP\n";
    } else {
      errs() << "\n\n--------------------------\n";
      errs() << "Found FEASIBLE solutions to LP\n";


      int last_cycle = 0;
      int last_latency = -1;

 
	for(int j = 0 ; j < ncycles; j++) 
	{
	  int icnt = 0;
	  int c = j+oldStateCounter;
	  bool printCycle = true;
	  
	  int myTable[(DUMMY-ALU)] = {0};


	  for (BasicBlock::iterator i = BB->begin(), e = BB->end(); 
           i != e; ++i, icnt++) 
	    {
	      
	      if(cplex.getValue(x[icnt][j])) 
		{
		  if(printCycle) {
		    errs() << "Cycle " << c << "\n";
		    printCycle = false;
		  }
		  Instruction *ins = &(*i);
		  op_t type = sP->get_type(ins);
		  errs() << *ins /*sP->get_fu_str(type)*/ << ": ";
 
		  if(c >= last_cycle) {
		    errs() << *ins << "\n";
		    last_latency = sP->get_latency(type);
		    //errs() << last_latency << "\n";
		    //exit(-1);
		    last_cycle = c;
		  }

		  if(useILP)
		    {
		      (*statePlan)[c].push_back(ins);
		      int myIdx = myTable[type];
		      myTable[type]++;
		      fuBase *b = fuTable[type][myIdx];

		      b->addOperation(ins, c);
		    }
	  
		 
		}
	    }
	}
     
      char buf[256];
      sprintf(buf, "ilp %d cycles, list %d cycles\n",
	     (last_cycle-oldStateCounter),
	     (*stateCounter-oldStateCounter));
      errs() << buf << "\n";
      
      if(useILP) {
	*stateCounter = (last_cycle + last_latency);
      }

      errs() << "\n--------------------------\n";
    }

    

  }
  catch(IloException& e) {
    cerr << "Concert exception caught: " << e << endl;
  }


}

bool cplexScheduler::addToReadyQueue(Value *V) {
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


void cplexScheduler::tryToStartExec(int q) {
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

	    

	    canStart = true;
	    readyQueues[qq].erase(it);

	    if(!useILP) {
	      b->addOperation(V, *stateCounter);
	      (*statePlan)[(*stateCounter)].push_back(V);
	    }
	    
	    listPlan[(*stateCounter)].push_back(V);

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


void cplexScheduler::updateFUTable() {
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


void cplexScheduler::updateInstQueue() {
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



bool cplexScheduler::allInstRetired() {
  size_t inFlightCount = 0;
  for(int i = 0; i < maxInstLatency; i++) {
    inFlightCount += instInFlight[i]->size();
  }
  return (inFlightCount == 0);
}

size_t cplexScheduler::numInstInFlight() {
  size_t inFlightCount = 0;
  for(int i = 0; i < maxInstLatency; i++) {
    inFlightCount += instInFlight[i]->size();
  }
  return inFlightCount;
}

void cplexScheduler::displayStatus() {
  for(map<Value*, int>::iterator it = schedMap.begin();
      it != schedMap.end(); it++)
    {
      errs() << *(it->first) << " @  " << it->second << "\n";
    }
}
