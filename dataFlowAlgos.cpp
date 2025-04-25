#include "dataFlowAlgos.h"

using namespace llvm;
using namespace std;

static void mergeSets(set<Value*> &x, set<Value*> &y)
{
  for(set<Value*>::iterator it=x.begin();
      it != x.end(); it++)
    {
      Value *v = *it;
      y.insert(v);
    }
}

static void removePhiOperands(Instruction *ins, set<Value*> &liveOut)
{
  /* operands of a phi node can not be live out */
  if(dyn_cast<PHINode>(ins))
    {
      for(User::op_iterator op = ins->op_begin(), ope=ins->op_end();
	  op != ope; op++) {
	if(dyn_cast<Instruction>(*op) || dyn_cast<Argument>(*op)) 
	    {
	      //errs() << "removing from live out: " << *(*op) << "\n";
	      liveOut.erase(*op);
	    }
      }
    }
}

static void removeUndefined(set<Value*> &definedIns, set<Value*> &s)
{
  for(set<Value*>::iterator it = s.begin();
      it != s.end(); it++)
    {
      Value *v = *it;
      if(definedIns.find(v) == definedIns.end())
	{
	  s.erase(v);
	}
    }
} 




void doLiveRanges(Function &F, 
		  map<Value*, set<Value*> > &liveRanges)
{
  for (inst_iterator I = inst_begin(F), 
	 E = inst_end(F); I != E; ++I) 
    {
      Instruction *i = &(*I);
      if(dyn_cast<PHINode>(i))
	{
	  for (Value::use_iterator ii = i->use_begin(), ee = ii->use_end(); ii != ee; ++ii) {
	    Value *v = *ii;
	    if(dyn_cast<Instruction>(v))
	      {
		liveRanges[i].insert(v);
	      }
	  }
	}
      else
	{
	  liveRanges[i].insert(i);
	}
    }

}


void doLiveValues(Function &F,
		map<Value*, set<Value*> > &liveIn,
		map<Value*, set<Value*> > &liveOut 
		)
{
  bool changed=false;
  bool first = false;
  
  map<Value*, set<Value*> > definedSet;
  set<Value*> prevSet;

  for(Function::arg_iterator I = F.arg_begin(), E = F.arg_end();
      I != E; ++I)
    {
      Argument *a = I;
      prevSet.insert(a);
    }

  for (inst_iterator I = inst_begin(F), 
	 E = inst_end(F); I != E; ++I) 
    {
      Instruction *i = &(*I);
      prevSet.insert(i);
      definedSet[i] = prevSet;
    }


  do
    {
      first = true;
      changed = false;
      for (inst_iterator I = inst_begin(F), 
	     E = inst_end(F); I != E; ++I) {
	Instruction *i = &(*I);
	inst_iterator II = I;
	II++;

	PHINode *phi = dyn_cast<PHINode>(i);
	set<Value*> tempSet;
	set<Value*> killedSet;
	set<Value*> DiffSet;
	set<Value*> ResultSet;
	set<Value*> UseSet;
	
	BranchInst *br = dyn_cast<BranchInst>(i);
	if(br)
	  {
	    for(unsigned int j = 0; j < br->getNumSuccessors(); j++)
	      {
		Instruction *n = (br->getSuccessor(j)->begin());
	
		//errs() << "successor of " << *i << " = " << *n << "\n"; 
		if(dyn_cast<Instruction>(n) || dyn_cast<Argument>(n)) {
		  mergeSets(liveIn[n], UseSet);
		}
	      }
	  }
	
	if(II != E)
	  {
	    Instruction *ii = &(*II);
	    if(dyn_cast<Instruction>(ii) || dyn_cast<Argument>(ii)) {
	      mergeSets(liveIn[ii], UseSet);
	    }
	   
	  }

	//iterate over uses
	/*
	*/

	killedSet.insert(i);

	for(User::op_iterator op = i->op_begin(), ope=i->op_end(); op != ope; op++) {
	  if(dyn_cast<Instruction>(*op) || dyn_cast<Argument>(*op)) 
	    {
	      tempSet.insert(op->get());
	    }
	}
	
	set_difference(UseSet.begin(), UseSet.end(),
		       killedSet.begin(), killedSet.end(),
		       inserter(DiffSet, DiffSet.end())
		       );

	set_union(DiffSet.begin(), DiffSet.end(),
		  tempSet.begin(), tempSet.end(),
		  inserter(ResultSet, ResultSet.end())
		  );


	if(liveIn[i] != ResultSet) 
	  {
	    changed = true;

	    liveIn[i] = ResultSet;
	    //mergeSets(ResultSet, liveIn[i]);
	  }

	if(liveOut[i] != UseSet) 
	  {
	    changed = true;
	    // errs() << "UseSet size = " << UseSet.size() << "\n";
	    //mergeSets(UseSet, liveOut[i]);
	    
	    liveOut[i] = UseSet;
	  }	
	
	if(first==false)
	  {
	    Value *v = i;
	    errs() << "INST: " << *v << "\n";
	    for(set<Value*>::iterator In=liveIn[v].begin();
		In != liveIn[v].end(); In++)
	      {
		Value *vv = *In;
		errs() << "\tIN:" << *vv << "\n";
	      }
	    
	    for(set<Value*>::iterator Out=liveOut[v].begin();
		Out != liveOut[v].end(); Out++)
	      {
		Value *vv = *Out;
		errs() << "\tOUT:" << *vv << "\n";
	      }
	    first = true;
	  }
	

	//errs() << "Result = " << ResultSet.size() << "\n";
      }
    } while(changed);

  for (inst_iterator I = inst_begin(F), 
	 E = inst_end(F); I != E; ++I) 
    {
      Instruction *i = &(*I);
      removeUndefined(definedSet[i], liveOut[i]);
      removeUndefined(definedSet[i], liveIn[i]);
    }
 }


