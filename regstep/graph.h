#pragma once

#include <Windows.h>
#include <set>
#include <map>
#include <queue>
#include <list>

struct BridgeCFGraph2;

typedef struct
{
	DWORD parentGraph; //function of which this node is a part
	DWORD start; //start of the block
	DWORD end; //end of the block (inclusive)
	DWORD brtrue; //destination if condition is true
	DWORD brfalse; //destination if condition is false
	DWORD icount; //number of instructions in node
	bool terminal; //node is a RET
	bool split; //node is a split (brtrue points to the next node)
	std::list<DWORD> exits; //exits (including brtrue and brfalse, DWORD)
	std::list<DWORD> eips; //containted instruction eip list
} BridgeCFNodeList2;

typedef struct
{
	DWORD entryPoint; //graph entry point
	void* userdata; //user data
	std::list<BridgeCFNodeList2> nodes; //graph nodes (BridgeCFNodeList)
} BridgeCFGraphList2;

#ifdef __cplusplus
#if _MSC_VER > 1300

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>
#include <stack>
#include <Zycore/LibC.h>
#include <Zydis/Zydis.h>


struct PhiInfo2
{
	DWORD nodeIdx;
	DWORD argCount;
};

struct BridgeCFNode2
{
	DWORD vertex;
	DWORD parentGraph; //function of which this node is a part
	DWORD start; //start of the block
	DWORD end; //end of the block (inclusive)
	DWORD brtrue; //destination if condition is true
	DWORD brfalse; //destination if condition is false
	DWORD icount; //number of instructions in node
	bool terminal; //node is a RET
	bool split; //node is a split (brtrue points to the next node)
	void* userdata; //user data
	std::vector<DWORD> exits; //exits (including brtrue and brfalse)
	std::vector<unsigned char> data; //block data
	std::set<DWORD> eips;
	std::set<DWORD> Aorigin; // 해당 블록에서 정의된 변수들의 집합

	//std::vector<Value*> StatusFlagsValue[5];
	//std::vector<Value*> RegValue[REG_EDI_8L + 1];
	//std::map<DWORD, vector<Value*>> MemValue;
	//std::map<DWORD, vector<IR*>> IRList;
	//Value* lastValue[REG_EDI_8L + 1];

	DWORD stackStart;

	BridgeCFNodeList2 ToNodeList() const
	{
		BridgeCFNodeList2 out;
		out.parentGraph = parentGraph;
		out.start = start;
		out.end = end;
		out.brtrue = brtrue;
		out.brfalse = brfalse;
		out.icount = icount;
		out.terminal = terminal;
		out.split = split;
		return std::move(out);
	}
	//int CreateIR(ZydisDecodedInstruction* ptr_di, ZydisDecodedOperand* operandPTr, DWORD _offset, DWORD& _stackStart, BridgeCFGraph& _graph);
};

struct BridgeCFGraph2
{
	DWORD entryPoint; //graph entry point
	std::unordered_map<DWORD, BridgeCFNode2> nodes; //CFNode.start -> CFNode
	std::map<DWORD, BridgeCFNode2*> vertexToBridgeCFNodePtr; //BridgeCFNode to CFNode
	std::unordered_map<DWORD, std::unordered_set<DWORD>> parents; //CFNode.start -> parents
	//BridgeDominatorGraph* domTree;
	DWORD nodeNum;
	DWORD** adj;

	std::set<DWORD> predecessors[500];
	//std::set<DWORD> defsites[REG_EDI_8L + 1];
	//set<vector<DWORD>> defsitesd;
	//std::set<DWORD> toInsertPhiNodeSet[REG_EDI_8L + 1];
	//std::map<DWORD, PhiInfo> phiInfo[REG_EDI_8L + 1];

	//DWORD Count[REG_EDI_8L + 1];
	//std::stack<DWORD> NumStack[REG_EDI_8L + 1];
	//std::stack<Value*> ValuePtrStack[REG_EDI_8L + 1];

	//std::vector<Value*> CFGRegValue[REG_EDI_8L + 1];
	//Value* CFGRegInitValue[REG_EDI_8L + 1];



	explicit BridgeCFGraph2(DWORD entryPoint)
		: entryPoint(entryPoint)
	{
		nodeNum = 0;
	}

	void AddNode(const BridgeCFNode2& node)
	{
		nodes[node.start] = node;
		AddParent(node.start, node.brtrue);
		AddParent(node.start, node.brfalse);
	}

	void AddParent(DWORD child, DWORD parent)
	{
		if (!child || !parent)
			return;
		auto found = parents.find(child);
		if (found == parents.end())
			parents[child] = std::unordered_set<DWORD>(std::initializer_list<DWORD> { parent });
		else
			found->second.insert(parent);
	}
	void DFS(BridgeCFNode2* _node, std::set<DWORD>& _visited);

	void dfsCFG();

	void getPredecessor(int n)
	{
		char buf[32] = { 0, };
		for (int i = 0; i < nodes.size(); i++)
		{
			//for (int j = 0; j < nodes.size(); j++)
			{
				if (adj[i][n] == 1)
				{
					predecessors[n].insert(i);
				}
			}
		}
	}

	bool isMergeable(BridgeCFNode2 a, BridgeCFNode2 b)
	{
		bool rst = false;
		char buf[32] = { 0, };
		// 노드 병합 조건 : a 노드의 Edge가 1개, b노드의 Predecessor 노드가 1개
		if (a.brfalse == 0)
		{
			if (predecessors[b.vertex].size() == 1)
			{
				rst = true;
			}
		}
		return rst;
	}
	bool findMergableBlock(DWORD vertex);
	//void RenameInit();
	//void Rename(DWORD n, BridgeDominatorGraph& _domGraph, bool isDebug, std::set<DWORD>& isVisited);
	//void printIRList();
};


#endif //_MSC_VER
#endif //__cplusplus