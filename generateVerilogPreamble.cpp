#include "generateVerilogPreamble.h"

using namespace llvm;
using namespace std;

static string genMemPortProto(int num)
{
  string s;
  string n = int2string(num);

  s += string("output [31:0] addr") + n;
  s += string(",output [31:0] dout") + n;
  s += string(",input [31:0] din") + n;
  s += string(",input mem_stall") + n;
  s += string(",output is_st") + n;
  s += string(",output mem_valid") + n;
  s += string(",");
  return s;
}

static string genMemPortTemps(int num)
{
  string s;
  string n = int2string(num);
  s += string("reg [31:0] r_addr") + 
    n + string(";\n");
  s += string("wire [31:0] w_addr") + 
    n + string(";\n");
  s += string("reg [31:0] r_dout") + 
    n + string(";\n");
  s += string("wire [31:0] w_dout") + 
    n +  string(";\n");
  s += string("reg r_is_st") + 
    n + string(";\n");
  s += string("wire w_is_st") + 
    n + string(";\n");
  s += string("reg r_mem_valid") + 
    n + string(";\n");
  s += string("wire w_mem_valid") + 
    n + string(";\n");
  s += string("assign addr") + n + string(" = r_addr") + 
    n + string(";\n");
  s += string("assign dout") + n + string(" = r_dout") + 
    n + string(";\n");
  s += string("assign is_st") + n + string(" = r_is_st") + 
    n + string(";\n");
  s += string("assign mem_valid") + n + string(" = r_mem_valid") + 
    n + string(";\n");
 
  return s;
}

static string genMemPortFlipFlops(int num)
{
  string s;
  string n = int2string(num);
  s += string("r_addr") + n + string(" <= rst ? 32'd0") + 
    string(": w_addr") + n + string(";\n");
 
  s += string("r_dout") + n + string(" <= rst ? 32'd0") +
    string(": w_dout") + n + string(";\n");

  s += string("r_is_st") + n + string(" <= rst ? 1'b0") +
    string(": w_is_st") + n + string(";\n");

  s += string("r_mem_valid") + n + string(" <= rst ? 1'b0") +
    string(": w_mem_valid") + n + string(";\n");

  return s;
}

static string genMemStall(int numMemPorts)
{
  string s;
  s += string("wire mem_stall = ");
  
  for(int i = 0; i < numMemPorts; i++)
    {
      string n = int2string(i);
      s += string("mem_stall") + n;
      if(i != (numMemPorts-1))
	{
	  s += string("|");
	}
    }

  s += string(";\n");
  return s;
}


string emitRTLPreamble(Function &F, 
		       int numStates,
		       int numBasicBlocks,
		       systemParam *sP,
		       map<Value*, scalarArgument*> &sArgMap)
{
  int numStateBits = (int)(lg2((size_t)numStates)+1);
  int numBlockBits = (int)(lg2((size_t)numBasicBlocks)+1);

  int numMemPorts = sP->get_count(MEM);

  string s = string("module ") +
    F.getNameStr() + string("_ilp(");

  s += string("input clk, input rst, input start,");

  int numArgs = (int)sArgMap.size();
  for(map<Value*, scalarArgument*>::iterator mit = sArgMap.begin();
      mit != sArgMap.end(); mit++)
    {
      s += "input [31:0] " + (mit->second)->getName();
      s += string(",");
    }

  
  for(int i = 0; i < numMemPorts; i++)
    {
      /* These signals vary given the number of memory ports */
      s += genMemPortProto(i);
    }

  s += string("output done, output [31:0] ret");  
  s += string(");\n");

  /* state and nstate registers */
  s += string("reg [") + int2string(numStateBits) + 
    string(":0] state,nstate;\n");
  s += string("reg r_done, t_done;\n");

 for(int i = 0; i < numMemPorts; i++)
    {
      s += genMemPortTemps(i);
    }

 s +=genMemStall(numMemPorts);

  s += string("reg [31:0] r_ret;\n");
  s += string("wire [31:0] w_ret;\n");
  s += string("wire w_take_branch;\n");

  /* last block register */
  s += string("reg [") + int2string(numBlockBits) + 
    string(":0] r_lbb, nbb;\n\n");

  s += string("assign done = r_done;\n");
  s += string("assign ret = r_ret;\n");



  s += string("\n\n");

  s += string("always@(posedge clk)\n");
  s += string("begin\n");
  s += string("state <= rst ? ") + int2string((1 << numStateBits) - 1) + 
    string(" : nstate;\n");
  
  s += string("r_done <= rst ? 1'b0 : t_done;\n");
  s += string("r_ret <= rst ? 32'd0 : w_ret;\n");
  s += string("r_lbb <= rst ? ") + int2string((1 << numBlockBits) - 1) + 
    string(" : nbb;\n");

  for(int i = 0; i < numMemPorts; i++)
    {
      s += genMemPortFlipFlops(i);
    }

  s += string("end\n");

  s += string("\n\n");

  return s;
}
