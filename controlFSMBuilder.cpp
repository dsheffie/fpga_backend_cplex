#include "controlFSMBuilder.h"
#include "util.h"

using namespace std;
using namespace llvm;

controlFSM::controlFSM(Function *F, 
		       bool memCanStall,
		       systemParam *sP,
		       std::map<llvm::Value*, int> *regMap,
		       std::map<llvm::Value*, scalarArgument*> *sArgMap,
		       int numStates,
		       std::map<llvm::BasicBlock*, int> *blockStateMap,
		       std::map<int, std::list<llvm::Value* > > *statePlan)
{
  this->F = F;
  this->statePlan = statePlan;
  this->numStates = numStates;
  this->blockStateMap = blockStateMap;
  this->memCanStall = memCanStall;
  this->regMap = regMap;
  this->sArgMap = sArgMap;

  this->sP = sP;

  numBB = 0;
  /* calculate number of basic blocks in the function */
  for (Function::iterator i = F->begin(), e = F->end(); 
       i != e; ++i)
    {
      BasicBlock *BB = i;
      blockMap[BB] = numBB;
      numBB++;
    }

  /* record states where branches occur */
  for(map<int, std::list<llvm::Value*> >::iterator mit = statePlan->begin();
      mit != statePlan->end(); mit++)
    {
      for(list<Value*>::iterator lit = (mit->second).begin(); 
	  lit != (mit->second).end(); lit++)
	{
	  Value *V = *lit;
	  Instruction *ins = dyn_cast<Instruction>(V);
	  if(ins)
	    {
	      if(ins->getOpcode()==Instruction::Br)
		{
		  branchStates.insert(mit->first);
		}
	      else if(ins->getOpcode()==Instruction::Ret)
		{
		  retStates.insert(mit->first);
		}
	      else if(ins->getOpcode()==Instruction::Load ||
		      ins->getOpcode()==Instruction::Store)
		{
		  int startState = mit->first;
		  int retireState = startState + (sP->get_latency(V) - 1);
		  //errs() << "mem retires @ " << retireState << "\n";
		  memRetireStates.insert(retireState);
		}
	    }
	  else
	    {
	      errs() << "something very wrong here\n";
	      exit(-1);
	    }
	}
    }


  errs() << branchStates.size() << " branches in the program\n";


} 

int controlFSM::getBlockNum(llvm::BasicBlock *BB)
{
  if(blockMap.find(BB) == blockMap.end()) {
    errs() << "couldn't find block number for current block\n";
    exit(-1);
  }
  return blockMap[BB];
}

int controlFSM::getNumBasicBlocks()
{
  return numBB;
}


string controlFSM::asVerilog()
{
  string s;
  int numStateBits = (int)(lg2((size_t)numStates)+1);
   

  s = string("\n\nalways@(*)\n");
  s += string("begin\n");
  s += string("nstate = state;\n");
  s += string("t_done = 1'b0;\n");
  s += string("nbb = r_lbb;\n");

  s += string("case(state)\n");

  /* emit RTL for the idle state */
  s += int2string((1 << numStateBits)-1) + (":\n");
  s += string("\t nstate = start ? 'd0 : ")  + 
    int2string((1 << numStateBits)-1) + string(";\n");
  
  for(int i = 0; i < numStates; i++)
    {
      s += int2string(i) + string(":\n");
      s += string("begin\n");
      /* We have a branch in this state */
      if(branchStates.find(i) != branchStates.end()) {
	
	BranchInst *Br = 0;
	for(list<Value*>::iterator lit = ((*statePlan)[i]).begin();
	    lit != ((*statePlan)[i]).end(); lit++)
	  {
	    BranchInst* BrInst = dyn_cast<BranchInst>(*lit);
	    if(BrInst) {
	      Br = BrInst;
	      break;
	    }
	  }

	if(Br == 0) {
	  errs() << "couldn't find branch instruction at desired state!\n";
	  exit(-1);
	}

	BasicBlock *currBB = Br->getParent();
	if(blockMap.find(currBB) == blockMap.end()) {
	  errs() << "couldn't find block number for current block\n";
	  exit(-1);
	}

	int blkNum = blockMap[Br->getParent()];
	s += string("\t nbb = ") + int2string(blkNum) + 
	  string(";\n");

	errs() << "======>nbb: " << i << " jump to BB " << blkNum << "\n";
	
	if(Br->isConditional())
	  {
	    int numSuccessors = (int)Br->getNumSuccessors();
	    int takenState = -1, notTakenState = -1;
	    if(numSuccessors != 2)
	      {
		errs() << "numSuccessors = " << numSuccessors << "\n";
		exit(-1);
	      }
	    /* this matches my implement in c2rtl */
	    BasicBlock *takenBB = Br->getSuccessor(0);
	    BasicBlock *notTakenBB = Br->getSuccessor(1);
	    
	    if(blockStateMap->find(takenBB) == blockStateMap->end()) {
	      errs() << "takenBB not found in state map\n";
	      exit(-1);
	    }

	    if(blockStateMap->find(notTakenBB) == blockStateMap->end()) {
	      errs() << "not taken BB not found in state map\n";
	      exit(-1);
	    }

	    
	    takenState = (*blockStateMap)[takenBB];
	    notTakenState = (*blockStateMap)[notTakenBB];

	    s += string("\t nstate = w_take_branch ? ") + int2string(takenState) + 
	      string(" : ") + int2string(notTakenState) + string(";\n");
	  }
	else
	  {
	    BasicBlock *takenBB = Br->getSuccessor(0);
	    int takenState = -1;
	    if(blockStateMap->find(takenBB) == blockStateMap->end()) {
	      errs() << "takenBB not found in state map\n";
	      exit(-1);
	    }
	    takenState = (*blockStateMap)[takenBB];

	    errs() << "======>state: " << i << " jump to state " << takenState << "\n\n";

	    s += string("\t nstate = ") + int2string(takenState) + string(";\n");
	  }
	
      }
      else if(retStates.find(i) != retStates.end()) {
	s += string("\t nstate =") + int2string((1 << numStateBits)-1) 
	  + string(";\n");
	s += string("\t t_done = 1'b1;\n");
      }
      /* Handle memory stalls */
      else if((memRetireStates.find(i) != memRetireStates.end()) &&
	      memCanStall)
	{
	  s += string("\t nstate = mem_stall ? ") + int2string(i) + 
	    string(" : ") + int2string(i + 1) +
	    string(";\n");
	}
      else {
	s += string("\t nstate = ") + int2string(i + 1) +
	  string(";\n");
      }

      s += string("end\n");

    }



  s += string("endcase\n");
  s += string("end\n\n\n");
  return s;
}

