#include "graph.h"
#include "plugin.h"
#include <algorithm>

bool isBranchInst(ZydisDecodedInstruction g_inst)
{
	if (g_inst.meta.category == ZYDIS_CATEGORY_UNCOND_BR ||
		g_inst.meta.category == ZYDIS_CATEGORY_COND_BR ||
		g_inst.meta.category == ZYDIS_CATEGORY_RET)
	{
		return true;
	}

	return false;
}

DWORD getBranchTarget(ZydisDecodedInstruction g_inst, ZydisDecodedOperand* operandPtr, std::uintptr_t _offset)
{
	ZyanU64 rst = 0;
	if (isBranchInst(g_inst))
	{
		if (operandPtr[0].imm.is_relative)
			ZydisCalcAbsoluteAddress(&g_inst, &operandPtr[0], _offset, &rst);
	}
	return rst;
}

bool isUncondBranch(ZydisDecodedInstruction g_inst)
{
	if (g_inst.mnemonic == ZYDIS_MNEMONIC_JMP ||
		g_inst.mnemonic == ZYDIS_MNEMONIC_RET)
		return true;
	return false;
}

void CreateCFG(DWORD _address)
{
	DWORD offset = _address;
	ZydisDecodedInstruction g_inst;
	ZydisDecoder decoder;
	ZydisFormatter formatter;
	ZydisDecodedOperand DecodedOperand[5];
	DWORD length = 0;
	DWORD branchTarget = 0;
	std::queue<DWORD> blockQueue;
	std::map<DWORD, BOOL> seen;
	char debugBuf[128] = { 0, };
	char buffer[256];

	BridgeCFGraph2* graphPtr = new BridgeCFGraph2(offset);
	BridgeCFNode2* nodePtr;
	BYTE instBuf[64];

	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);

	blockQueue.push(offset);

	DWORD blockIndex = 0;

	std::set<DWORD> alreadyInsertedStart;

	while (!blockQueue.empty())
	{
		offset = blockQueue.front();
		blockQueue.pop();

		if (seen[offset])
		{
			continue;
		}

		seen[offset] = true;

		nodePtr = new BridgeCFNode2;
		nodePtr->start = nodePtr->end = offset;
		nodePtr->vertex = blockIndex;
		nodePtr->terminal = false;
		blockIndex++;

		_plugin_logprintf("Create Node %p\n", nodePtr->start);
		
		DbgMemRead(offset, instBuf, 15);
		while (1)
		{
			ZydisDecoderDecodeFull(&decoder, (BYTE*)instBuf, 15, &g_inst, DecodedOperand,
				ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);

			nodePtr->eips.insert(offset);

			if (isBranchInst(g_inst))
			{
				branchTarget = getBranchTarget(g_inst, DecodedOperand, offset);

				if (branchTarget && g_inst.meta.category != ZYDIS_CATEGORY_RET)//&& !seen[branchTarget])
				{
					nodePtr->brtrue = branchTarget;
					blockQueue.push(branchTarget);
				}

				if (isUncondBranch(g_inst))
				{
					if (g_inst.mnemonic == ZYDIS_MNEMONIC_RET)
						nodePtr->terminal = true;
					else if (g_inst.mnemonic == ZYDIS_MNEMONIC_JMP && DecodedOperand[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
						nodePtr->terminal = true;
					graphPtr->AddNode(*nodePtr);
					break;
				}
				/*
				else if (g_inst.mnemonic == ZYDIS_MNEMONIC_INT3)
				{
					graphPtr->AddNode(*nodePtr);
					nodePtr->terminal = true;
					break;
				}
				*/
				else if (g_inst.meta.category == ZYDIS_CATEGORY_COND_BR)
				{
					nodePtr->brfalse = offset + g_inst.length;
					graphPtr->AddNode(*nodePtr);
					blockQueue.push(offset + g_inst.length);
					break;
				}

			}
			nodePtr->end += g_inst.length;
			offset += g_inst.length;
			DbgMemRead(offset, instBuf, 15);
		}
	}

	// Split overlapping blocks
	for (auto& it : graphPtr->nodes)
	{
		auto& node = it.second;
		DWORD addr = node.start;
		DWORD size = 0;
		DWORD icount = 0;
		DbgMemRead(addr, instBuf, 15);

		while (addr < node.end)
		{
			icount++;
			ZydisDecoderDecodeFull(&decoder, instBuf, 15, &g_inst, DecodedOperand,
				ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);
			size = g_inst.length;

			if (graphPtr->nodes.count(addr + size))
			{
				node.end = addr;
				node.brtrue = addr + size;
				node.brfalse = 0;
				break;
			}
			addr += size;
			DbgMemRead(addr, instBuf, 15);
		}
	}

	// 그래프 연결 관계를 표현하기 위한 adj 2차원 배열 할당
	graphPtr->adj = (DWORD**)malloc((sizeof(DWORD*) * (graphPtr->nodes.size())));
	for (int i = 0; i < graphPtr->nodes.size(); i++)
	{
		graphPtr->adj[i] = (DWORD*)malloc(sizeof(DWORD) * graphPtr->nodes.size());

		/*
		for (int j = 0; j < graphPtr->nodes.size(); j++)
		{
			graphPtr->adj[i][j] = 0;
		}
		*/
	}
#ifdef DEBUG
	sprintf(debugBuf, "malloc graphPtr->nodes.size() %d\n", graphPtr->nodes.size());
	OutputDebugStringA(debugBuf);
#endif
	graphPtr->dfsCFG();

	//for (int i = 0; i < graphPtr->nodes.size(); i++)
	//{
	//	graphPtr->getPredecessor(i);
	//}

	//graphPtr->domTree = new BridgeDominatorGraph(graphPtr->nodeNum);
#ifdef DEBUG
	sprintf(debugBuf, "graphPtr->domTree = new BridgeDominatorGraph\n");
	OutputDebugStringA(debugBuf);
#endif

	//graphPtr->domTree->Dominators(*graphPtr);
#ifdef DEBUG
	sprintf(debugBuf, "graphPtr->domTree->Dominators\n");
	OutputDebugStringA(debugBuf);
#endif
	//graphPtr->domTree->CreateAdj();


	/*
	for (int i = 0; i < graphPtr->nodes.size(); i++)
	{
		graphPtr->domTree->GetChildren(i);
	}

	graphPtr->domTree->computeDF(*graphPtr, 0);

	graphPtr->domTree->PrintDF(*graphPtr, graphPtr->nodes.size());
	graphPtr->domTree->PrintDfnum(*graphPtr->domTree);
	graphPtr->domTree->InsertPHI(*graphPtr, false);
	graphPtr->RenameInit();
	std::set<DWORD> isVisited;
	graphPtr->Rename(graphPtr->entryPoint, *graphPtr->domTree, false, isVisited);
	graphPtr->printIRList();
	*/

	_plugin_logprintf("-------------------------------------\n");
	for (auto it : graphPtr->nodes)
	{
		//char buf[128];
		_plugin_logprintf("%p %p\n", it.second.start, it.second.end);
		//OutputDebugStringA(buf);
	}
	_plugin_logprintf("-------------------------------------\n");

	free(graphPtr->adj);
}

void BridgeCFGraph2::DFS(BridgeCFNode2* _node, std::set<DWORD>& _visited)
{
	char debugBuf[64] = { 0, };
	_visited.insert(_node->start);
	vertexToBridgeCFNodePtr.insert(std::make_pair(_node->vertex, _node));
	nodeNum++;

	// Todo : Basic Block별 IR 생성 코드 추가
	// 현재 Node에 포함된 x86 명령어들을 디코드하여 IR을 생성한다.
	// ..
	//ZydisDecodedInstruction inst;
	//ZydisDecoder decoder;
	//ZydisDecodedOperand DecodedOperand[5];

	DWORD eip = _node->start;
	DWORD instCnt = 0;
	DWORD currentStack = _node->stackStart;

	//ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);
	DWORD nodeStartinIr = _node->start;

	/*
	while (1)
	{
		if (eip > _node->end)
			break;

		ZydisDecoderDecodeFull(&decoder, (BYTE*)eip, 15, &inst, DecodedOperand,
			ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);

		//_node->CreateIR(&inst, DecodedOperand, eip, currentStack, *this);
		//printf("[%p] Stack Address :%p\n", eip, currentStack);
		eip = eip + inst.length;
		instCnt++;
	}
	*/

	/*
	for (int i = REG_EAX_16H; i < REG_EDI_8L + 1; i++)
	{
		if (_node->RegValue[i].size() > 1)
		{
			//printf("Node(%p) defines %s\n", _node->start, ValueIndextoString(i).c_str());
			defsites[i].insert(_node->vertex);
			_node->Aorigin.insert(i);
		}
	}
	*/

	//printf("Node(%p) IRList size :%d Aorigin size :%d instCnt:%d\n", _node->start, _node->IRList.size(), _node->Aorigin.size(), instCnt);

	// brtrue와 brfalse가 가리키는 노드가 _node의 successor가 되겠다.
	if (_node->brtrue)
	{
		DWORD nextBB = _node->brtrue;

		if (nodes.find(nextBB) != nodes.end())
		{
			adj[_node->vertex][nodes[nextBB].vertex] = 1;
			predecessors[nodes[nextBB].vertex].insert(_node->vertex);
#ifdef DEBUG
			sprintf(debugBuf, "adj[%d][%d] [%p][%p]\n", _node->vertex, nodes[nextBB].vertex, _node->start, nodes[nextBB].start);
			OutputDebugStringA(debugBuf);
#endif
			if (_visited.find(nextBB) == _visited.end())
			{
				nodes[nextBB].stackStart = currentStack;
				/*
				_node->MemValue.insert(_node->MemValue.begin(), _node->MemValue.end());
				for (int i = 0; i < 8; i++)
				{
					nodes[nextBB].RegValue[(3 * i) + 0].push_back(_node->RegValue[(3 * i) + 0].back());
					nodes[nextBB].RegValue[(3 * i) + 1].push_back(_node->RegValue[(3 * i) + 1].back());
					nodes[nextBB].RegValue[(3 * i) + 2].push_back(_node->RegValue[(3 * i) + 2].back());
				}
				*/
				DFS(&nodes[nextBB], _visited);
			}
		}
	}

	if (_node->brfalse)
	{
		DWORD nextBB = _node->brfalse;

		if (nodes.find(nextBB) != nodes.end())
		{
			adj[_node->vertex][nodes[nextBB].vertex] = 1;
			predecessors[nodes[nextBB].vertex].insert(_node->vertex);
#ifdef DEBUG
			sprintf(debugBuf, "adj[%d][%d] [%p][%p]\n", _node->vertex, nodes[nextBB].vertex, _node->start, nodes[nextBB].start);
			OutputDebugStringA(debugBuf);
#endif
			if (_visited.find(nextBB) == _visited.end())
			{
				nodes[nextBB].stackStart = currentStack;
				/*
				_node->MemValue.insert(_node->MemValue.begin(), _node->MemValue.end());
				for (int i = 0; i < 8; i++)
				{
					nodes[nextBB].RegValue[(3 * i) + 0].push_back(_node->RegValue[(3 * i) + 0].back());
					nodes[nextBB].RegValue[(3 * i) + 1].push_back(_node->RegValue[(3 * i) + 1].back());
					nodes[nextBB].RegValue[(3 * i) + 2].push_back(_node->RegValue[(3 * i) + 2].back());
				}
				*/
				DFS(&nodes[nextBB], _visited);
			}
		}
	}
}

void BridgeCFGraph2::dfsCFG()
{
	std::set<DWORD> visited;
	// brtrue 부터 방문
	// brfalse가 존재하면 stack에 저장
	// 방문한 노드는 visited에 저장
	// br
	if (nodes.find(entryPoint) != nodes.end())
	{
		/*
		initRegisterValue(nodes[entryPoint].start);
		for (int i = 0; i < 8; i++)
		{
			nodes[entryPoint].RegValue[(3 * i) + 0].push_back(CFGRegInitValue[(3 * i) + 0]);
			nodes[entryPoint].RegValue[(3 * i) + 1].push_back(CFGRegInitValue[(3 * i) + 1]);
			nodes[entryPoint].RegValue[(3 * i) + 2].push_back(CFGRegInitValue[(3 * i) + 2]);
		}
		*/

		nodes[entryPoint].stackStart = 0x90000000;
		if (visited.find(entryPoint) == visited.end())
			DFS(&nodes[entryPoint], visited);
	}
}

bool BridgeCFGraph2::findMergableBlock(DWORD vertex)
{
	vertexToBridgeCFNodePtr[vertex]->brtrue;
	vertexToBridgeCFNodePtr[vertex]->brfalse;
}