size_t maxLiveValues( llvm::Function &F,
		      std::map<llvm::Value*, std::set<llvm::Value*> > &liveIn,
		      std::map<llvm::Value*, std::set<llvm::Value*> > &liveOut)
{
  size_t maxLiveIn = 0;
  size_t maxLiveOut = 0;

  size_t maxVectorIn = 0;
  size_t maxVectorOut = 0;
  
  /* We handle vector registers and arguments separately from
   * scalar values
   *
   * 1) arguments are stored as parameters into the module, so
   * we don't need explicitly allocate storage for them
   *
   * 2) vector values are stored in a vector register file which
   * is disjoint from the primary scalar registers
   */

  //TODO: vector registers 

  for(map<Value*, set<Value*> >::iterator mit = liveIn.begin();
      mit != liveIn.end(); mit++)
    {
      size_t n = 0;
      size_t v = 0;
      for(set<Value*>::iterator sit = (mit->second).begin();
	  sit != (mit->second).end(); sit++)
	{
	  Value *V = *sit;
	  const Type *T = V->getType();

	  if(T->isVectorTy()) {
	    v++;
	  }
	  else 
	    {
	      if(!(dyn_cast<Argument>(V))) {
		n++;
	      }
	    }
	}
      if(n > maxLiveIn)
	maxLiveIn = n;
      if(v > maxVectorIn)
	maxVectorIn = v;
    }

  for(map<Value*, set<Value*> >::iterator mit = liveOut.begin();
      mit != liveOut.end(); mit++)
    {
      size_t n = 0;
      size_t v = 0;
      for(set<Value*>::iterator sit = (mit->second).begin();
	  sit != (mit->second).end(); sit++)
	{
	  Value *V = *sit;
	  const Type *T = V->getType();
	  if(T->isVectorTy()) {
	    v++;
	  }
	  else 
	    {
	      if(!(dyn_cast<Argument>(V))) {
		n++;
	      }
	    }
	}
      if(n > maxLiveOut)
	maxLiveOut = n;
      if( v > maxVectorOut)
	maxVectorOut = v;
    }

  size_t maxVector = (maxVectorIn > maxVectorOut) ? 
    maxVectorIn : maxVectorOut;
  
  errs() << "maxVector " << maxVector << "\n";

  return (maxLiveIn > maxLiveOut) ? maxLiveIn : maxLiveOut;
}


static void whatDies(set<Value*> &liveIn, set<Value*> &liveOut, set<Value*> &deadSet)
{
  for(set<Value*>::iterator sit = liveIn.begin();
      sit != liveIn.end(); sit++)
    {
      Value *V = *sit;
      
      /* Remember, arguments are handled 
       * differently */
      if(dyn_cast<Argument>(V))
	continue;
      
      /* He's dead Jim, we didn't find
       * a value live in in the live out 
       * set */
      if(liveOut.find(V) == liveOut.end()) {
	deadSet.insert(V);
      }
    }
}

void computePlaceOfDeath(std::map<llvm::Value*, std::set<llvm::Value*> > &liveIn,
			 std::map<llvm::Value*, std::set<llvm::Value*> > &liveOut,
			 std::map<llvm::Value*, std::set<llvm::Value*> > &diesAtValue)
{
  /* Sanity check */
    for(map<Value*, set<Value*> >::iterator mit = liveIn.begin();
      mit != liveIn.end(); mit++)
    {
      Value *V = mit->first;
      Instruction *ins = dyn_cast<Instruction>(V);
      assert(ins);

      /* PHIs are handled differently (aka weird) */
      if(ins->getOpcode()==Instruction::PHI)
	continue;

      for(User::op_iterator op = ins->op_begin(), ope=ins->op_end(); op != ope; op++) 
	{
	  Value *O = *op;
	  if(dyn_cast<Instruction>(O))
	    {
	      if(mit->second.find(O) == mit->second.end()) {
		errs() << *ins << " : didn't find argument in live in!\n";
		exit(-1);
	      }
	    }
	}
    }

  /* compute live ranges for each LLVM variable */
  for(map<Value*, set<Value*> >::iterator mit = liveIn.begin();
      mit != liveIn.end(); mit++)
    {
      set<Value*> deadSet;
      whatDies(mit->second, liveOut[mit->first], deadSet); 
      diesAtValue[mit->first] = deadSet;
      //errs() << deadSet.size() << " registers died\n";
    }
}