void controlFSM::saveScheduleToDisk()
{
  string s;
  string fname = F->getNameStr() + string("_sch") + 
    string(".txt");
  FILE *fp = fopen(fname.c_str(), "w");

  for(map<int, list<Value*> >::iterator it = statePlan->begin();
      it != statePlan->end(); it++) 
    {
      s += "State " + int2string(it->first) + 
	string(" :\n");
      
      for(list<Value*>::iterator lit = (it->second).begin();
	  lit != (it->second).end(); lit++)
	{
	  Value *V = *lit;
	  Instruction *ins = dyn_cast<Instruction>(V);
	  //s += ins->getParent()->getName() + "\n";

	  if(regMap->find(V) != regMap->end())
	    {
	      s += string("\t") + int2string((*regMap)[V]) + string(" <- ") +
		string(ins->getOpcodeName()) + string("( ");
	    }
	  else
	    {
	      s += string("\t") + string(ins->getOpcodeName()) + 
		    string("( ");

	      if(ins->getOpcode() == Instruction::Br)
		{
		  BranchInst* Br = dyn_cast<BranchInst>(ins);
		  
		  if(Br->isConditional()) {
		    BasicBlock *takenBB = Br->getSuccessor(0);
		    BasicBlock *notTakenBB = Br->getSuccessor(1);
		    int takenState = (*blockStateMap)[takenBB];
		    int notTakenState = (*blockStateMap)[notTakenBB];

		    s += string("taken=") + int2string(takenState) + string("  ");
		    s += string("ntaken=") + int2string(notTakenState) +string("  ");

		  } else {
		    BasicBlock *takenBB = Br->getSuccessor(0);
		    int takenState = (*blockStateMap)[takenBB];
		    s += string("taken=") + int2string(takenState) + string("  ");
		  }
		}
	      
	
	
	    }

	  for(int i = 0; i < ins->getNumOperands(); i++)
	    {
	      Value *O = ins->getOperand(i);
	      Instruction *oIns = dyn_cast<Instruction>(O);
	      if(regMap->find(oIns) != regMap->end())
		{
		  s += int2string((*regMap)[oIns]) + 
		    string("  ");
		}
	      else if(sArgMap->find(O) != sArgMap->end())
		{
		  s += ((*sArgMap)[O])->getName() + 
		    string(" ");
		}
	    }
	  s+=string(" )\n");
	}
    }


  s += "\n\n\n";

  for(std::map<BasicBlock*, int>::iterator mit = blockMap.begin();
      mit != blockMap.end(); mit++)
    {
      errs() << "block " << mit->second << " : " << (mit->first)->getName() << "\n";
      //s += int2string(
    }

  
  fprintf(fp, "%s", s.c_str());
  fclose(fp);
}
