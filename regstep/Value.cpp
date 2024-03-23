#include "Value.h"
#include <map>

extern vector<Value*> StatusFlagsValue[5];
extern vector<Value*> RegValue[ZYDIS_REGISTER_MAX_VALUE];
extern map<DWORD, vector<Value*>> MemValue;
extern multimap<DWORD, vector<IR*>> IRList;

extern z3::context* z3Context;
extern z3::expr* z3Equation;
extern map<string, z3::expr*> symbolExprMap;

map<DWORD, IR*> vaddList;

DWORD tmpNum = 0;
Value* GetImmValue(DWORD _immValue, BYTE _size, vector<IR*>& irList);
ULONG_PTR GetRegisterValueFromRegdump(ZydisRegister _zydisRegIdx, REGDUMP& regdump);

UCHAR targetByte[] = { 0x71,0x68,0x63,0x70,0x67,0x62,0x70,0x75,0x77,0x62,0x61,0x67,0x67,0x65,0x70,0x75,0x6C,0x68,0x73,0x74,0x78,0x62,0x77,0x6F,0x77,0x61,0x77,0x66,0x67,0x72,0x6B,0x7A,0x6A,0x73,0x74,0x63,0x63,0x62,0x6E,0x62,0x73,0x68,0x65,0x6B,0x70,0x67,0x6C,0x6C,0x7A,0x65 };
DWORD g_idx = 0;

bool isLastRegValue(Value* temIrPtr)
{
	bool rst = true;
	if (temIrPtr != RegValue[REG_EAX_16H].back() && temIrPtr != RegValue[REG_EAX_8H].back() && temIrPtr != RegValue[REG_EAX_8L].back() &&
		temIrPtr != RegValue[REG_EBX_16H].back() && temIrPtr != RegValue[REG_EBX_8H].back() && temIrPtr != RegValue[REG_EBX_8L].back() &&
		temIrPtr != RegValue[REG_ECX_16H].back() && temIrPtr != RegValue[REG_ECX_8H].back() && temIrPtr != RegValue[REG_ECX_8L].back() &&
		temIrPtr != RegValue[REG_EDX_16H].back() && temIrPtr != RegValue[REG_EDX_8H].back() && temIrPtr != RegValue[REG_EDX_8L].back() &&
		temIrPtr != RegValue[REG_EDI_16H].back() && temIrPtr != RegValue[REG_EDI_8H].back() && temIrPtr != RegValue[REG_EDI_8L].back() &&
		temIrPtr != RegValue[REG_ESI_16H].back() && temIrPtr != RegValue[REG_ESI_8H].back() && temIrPtr != RegValue[REG_ESI_8L].back() &&
		temIrPtr != RegValue[REG_EBP_16H].back() && temIrPtr != RegValue[REG_EBP_8H].back() && temIrPtr != RegValue[REG_EBP_8L].back() &&
		temIrPtr != RegValue[REG_ESP_16H].back() && temIrPtr != RegValue[REG_ESP_8H].back() && temIrPtr != RegValue[REG_ESP_8L].back())
		rst = false;
	return rst;
}

Value::Value()
{
	Name = "t" + std::to_string(tmpNum);
	//printf("Value Name :%s\n", Name.c_str());
	tmpNum++;
}

Value::Value(DWORD val, WORD size)
{
	OperandType = OPERANDTYPE_CONSTANT;
	Size = size;
	Name = "t" + std::to_string(tmpNum);
	//printf("Value Name :%s\n", Name.c_str());
	tmpNum++;
}

Value::Value(string ValName, DWORD idx)
{
	Name = ValName + "(" + std::to_string(idx) + ")";
	//printf("Value Name :%s\n", Name.c_str());
}

Value::Value(string ValName, DWORD idx, BOOL _isTainted)
{
	isTainted = _isTainted;
	Name = ValName + "(" + std::to_string(idx) + ")";
	//printf("Value Name :%s\n", Name.c_str());
}

IR::IR(OPR _opr, Value* op1)
{
	opr = _opr;
	AddOperand(op1);

	if (op1->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 1\n", cntd);
	}

	/*
	if (_opr == OPR_EXTRACT16H && dynamic_cast<IR*>(op1))
	{
		if (dynamic_cast<IR*>(op1)->opr == OPR_CONCAT)
		{
			_plugin_logprintf("[%p] Concat 1 Optimized\n", cntd);
		}
	}
	*/
	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + 1;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);
	CreateZ3Expr(this);
}

IR::IR(string ValName, DWORD index, OPR _opr, Value* op1) :Value(ValName, index, op1->isTainted)
{
	opr = _opr;
	AddOperand(op1);

	if (op1->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 1\n", cntd);
	}

	CreateZ3Expr(this);

	/*
	if (_opr == OPR_EXTRACT16H && dynamic_cast<IR*>(op1))
	{
		if (dynamic_cast<IR*>(op1)->opr == OPR_CONCAT)
		{
			if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
			{
				if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->opr == OPR_EXTRACT16H)
				{
					ast = dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->Operands[0]->valuePtr->ast;
					ast->childrenCount = dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->Operands[0]->valuePtr->ast->childrenCount;
					//_plugin_logprintf("[%p] Concat 1 OPR_EXTRACT16H Optimized\n", cntd);
					return;
				}
			}
		}
	}


	if (_opr == OPR_EXTRACT8H && dynamic_cast<IR*>(op1))
	{
		if ((dynamic_cast<IR*>(op1)->Operands.size() >1) &&dynamic_cast<IR*>(op1)->opr == OPR_CONCAT)
		{
			if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[1]->valuePtr))
			{
				if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[1]->valuePtr)->opr == OPR_EXTRACT8H)
				{
					ast = dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[1]->valuePtr)->Operands[0]->valuePtr->ast;
					ast->childrenCount = dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[1]->valuePtr)->Operands[0]->valuePtr->ast->childrenCount;
					//_plugin_logprintf("[%p] Concat 1 OPR_EXTRACT8H Optimized\n", cntd);
					return;
				}
			}
		}
	}

	if (_opr == OPR_EXTRACT8L && dynamic_cast<IR*>(op1))
	{
		if ((dynamic_cast<IR*>(op1)->Operands.size() > 2) && dynamic_cast<IR*>(op1)->opr == OPR_CONCAT)
		{
			if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[2]->valuePtr))
			{
				if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[2]->valuePtr)->opr == OPR_EXTRACT8L)
				{
					ast = dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[2]->valuePtr)->Operands[0]->valuePtr->ast;
					ast->childrenCount = dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[2]->valuePtr)->Operands[0]->valuePtr->ast->childrenCount;
					//_plugin_logprintf("[%p] Concat 1 OPR_EXTRACT8L Optimized\n", cntd);
					return;
				}
			}
		}
	}
	*/

	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + 1;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);
}

IR::IR(OPR _opr, Value* op1, Value* op2)
{
	if (op1->Size != op2->Size)
	{
		//_plugin_logprintf("[%p] Operand size is mismatching (%d %d) %s\n", cntd, op1->Size, op2->Size, printOpr(_opr).c_str());
	}
	opr = _opr;
	AddOperand(op1);
	AddOperand(op2);

	if (op1->isTainted || op2->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags || op2->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT && op2->OperandType == IR::OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 2\n", cntd);
	}

	Size = op1->Size;
	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + op2->ast->childrenCount + 2;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);

	std::shared_ptr<BTreeNode>  ptr2 = std::make_shared<BTreeNode>(MakeBTreeNode(op2->Name));
	MakeRightSubTree(ast, op2->ast);

	CreateZ3Expr(this);
}

IR::IR(OPR _opr, Value* op1, Value* op2, BYTE size)
{
	opr = _opr;
	Size = size;
	AddOperand(op1);
	AddOperand(op2);

	if (op1->isTainted || op2->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags || op2->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT && op2->OperandType == IR::OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 2\n", cntd);
	}

	CreateZ3Expr(this);

	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + op2->ast->childrenCount + 2;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);

	std::shared_ptr<BTreeNode>  ptr2 = std::make_shared<BTreeNode>(MakeBTreeNode(op2->Name));
	MakeRightSubTree(ast, op2->ast);
}

IR::IR(string ValName, DWORD index, OPR _opr, Value* op1, Value* op2)
{
	opr = _opr;
	AddOperand(op1);
	AddOperand(op2);

	if (op1->isTainted || op2->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags || op2->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT && op2->OperandType == IR::OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 2\n", cntd);
	}

	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + op2->ast->childrenCount + 2;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);

	std::shared_ptr<BTreeNode>  ptr2 = std::make_shared<BTreeNode>(MakeBTreeNode(op2->Name));
	MakeRightSubTree(ast, op2->ast);

	CreateZ3Expr(this);

	Value(ValName, index);
}

IR::IR(OPR _opr, Value* op1, Value* op2, Value* op3)
{
	opr = _opr;
	Size = op1->Size;
	AddOperand(op1);
	AddOperand(op2);
	AddOperand(op3);

	if (op1->isTainted || op2->isTainted || op3->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags || op2->isEflags || op3->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT && op2->OperandType == IR::OPERANDTYPE_CONSTANT && op3->OperandType == IR::OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 3\n", cntd);
	}

	CreateZ3Expr(this);

	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + op2->ast->childrenCount + op3->ast->childrenCount + 3;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);

	std::shared_ptr<BTreeNode>  ptr2 = std::make_shared<BTreeNode>(MakeBTreeNode(op2->Name));
	MakeRightSubTree(ast, op2->ast);

	std::shared_ptr<BTreeNode>  ptr3 = std::make_shared<BTreeNode>(MakeBTreeNode(op3->Name));
	MakeThirdSubTree(ast, op3->ast);
}

IR::IR(OPR _opr, Value* op1, Value* op2, Value* op3, Value* op4)
{
	opr = _opr;
	AddOperand(op1);
	AddOperand(op2);
	AddOperand(op3);
	AddOperand(op4);

	if (op1->isTainted || op2->isTainted || op3->isTainted || op4->isTainted)
	{
		isTainted = true;
	}

	if (op1->isEflags || op2->isEflags || op3->isEflags || op4->isEflags)
	{
		isEflags = true;
	}

	if (op1->OperandType == OPERANDTYPE_CONSTANT && op2->OperandType == IR::OPERANDTYPE_CONSTANT && op3->OperandType == IR::OPERANDTYPE_CONSTANT && op4->OperandType == IR::OPERANDTYPE_CONSTANT)
	{
		//_plugin_logprintf("[%p] Constant Folding 4\n", cntd);
	}

	CreateZ3Expr(this);

	/*
	if (_opr == OPR_CONCAT && dynamic_cast<IR*>(op1))
	{
		Value* val1 = nullptr;
		Value* val2 = nullptr;
		Value* val3 = nullptr;
		Value* val4 = nullptr;

		printIR(dynamic_cast<IR*>(op1));

		if (dynamic_cast<IR*>(op1)->opr == OPR_EXTRACT8HH)
		{
			val1 = dynamic_cast<IR*>(op1)->Operands[0]->valuePtr;
			if (dynamic_cast<IR*>(op2))
			{
				if (dynamic_cast<IR*>(op2)->opr == OPR_EXTRACT8HL)
				{
					val2 = dynamic_cast<IR*>(op2)->Operands[0]->valuePtr;
					if (dynamic_cast<IR*>(op3))
					{
						if (dynamic_cast<IR*>(op3)->opr == OPR_EXTRACT8H)
						{
							val3 = dynamic_cast<IR*>(op3)->Operands[0]->valuePtr;
							if (dynamic_cast<IR*>(op4))
							{
								if (dynamic_cast<IR*>(op4)->opr == OPR_EXTRACT8L)
								{
									val4 = dynamic_cast<IR*>(op4)->Operands[0]->valuePtr;
									if ((val1 == val2) && (val2 == val3) && (val2 == val4))
									{
										ast = val1->ast;
										ast->childrenCount = op1->ast->childrenCount;
										_plugin_logprintf("Concat 4 Optimized\n");
										return;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	*/
	ast = std::make_shared<BTreeNode>(MakeBTreeNode(printOpr(_opr)));
	ast->m_NodeType = NT_OPERATOR;
	ast->childrenCount = op1->ast->childrenCount + op2->ast->childrenCount + op3->ast->childrenCount + op4->ast->childrenCount + 4;

	std::shared_ptr<BTreeNode> ptr1 = std::make_shared<BTreeNode>(MakeBTreeNode(op1->Name));
	MakeLeftSubTree(ast, op1->ast);

	std::shared_ptr<BTreeNode>  ptr2 = std::make_shared<BTreeNode>(MakeBTreeNode(op2->Name));
	MakeRightSubTree(ast, op2->ast);
#ifdef DEBUG
	MessageBoxA(0, "Op2", "Op2", 0);
#endif


	std::shared_ptr<BTreeNode>  ptr3 = std::make_shared<BTreeNode>(MakeBTreeNode(op3->Name));
	MakeThirdSubTree(ast, op3->ast);

#ifdef DEBUG
	MessageBoxA(0, "Op3", "Op3", 0);
#endif

	std::shared_ptr<BTreeNode>  ptr4 = std::make_shared<BTreeNode>(MakeBTreeNode(op4->Name));
	MakeFourthSubTree(ast, op4->ast);

#ifdef DEBUG
	MessageBoxA(0, "Op4", "Op4", 0);
#endif
}

IR* CreateBVVIR(DWORD intVal, BYTE size)
{
	IR* op1 = new ConstInt(IR::OPR_GET32INT, intVal);
	op1->Size = size;
	op1->ast = std::make_shared<BTreeNode>(MakeBTreeNode(op1->printOpr(IR::OPR_GET32INT)));
	op1->ast->m_NodeType = NT_OPERATOR;
	op1->ast->valPtr = op1;

	IR* op2 = new ConstInt(IR::OPR_GET8INT, size);
	op2->Size = 8;
	op2->ast = std::make_shared<BTreeNode>(MakeBTreeNode(op2->printOpr(IR::OPR_GET8INT)));
	op2->ast->m_NodeType = NT_OPERATOR;
	op2->ast->valPtr = op2;

	IR* testAddIR = new IR(IR::OPR_BVV, op1, op2, size);
	testAddIR->OperandType = IR::OPERANDTYPE_CONSTANT;
	testAddIR->Size = size;
	testAddIR->ast->m_NodeType = NT_OPERATOR;
	//testAddIR->z3ExprPtr = new z3::expr(z3Context->bv_val((unsigned int)intVal, size));
	//testAddIR->isHiddenRHS = true;
	return testAddIR;
}

IR* CreateExtractIR(DWORD from, DWORD to, Value* val)
{
	IR* op1 = CreateBVVIR(from, 1);

	IR* op2 = CreateBVVIR(to, 1);

	IR* testAddIR = new IR(IR::OPR_EXTRACT, op1, op2, val);
	testAddIR->Size = to - from + 1;
	//testAddIR->isHiddenRHS = true;
	return testAddIR;
}

IR* CraeteLoadIR(Value* op1, IR::OPR _opr)
{
	IR* testAddIR = new IR(IR::OPR_LOAD, op1);
	//testAddIR->isHiddenRHS = true;
	return testAddIR;
}

IR* CreateZeroExtendIR(Value* _val, BYTE _toExtendSize, BYTE _currentSize, vector<IR*>& irList)
{
	IR* op1 = CreateBVVIR(_toExtendSize - _currentSize, 8);
	//op1->Size = size;
	//op1->ast = std::make_shared<BTreeNode>(MakeBTreeNode(op1->printOpr(IR::OPR_BVV)));
	//op1->ast->m_NodeType = NT_OPERATOR;
	//op1->ast->valPtr = op1;	

	IR* testAddIR = new IR(IR::OPR_ZEXT, op1, _val, _toExtendSize);
	testAddIR->Size = _toExtendSize;
	testAddIR->ast->m_NodeType = NT_OPERATOR;
	//testAddIR->isHiddenRHS = true;
	irList.push_back(testAddIR);
	return testAddIR;
}

IR* CreateSignExtendIR(Value* _val, BYTE _toExtendSize, BYTE _currentSize, vector<IR*>& irList)
{
	IR* op1 = CreateBVVIR(_toExtendSize - _currentSize, 8);
	//op1->Size = size;
	//op1->ast = std::make_shared<BTreeNode>(MakeBTreeNode(op1->printOpr(IR::OPR_BVV)));
	//op1->ast->m_NodeType = NT_OPERATOR;
	//op1->ast->valPtr = op1;	

	IR* testAddIR = new IR(IR::OPR_SEXT, op1, _val, _toExtendSize);
	testAddIR->Size = _toExtendSize;
	testAddIR->ast->m_NodeType = NT_OPERATOR;
	//testAddIR->isHiddenRHS = true;
	irList.push_back(testAddIR);
	return testAddIR;
}

IR* CraeteUnaryIR(Value* op1, IR::OPR _opr)
{
	IR* testAddIR = new IR(_opr, op1);
	if (op1->isTainted)
	{
		testAddIR->isTainted = true;
	}

	if (_opr == IR::OPR::OPR_EXTRACTSF64 || _opr == IR::OPR::OPR_EXTRACTSF32 ||
		_opr == IR::OPR::OPR_EXTRACTSF16 || _opr == IR::OPR::OPR_EXTRACTSF8)
	{
		testAddIR->Size = 1;
	}

	else
	{
		testAddIR->Size = op1->Size;
	}
	return testAddIR;
}

IR* CraeteBinaryIR(Value* op1, Value* op2, IR::OPR _opr)
{
	IR* testAddIR = new IR(_opr, op1, op2);
	testAddIR->Size = op1->Size;
	return testAddIR;
}

IR* CraeteStoreIR(Value* op1, Value* op2, IR::OPR _opr)
{
	IR* testAddIR = new IR(IR::OPR_STORE, op1, op2);
	testAddIR->isHiddenRHS = true;
	return testAddIR;
}

IR* CraeteBitVecValIR(DWORD val, DWORD size, IR::OPR _opr)
{
	Value* valValue = new ConstInt(IR::OPR_GET32INT, val);
	Value* sizeValue = new ConstInt(IR::OPR_GET32INT, size);
	IR* testAddIR = new IR(IR::OPR_BVV, valValue, sizeValue);
	return testAddIR;
}

void ConcatOptimize(Value* concatValue)
{
	IR* concatIR = dynamic_cast<IR*>(concatValue);
	bool isOperandSame = true;
	if (concatIR != nullptr)
	{
		if (concatIR->opr == IR::OPR_CONCAT)
		{
			for (int i = 0; i < concatIR->Operands.size(); i++)
			{
				if (concatIR->Operands[0]->valuePtr != concatIR->Operands[i]->valuePtr)
				{
					isOperandSame = false;
					break;
				}
			}

			if (isOperandSame == true)
			{
				for (int i = 0; i < concatIR->Operands.size(); i++)
				{
					if (dynamic_cast<IR*>(concatIR->Operands[i]->valuePtr))
					{
						IR::OPR opr = dynamic_cast<IR*>(concatIR->Operands[i]->valuePtr)->opr;
						switch (opr)
						{
						case IR::OPR_EXTRACT16H:
							break;
						case IR::OPR_EXTRACT8HH:
							break;
						case IR::OPR_EXTRACT8HL:
							break;
						case IR::OPR_EXTRACT8H:
							break;
						case IR::OPR_EXTRACT8L:
							break;
						default:
							break;
						}
					}
				}
			}
		}
	}
}

void findMemValue()
{
	for (auto testIt : IRList)
	{
		for (auto irIt : testIt.second)
		{
			if (irIt->opr == IR::OPR_STORE)
			{
				if (ConstInt* constIr = dynamic_cast<ConstInt*>(irIt->Operands[1]->valuePtr))
				{
					MemValue[constIr->intVar].push_back(irIt->Operands[0]->valuePtr);
				}
			}
		}
	}
}

// Value에 대해서 각 레지스터의 파트 별로 저장한다. (상위 16비트, 상위 8비트, 하위 8비트)
void SaveRegisterValue(ZydisRegister _zydisReg, Value* _regValue, BYTE _size, vector<IR*>& irList)
{
	DWORD op1ConstValue;
	DWORD op2ConstValue;
	DWORD op3ConstValue;

	if (_size == 32)
	{
		IR* reg16hIR;
		IR* reg8hIR;
		IR* reg8lIR;

		switch (_zydisReg)
		{
		case ZYDIS_REGISTER_EAX:
			reg16hIR = new IR("EAX16H", RegValue[REG_EAX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			reg16hIR->Size = 16;
			RegValue[REG_EAX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EAX8H", RegValue[REG_EAX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_EAX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EAX8L", RegValue[REG_EAX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_EAX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EBX:
			reg16hIR = new IR("EBX16H", RegValue[REG_EBX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			reg16hIR->Size = 16;
			RegValue[REG_EBX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EBX8H", RegValue[REG_EBX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_EBX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EBX8L", RegValue[REG_EBX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_EBX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_ECX:
			reg16hIR = new IR("ECX16H", RegValue[REG_ECX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			reg16hIR->Size = 16;
			RegValue[REG_ECX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("ECX8H", RegValue[REG_ECX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_ECX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("ECX8L", RegValue[REG_ECX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_ECX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EDX:
			reg16hIR = new IR("EDX16H", RegValue[REG_EDX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_EDX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EDX8H", RegValue[REG_EDX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_EDX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EDX8L", RegValue[REG_EDX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_EDX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EBP:
			reg16hIR = new IR("EBP16H", RegValue[REG_EBP_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			reg16hIR->Size = 16;
			RegValue[REG_EBP_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EBP8H", RegValue[REG_EBP_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_EBP_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EBP8L", RegValue[REG_EBP_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_EBP_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_ESI:
			reg16hIR = new IR("ESI16H", RegValue[REG_ESI_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			reg16hIR->Size = 16;
			RegValue[REG_ESI_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("ESI8H", RegValue[REG_ESI_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_ESI_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("ESI8LH", RegValue[REG_ESI_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_ESI_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EDI:
			reg16hIR = new IR("EDI16H", RegValue[REG_EDI_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			reg16hIR->Size = 16;
			RegValue[REG_EDI_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EDI8H", RegValue[REG_EDI_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_EDI_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EDI8L", RegValue[REG_EDI_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_EDI_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;
		}
	}

	else if (_size == 16)
	{
		IR* reg8hIR;
		IR* reg8lIR;

		switch (_zydisReg)
		{
		case ZYDIS_REGISTER_AX:
			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				reg8hIR->Size = 8;
				RegValue[REG_EAX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				reg8lIR->Size = 8;
				RegValue[REG_EAX_8L].push_back(reg8lIR);
				break;
			}

			else
			{
				reg8hIR = new IR("EAX8H", RegValue[REG_EAX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				reg8hIR->Size = 8;
				RegValue[REG_EAX_8H].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
				reg8lIR->Size = 8;
				RegValue[REG_EAX_8L].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				break;
			}

		case ZYDIS_REGISTER_BX:
			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				reg8hIR->Size = 8;
				RegValue[REG_EBX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				reg8lIR->Size = 8;
				RegValue[REG_EBX_8L].push_back(reg8lIR);
				break;
			}
			else
			{
				reg8hIR = new IR("EBX8H", RegValue[REG_EBX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				reg8hIR->Size = 8;
				RegValue[REG_EBX_8H].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				reg8lIR = new IR("EBX8L", RegValue[REG_EBX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
				reg8lIR->Size = 8;
				RegValue[REG_EBX_8L].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				break;
			}

		case ZYDIS_REGISTER_CX:
			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				reg8hIR->Size = 8;
				RegValue[REG_ECX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				reg8lIR->Size = 8;
				RegValue[REG_ECX_8L].push_back(reg8lIR);
				break;
			}

			else
			{
				reg8hIR = new IR("ECX8H", RegValue[REG_ECX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				reg8hIR->Size = 8;
				RegValue[REG_ECX_8H].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				reg8lIR = new IR("ECX8L", RegValue[REG_ECX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
				reg8lIR->Size = 8;
				RegValue[REG_ECX_8L].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				break;
			}

		case ZYDIS_REGISTER_DX:

			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				reg8hIR->Size = 8;
				RegValue[REG_EDX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				reg8lIR->Size = 8;
				RegValue[REG_EDX_8L].push_back(reg8lIR);
				break;
			}

			reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _regValue);
			reg8hIR->Size = 8;
			RegValue[REG_EDX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
			reg8lIR->Size = 8;
			RegValue[REG_EDX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;
		}
	}

	else if (_size == 8)
	{
		IR* reg8hIR;
		IR* reg8lIR;

		switch (_zydisReg)
		{
		case ZYDIS_REGISTER_AH:			
			//reg8hIR = CreateExtractIR(15, 8, _regValue);
			RegValue[REG_EAX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_BH:
			//reg8hIR = CreateExtractIR(15, 8, _regValue);
			RegValue[REG_EBX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_CH:
			//reg8hIR = CreateExtractIR(15, 8, _regValue);
			RegValue[REG_ECX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_DH:
			//reg8hIR = CreateExtractIR(15, 8, _regValue);
			RegValue[REG_EDX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_AL:
			//reg8lIR = CreateExtractIR(7, 0, _regValue);
			RegValue[REG_EAX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_BL:
			//reg8lIR = CreateExtractIR(7, 0, _regValue);
			RegValue[REG_EBX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_CL:
			//reg8lIR = CreateExtractIR(7, 0, _regValue);
			RegValue[REG_ECX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_DL:
			//reg8lIR = CreateExtractIR(7, 0, _regValue);
			RegValue[REG_EDX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;
		}
	}
}
//#define DEBUG
// Store 명령어의 오퍼랜드가 Constant인 경우 해당 메모리 주소의 MemValuePool에 Value* 저장
Value* GetRegisterValue(ZydisRegister _zydisReg, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR = nullptr;
	Value* op1;
	Value* op2;
	Value* op3;

	DWORD op1ConstValue = 0;
	DWORD op2ConstValue = 0;
	DWORD op3ConstValue = 0;
	DWORD foldedConstValue = 0;

	//rstIR->opr = IR::OPR::OPR_CONCAT; 
	switch (_zydisReg)
	{
		// 32 비트 레지스터인 경우
	case ZYDIS_REGISTER_EAX:
		if (RegValue[REG_EAX_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cax) >> 16, 16);
			RegValue[REG_EAX_16H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 16H :%x\n", (_regdump.regcontext.cax) >> 16);
			MessageBoxA(0, "test", "test", 0);
#endif
			irList.push_back(rstIR);
		}
		op1 = RegValue[REG_EAX_16H].back();

		if (RegValue[REG_EAX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cax) & 0xffff) >> 8, 8);
			RegValue[REG_EAX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = RegValue[REG_EAX_8H].back();

		if (RegValue[REG_EAX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cax) & 0xff, 8);
			RegValue[REG_EAX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.cax) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = RegValue[REG_EAX_8L].back();

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}


		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/
		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_REGISTER;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EBX:

		if (RegValue[REG_EBX_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbx) >> 16, 16);
			RegValue[REG_EBX_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EBX_16H].back());

		if (RegValue[REG_EBX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
			RegValue[REG_EBX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBX_8H].back());

		if (RegValue[REG_EBX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_EBX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8L :%x\n", (_regdump.regcontext.cbx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EBX_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}

		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_REGISTER;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_ECX:
		if (RegValue[REG_ECX_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.ccx) >> 16, 16);
			RegValue[REG_ECX_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_ECX_16H].back());

		if (RegValue[REG_ECX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.ccx) & 0xffff) >> 8, 8);
			RegValue[REG_ECX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8H :%x\n", ((_regdump.regcontext.ccx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ECX_8H].back());

		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.ccx) & 0xff, 8);
			RegValue[REG_ECX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8L :%x\n", (_regdump.regcontext.ccx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ECX_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}

		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_REGISTER;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EDX:

		if (RegValue[REG_EDX_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdx) >> 16, 16);
			RegValue[REG_EDX_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EDX_16H].back());

		if (RegValue[REG_EDX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cdx) & 0xffff) >> 8, 8);
			RegValue[REG_EDX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8H :%x\n", ((_regdump.regcontext.cdx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDX_8H].back());

		if (RegValue[REG_EDX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdx) & 0xff, 8);
			RegValue[REG_EDX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8L :%x\n", (_regdump.regcontext.cdx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EDX_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}

		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EBP:

		if (RegValue[REG_EBP_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbp) >> 16, 16);
			RegValue[REG_EBP_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EBP_16H].back());

		if (RegValue[REG_EBP_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cbp) & 0xffff) >> 8, 8);
			RegValue[REG_EBP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBP 8H :%x\n", ((_regdump.regcontext.cbp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBP_8H].back());

		if (RegValue[REG_EBP_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbp) & 0xff, 8);
			RegValue[REG_EBP_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBP 8L :%x\n", (_regdump.regcontext.cbp) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EBP_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}

		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_ESP:

		if (RegValue[REG_ESP_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.csp) >> 16, 16);
			RegValue[REG_ESP_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_ESP_16H].back());

		if (RegValue[REG_ESP_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.csp) & 0xffff) >> 8, 8);
			RegValue[REG_ESP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESP 8H :%x\n", ((_regdump.regcontext.csp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESP_8H].back());

		if (RegValue[REG_ESP_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.csp) & 0xff, 8);
			RegValue[REG_ESP_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.csp) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ESP_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}


		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;

		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_ESI:

		if (RegValue[REG_ESI_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.csi) >> 16, 16);
			RegValue[REG_ESI_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_ESI_16H].back());

		if (RegValue[REG_ESI_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.csi) & 0xffff) >> 8, 8);
			RegValue[REG_ESI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESI_8H].back());

		if (RegValue[REG_ESI_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.csi) & 0xff, 8);
			RegValue[REG_ESI_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8L :%x\n", (_regdump.regcontext.csi) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ESI_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}

		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EDI:

		if (RegValue[REG_EDI_16H].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdi) >> 16, 16);
			RegValue[REG_EDI_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EDI_16H].back());

		if (RegValue[REG_EDI_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cdi) & 0xffff) >> 8, 8);
			RegValue[REG_EDI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8H :%x\n", ((_regdump.regcontext.cdi) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDI_8H].back());

		if (RegValue[REG_EDI_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdi) & 0xff, 8);
			RegValue[REG_EDI_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8L :%x\n", (_regdump.regcontext.cdi) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EDI_8L].back());

		/*
		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_EXTRACT16H &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_EXTRACT8L)
			{
				if (dynamic_cast<IR*>(op1)->Operands[0]->valuePtr == dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(op2)->Operands[0]->valuePtr == dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr))
						{
							irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr));
							return dynamic_cast<IR*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr);
						}
					}
				}
			}
		}

		if (dynamic_cast<IR*>(op1) &&
			dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op1)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op1)->Operands[0]->valuePtr)->intVar << 16;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;

		// 16 비트 레지스터
	case ZYDIS_REGISTER_AX:
		if (RegValue[REG_EAX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cax) & 0xffff) >> 8, 8);
			RegValue[REG_EAX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = RegValue[REG_EAX_8H].back();

		if (RegValue[REG_EAX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cax) & 0xff, 8);
			RegValue[REG_EAX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.cax) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = RegValue[REG_EAX_8L].back();

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_BX:
		if (RegValue[REG_EBX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
			RegValue[REG_EBX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBX_8H].back());

		if (RegValue[REG_EBX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_EBX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8L :%x\n", (_regdump.regcontext.cbx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EBX_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_CX:
		if (RegValue[REG_ECX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
			RegValue[REG_ECX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ECX_8H].back());

		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_ECX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8L :%x\n", (_regdump.regcontext.cbx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ECX_8L].back());

		/*
		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_DX:
		if (RegValue[REG_EDX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cdx) & 0xffff) >> 8, 8);
			RegValue[REG_EDX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8H :%x\n", ((_regdump.regcontext.cdx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDX_8H].back());

		if (RegValue[REG_EDX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdx) & 0xff, 8);
			RegValue[REG_EDX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8L :%x\n", (_regdump.regcontext.cdx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EDX_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_BP:
		if (RegValue[REG_EBP_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cbp) & 0xffff) >> 8, 8);
			RegValue[REG_EBP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBP 8H :%x\n", ((_regdump.regcontext.cbp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBP_8H].back());

		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_ECX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8L :%x\n", _regdump.regcontext.cbx & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EBP_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_SP:
		if (RegValue[REG_ESP_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.csp) & 0xffff) >> 8, 8);
			RegValue[REG_ESP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESP 8H :%x\n", ((_regdump.regcontext.csp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESP_8H].back());

		if (RegValue[REG_ESP_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.csp) & 0xff, 8);
			RegValue[REG_ESP_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESP 8L :%x\n", _regdump.regcontext.csp & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ESP_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_SI:
		if (RegValue[REG_ESI_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.csi) & 0xffff) >> 8, 8);
			RegValue[REG_ESI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8H :%x\n", ((_regdump.regcontext.csi) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESI_8H].back());

		if (RegValue[REG_ESI_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.csi) & 0xff, 8);
			RegValue[REG_ESI_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8L :%x\n", _regdump.regcontext.csi & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ESI_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_DI:
		if (RegValue[REG_EDI_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cdi) & 0xffff) >> 8, 8);
			RegValue[REG_EDI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8H :%x\n", ((_regdump.regcontext.cdi) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDI_8H].back());

		if (RegValue[REG_EDI_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdi) & 0xff, 8);
			RegValue[REG_EDI_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8L :%x\n", _regdump.regcontext.cdi & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EDI_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;

		// 8비트
	case ZYDIS_REGISTER_AH:
		if (RegValue[REG_EAX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cax) & 0xffff) >> 8, 8);
			RegValue[REG_EAX_8H].push_back(rstIR);
			//#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
			//#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EAX_8H].back());
		if (op2->Size == 0)
			MessageBoxA(0, "Test", "Test", 0);
		return op2;
		break;

	case ZYDIS_REGISTER_AL:
		if (RegValue[REG_EAX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cax) & 0xff, 8);
			RegValue[REG_EAX_8L].push_back(rstIR);
			//#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.cax) & 0xff);
			//#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EAX_8L].back());
		return op2;
		break;

	case ZYDIS_REGISTER_BH:
		if (RegValue[REG_EBX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
			RegValue[REG_EBX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBX_8H].back());
		return op2;
		break;

	case ZYDIS_REGISTER_BL:
		if (RegValue[REG_EBX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_EBX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8L :%x\n", (_regdump.regcontext.cbx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBX_8L].back());
		return op2;
		break;

	case ZYDIS_REGISTER_CH:
		if (RegValue[REG_ECX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.ccx) & 0xffff) >> 8, 8);
			RegValue[REG_ECX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8H :%x\n", ((_regdump.regcontext.ccx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ECX_8H].back());
		return op2;
		break;

	case ZYDIS_REGISTER_CL:
		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.ccx) & 0xff, 8);
			RegValue[REG_ECX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8L :%x\n", (_regdump.regcontext.cdx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ECX_8L].back());
		return op2;
		break;

	case ZYDIS_REGISTER_DH:
		if (RegValue[REG_EDX_8H].size() == 0)
		{
			rstIR = CreateBVVIR(((_regdump.regcontext.cdx) & 0xffff) >> 8, 8);
			RegValue[REG_EDX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8H :%x\n", ((_regdump.regcontext.cdx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDX_8H].back());
		return op2;
		break;

	case ZYDIS_REGISTER_DL:
		if (RegValue[REG_EDX_8L].size() == 0)
		{
			rstIR = CreateBVVIR((_regdump.regcontext.cdx) & 0xff, 8);
			RegValue[REG_EDX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8L :%x\n", (_regdump.regcontext.cdx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDX_8L].back());
		return op2;
		break;
	}
	return nullptr;
}

//#define DEBUG
void SaveMemoryValue(ZydisDecodedInstruction* decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, Value* _toSaveValue, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* op3;
	IR* offset1;
	IR* offset2;
	IR* offset3;
	IR* offset4;

	Value* offsetImm1;
	Value* offsetImm2;
	Value* offsetImm3;
	Value* offsetImm4;

	Value* offset0Value;
	Value* offset1Value;
	Value* offset2Value;
	Value* offset3Value;

	Value* BaseValue = nullptr;;
	Value* Disp = nullptr;
	Value* Scale = nullptr;;
	Value* IndexValue = nullptr;;

	IR* BaseDisp = nullptr;;
	IR* ScaleIndex = nullptr;;
	IR* EAValue = nullptr;;

	DWORD _targetMem;

	// 1. EA 계산
	_targetMem = GetEffectiveAddress32(_decodedOperandPtr, _regdump);

	// 2. EA를 계산하기 위한 IR 생성
	// 2-1. 오퍼랜드에 Base가 존재하는 경우
	if (_decodedOperandPtr->mem.base != ZYDIS_REGISTER_NONE)
	{
		BaseValue = GetRegisterValue(_decodedOperandPtr->mem.base, _regdump, irList);

		if (BaseValue == nullptr)
			MessageBoxA(0, "BaseValue is null", "SaveMemoryValue", 0);

		if (_decodedOperandPtr->mem.disp.has_displacement)
		{
			// [Base + Index*Scale + Disp]

			// Base + Disp
			Disp = GetImmValue(_decodedOperandPtr->mem.disp.value, 32, irList);
			BaseDisp = CraeteBinaryIR(BaseValue, Disp, IR::OPR_ADD);
			irList.push_back(BaseDisp);

			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{
				Scale = GetImmValue(_decodedOperandPtr->mem.scale, 32, irList);
				IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
				ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
				irList.push_back(ScaleIndex);
				EAValue = CraeteBinaryIR(BaseDisp, ScaleIndex, IR::OPR_ADD);
				irList.push_back(EAValue);

			}

			// [Base + Disp]
			else
			{
				EAValue = BaseDisp;
			}
		}

		// Displacement가 없는경우
		else
		{
			// [Base + Index*Scale]
			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{
				Scale = GetImmValue(_decodedOperandPtr->mem.scale, 32, irList);
				IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
				ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
				irList.push_back(ScaleIndex);
				EAValue = CraeteBinaryIR(BaseValue, ScaleIndex, IR::OPR_ADD);
				irList.push_back(EAValue);
			}
			// [Base]
			else
			{
				Disp = GetImmValue(0, 32, irList);
				EAValue = CraeteBinaryIR(BaseValue, Disp, IR::OPR_ADD);
				irList.push_back(EAValue);
			}
		}
	}

	// 2-2. 오퍼랜드에 Base가 존재하지 않는 경우
	else
	{
		// [index*Scale + Disp]
		if (_decodedOperandPtr->mem.disp.has_displacement)
		{

			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{
				Scale = GetImmValue(_decodedOperandPtr->mem.scale, 32, irList);
				IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
				ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
				irList.push_back(ScaleIndex);

				Disp = GetImmValue(_decodedOperandPtr->mem.disp.value, 32, irList);
				EAValue = CraeteBinaryIR(ScaleIndex, Disp, IR::OPR_ADD);
				irList.push_back(EAValue);
			}

			else
			{
				EAValue = CreateBVVIR(_decodedOperandPtr->mem.disp.value, 32);
				irList.push_back(EAValue);
			}
		}

		// [index * Scale]
		else
		{
			Scale = GetImmValue(_decodedOperandPtr->mem.scale, 32, irList);
			IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
			ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
			irList.push_back(ScaleIndex);
		}
	}

	// Disp

	if (_decodedOperandPtr->size == 32)
	{
		// Base 레지스터의 Value가 상수인 경우 EA는 상수이므로 해당 EA에 대한 Memory Value Pool에 저장한다.					
		{
			DWORD _memAddr = _targetMem;
			IR* reg8hh;
			IR* reg8hlIR;
			IR* reg8hIR;
			IR* reg8lIR;

			// 3. Store 명령어 생성
			rstIR = CraeteStoreIR(EAValue, _toSaveValue, IR::OPR_STORE);
			rstIR->isHiddenRHS = true;
			irList.push_back(rstIR);

			// 4. 메모리의 각 1바이트 별로 Value를 저장함.
			/*
			// 메모리에 저장할 Value가 상수인 경우
			if (dynamic_cast<ConstInt*>(_toSaveValue))
			{
				printf("SaveMemoryValue -> Value is Constnat\n");
			}

			// 메모리에 저장할 Value가 상수가 아닌 경우
			else
			*/
			{
				//if (MemValue.find(_memAddr + 3) != MemValue.end())
				//{
				//	reg8hh = new IR(IR::OPR::OPR_EXTRACT8HH, _toSaveValue);
				//	vector<Value*> tempVecMemAddr3;
				//	tempVecMemAddr3.push_back(reg8hh);
				//	MemValue.insert(make_pair(_memAddr + 3, tempVecMemAddr3));
				//}
				//else
					reg8hh = CreateExtractIR(31, 24, _toSaveValue); //new IR(IR::OPR::OPR_EXTRACT8HH, _toSaveValue);
					

				MemValue[_memAddr + 3].push_back(reg8hh);
				irList.push_back(reg8hh);

#ifdef DEBUG
				_plugin_logprintf("[SaveMemoryValue] [%p] %p\n", _regdump.regcontext.cip, _memAddr);
				if(dynamic_cast<IR*>(_toSaveValue))
				printIR(dynamic_cast<IR*>(_toSaveValue));
#endif
				//rstIR3 = CraeteStoreIR(BaseValue, _toSaveValue, IR::OPR_STORE);
				//rstIR3->isHiddenRHS = true;

				//if (MemValue.find(_memAddr + 2) != MemValue.end())
				//{
				//	reg8hlIR = new IR(IR::OPR::OPR_EXTRACT8HL, _toSaveValue);
				//	vector<Value*> tempVecMemAddr2;
				//	tempVecMemAddr2.push_back(reg8hlIR);
				//	MemValue.insert(make_pair(_memAddr + 2, tempVecMemAddr2));
				//}
				//else
					reg8hlIR = CreateExtractIR(23, 16, _toSaveValue); //new IR(IR::OPR::OPR_EXTRACT8HL, _toSaveValue);

				MemValue[_memAddr + 2].push_back(reg8hlIR);
				irList.push_back(reg8hlIR);

				//if (MemValue.find(_memAddr + 1) != MemValue.end())
				//{
				//	reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _toSaveValue);
				//	vector<Value*> tempVecMemAddr1;
				//	tempVecMemAddr1.push_back(reg8hIR);
				//	MemValue.insert(make_pair(_memAddr + 1, tempVecMemAddr1));
				//}
				//else
					reg8hIR = CreateExtractIR(15, 8, _toSaveValue); //new IR(IR::OPR::OPR_EXTRACT8H, _toSaveValue);

				MemValue[_memAddr + 1].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				//if (MemValue.find(_memAddr) != MemValue.end())
				//{
				//	reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
				//	vector<Value*> tempVecMemAddr;
				//	tempVecMemAddr.push_back(reg8lIR);
				//	MemValue.insert(make_pair(_memAddr, tempVecMemAddr));
				//}
				//else
					reg8lIR = CreateExtractIR(7, 0, _toSaveValue);//new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
				MemValue[_memAddr].push_back(reg8lIR);
				irList.push_back(reg8lIR);
			}
		}
	}

	else if (_decodedOperandPtr->size == 16)
	{
		// Base 레지스터의 Value가 상수인 경우 EA는 상수이므로 해당 EA에 대한 Memory Value Pool에 저장한다.					
		{
			DWORD _memAddr = _targetMem;
			IR* reg8hh;
			IR* reg8hlIR;
			IR* reg8hIR;
			IR* reg8lIR;

			// 3. Store 명령어 생성
			rstIR = CraeteStoreIR(EAValue, _toSaveValue, IR::OPR_STORE);
			rstIR->isHiddenRHS = true;
			irList.push_back(rstIR);

			// 4. 메모리의 각 1바이트 별로 Value를 저장함. 
			// 메모리에 저장할 Value가 상수인 경우
			if (dynamic_cast<ConstInt*>(_toSaveValue))
			{
				_plugin_logprintf("[%p] SaveMemoryValue -> Value is Constnat %p\n", cntd, _targetMem);
			}

			// 메모리에 저장할 Value가 상수가 아닌 경우
			else
			{
				if (MemValue.find(_memAddr + 1) != MemValue.end())
				{
					reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _toSaveValue);
					reg8hIR->Size = 8;
					vector<Value*> tempVecMemAddr1;
					tempVecMemAddr1.push_back(reg8hIR);
					MemValue.insert(make_pair(_memAddr + 1, tempVecMemAddr1));
				}
				else
					reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _toSaveValue);
				reg8hIR->Size = 8;
				MemValue[_memAddr + 1].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				if (MemValue.find(_memAddr) != MemValue.end())
				{
					reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
					reg8lIR->Size = 8;
					vector<Value*> tempVecMemAddr;
					tempVecMemAddr.push_back(reg8lIR);
					MemValue.insert(make_pair(_memAddr, tempVecMemAddr));
				}
				else
					reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
				reg8lIR->Size = 8;
				MemValue[_memAddr].push_back(reg8lIR);
				irList.push_back(reg8lIR);
			}
		}
	}

	else if (_decodedOperandPtr->size == 8)
	{
		// Base 레지스터의 Value가 상수인 경우 EA는 상수이므로 해당 EA에 대한 Memory Value Pool에 저장한다.					
		{
			DWORD _memAddr = _targetMem;
			IR* reg8hh;
			IR* reg8hlIR;
			IR* reg8hIR;
			IR* reg8lIR;

			// 3. Store 명령어 생성
			rstIR = CraeteStoreIR(EAValue, _toSaveValue, IR::OPR_STORE);
			rstIR->isHiddenRHS = true;
			irList.push_back(rstIR);

			// 4. 메모리의 각 1바이트 별로 Value를 저장함. 
			// 메모리에 저장할 Value가 상수인 경우
			if (dynamic_cast<ConstInt*>(_toSaveValue))
			{
				printf("SaveMemoryValue -> Value is Constnat\n");
			}

			// 메모리에 저장할 Value가 상수가 아닌 경우
			else
			{
				//if (MemValue.find(_memAddr) != MemValue.end())
				//{
				//	reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
				//	vector<Value*> tempVecMemAddr;
				//	tempVecMemAddr.push_back(reg8lIR);
				//	MemValue.insert(make_pair(_memAddr, tempVecMemAddr));
				//}
				//else
					reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
					reg8lIR->Size = 8;
				MemValue[_memAddr].push_back(reg8lIR);
				irList.push_back(reg8lIR);
			}
		}
	}

	return;
}

void SaveMemoryValue(ZydisRegister zydisRegister, Value* _toSaveValue, BYTE _size, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* op3;
	IR* offset1;
	IR* offset2;
	IR* offset3;
	IR* offset4;

	Value* offsetImm1;
	Value* offsetImm2;
	Value* offsetImm3;
	Value* offsetImm4;

	Value* offset0Value;
	Value* offset1Value;
	Value* offset2Value;
	Value* offset3Value;

	Value* BaseValue = nullptr;;
	Value* Disp = nullptr;
	Value* Scale = nullptr;;
	Value* IndexValue = nullptr;;

	IR* BaseDisp = nullptr;;
	IR* ScaleIndex = nullptr;;
	IR* EAValue = nullptr;;

	DWORD _targetMem;

	// 1. EA 계산
	_targetMem = _regdump.regcontext.csp - 4; //GetRegisterValueFromRegdump(zydisRegister, _regdump);

	// 2. EA를 계산하기 위한 IR 생성
	// 2-1. 오퍼랜드에 Base가 존재하는 경우

	{
		BaseValue = GetRegisterValue(zydisRegister, _regdump, irList);

		if (BaseValue == nullptr)
			MessageBoxA(0, "BaseValue is null", "SaveMemoryValue", 0);

		// Displacement가 없는경우
		{
			{
				Disp = GetImmValue(0, 32, irList);
				EAValue = CraeteBinaryIR(BaseValue, Disp, IR::OPR_ADD);
				irList.push_back(EAValue);
			}
		}
	}

	//if (_decodedOperandPtr->size == 32)
	{
		// Base 레지스터의 Value가 상수인 경우 EA는 상수이므로 해당 EA에 대한 Memory Value Pool에 저장한다.					
		{
			DWORD _memAddr = _targetMem;
			IR* reg8hh;
			IR* reg8hlIR;
			IR* reg8hIR;
			IR* reg8lIR;

			// 3. Store 명령어 생성
			rstIR = CraeteStoreIR(EAValue, _toSaveValue, IR::OPR_STORE);
			rstIR->isHiddenRHS = true;
			irList.push_back(rstIR);

			// 4. 메모리의 각 1바이트 별로 Value를 저장함. 
			// 메모리에 저장할 Value가 상수인 경우
			if (dynamic_cast<ConstInt*>(_toSaveValue))
			{
				_plugin_logprintf("[%p] SaveMemoryValue -> Value is Constnat\n", cntd);
			}

			// 메모리에 저장할 Value가 상수가 아닌 경우
			//else
			{
				if (MemValue.find(_memAddr + 3) != MemValue.end())
				{
					reg8hh = new IR(IR::OPR::OPR_EXTRACT8HH, _toSaveValue);
					//vector<Value*> tempVecMemAddr3;
					//tempVecMemAddr3.push_back(reg8hh);
					//MemValue.insert(make_pair(_memAddr + 3, tempVecMemAddr3));
				}
				else
					reg8hh = new IR(IR::OPR::OPR_EXTRACT8HH, _toSaveValue);

				MemValue[_memAddr + 3].push_back(reg8hh);
				irList.push_back(reg8hh);
#ifdef DEBUG
				_plugin_logprintf("[SaveMemoryValue] [%p] %p\n", _regdump.regcontext.cip, _memAddr);
#endif
				//rstIR3 = CraeteStoreIR(BaseValue, _toSaveValue, IR::OPR_STORE);
				//rstIR3->isHiddenRHS = true;

				if (MemValue.find(_memAddr + 2) != MemValue.end())
				{
					reg8hlIR = new IR(IR::OPR::OPR_EXTRACT8HL, _toSaveValue);
					//vector<Value*> tempVecMemAddr2;
					//tempVecMemAddr2.push_back(reg8hlIR);
					//MemValue.insert(make_pair(_memAddr + 2, tempVecMemAddr2));
				}
				else
					reg8hlIR = new IR(IR::OPR::OPR_EXTRACT8HL, _toSaveValue);

				MemValue[_memAddr + 2].push_back(reg8hlIR);
				irList.push_back(reg8hlIR);

				if (MemValue.find(_memAddr + 1) != MemValue.end())
				{
					reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _toSaveValue);
					//vector<Value*> tempVecMemAddr1;
					//tempVecMemAddr1.push_back(reg8hIR);
					//MemValue.insert(make_pair(_memAddr + 1, tempVecMemAddr1));
				}
				else
					reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _toSaveValue);

				MemValue[_memAddr + 1].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				if (MemValue.find(_memAddr) != MemValue.end())
				{
					reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
					//vector<Value*> tempVecMemAddr;
					//tempVecMemAddr.push_back(reg8lIR);
					//MemValue.insert(make_pair(_memAddr, tempVecMemAddr));
				}
				else
					reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _toSaveValue);
				MemValue[_memAddr].push_back(reg8lIR);
				irList.push_back(reg8lIR);
			}
		}
	}



	return;
}

Value* GetEAValue(ZydisDecodedInstruction* decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* BaseValue = nullptr;
	Value* Disp = nullptr;
	IR* Scale = nullptr;
	Value* IndexValue = nullptr;

	IR* BaseDisp = nullptr;
	IR* ScaleIndex = nullptr;
	IR* EAValue = nullptr;
	Value* op3;


	Value* foldedValue;

	DWORD targetMem1 = 0;
	DWORD targetMem2 = 0;
	DWORD targetMem3 = 0;
	DWORD targetMem4 = 0;

	DWORD op1ConstValue = 0;
	DWORD op2ConstValue = 0;
	DWORD op3ConstValue = 0;
	DWORD op4ConstValue = 0;
	DWORD foldedConstValue = 0;

	BYTE readByte;

	// 1. EA를 구한다.
	targetMem1 = GetEffectiveAddress32(_decodedOperandPtr, _regdump);
	targetMem2 = targetMem1 + 1;
	targetMem3 = targetMem1 + 2;
	targetMem4 = targetMem1 + 3;

	// 2. EA에 대한 MemValue를 찾는다. Operand Size에 따라서 1바이트(offset1Value1), 2바이트(Concat offset1Value1~offset1Valu2), 4바이트(Concat offset1Value1~offset1Valu4) 단위로 찾는다.
	if (_decodedOperandPtr->mem.base != ZYDIS_REGISTER_NONE)
	{
		BaseValue = GetRegisterValue(_decodedOperandPtr->mem.base, _regdump, irList);

		if (BaseValue == nullptr)
			MessageBoxA(0, "BaseValue is null", "SaveMemoryValue", 0);

		if (_decodedOperandPtr->mem.disp.has_displacement)
		{
			// [Base + Index*Scale + Disp]

			// Base + Disp
			Disp = GetImmValue(_decodedOperandPtr->mem.disp.value, 32, irList);
			BaseDisp = CraeteBinaryIR(BaseValue, Disp, IR::OPR_ADD);
			irList.push_back(BaseDisp);

			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{
				Scale = CreateBVVIR(_decodedOperandPtr->mem.scale, 32);
				irList.push_back(Scale);
				IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
				ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
				irList.push_back(ScaleIndex);
				EAValue = CraeteBinaryIR(BaseDisp, ScaleIndex, IR::OPR_ADD);
				irList.push_back(EAValue);

			}

			// [Base + Disp]
			else
			{
				EAValue = BaseDisp;
			}
		}

		// Displacement가 없는경우
		else
		{
			// [Base + Index*Scale]
			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{
				Scale = CreateBVVIR(_decodedOperandPtr->mem.scale, 32);//GetImmValue(_decodedOperandPtr->mem.scale, 32, irList);
				irList.push_back(Scale);
				if (Scale->z3ExprPtr == nullptr)
					_plugin_logprintf("[%p] Scale->z3ExprPtr is nullptr\n", _regdump.regcontext.cip);

				IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
				if (IndexValue->z3ExprPtr == nullptr)
					_plugin_logprintf("[%p] IndexValue->z3ExprPtr is nullptr\n", _regdump.regcontext.cip);
				ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
				if (ScaleIndex->z3ExprPtr == nullptr)
					_plugin_logprintf("[%p] ScaleIndex->z3ExprPtr is nullptr\n", _regdump.regcontext.cip);
				irList.push_back(ScaleIndex);
				EAValue = CraeteBinaryIR(BaseValue, ScaleIndex, IR::OPR_ADD);
				irList.push_back(EAValue);
				if (EAValue->z3ExprPtr == nullptr)
					_plugin_logprintf("[%p] EAValue->z3ExprPtr is nullptr\n", _regdump.regcontext.cip);
			}
			// [Base]
			else
			{
				Disp = GetImmValue(0, 32, irList);
				EAValue = CraeteBinaryIR(BaseValue, Disp, IR::OPR_ADD);
				irList.push_back(EAValue);
			}
		}
	}

	// 2-2. 오퍼랜드에 Base가 존재하지 않는 경우
	else
	{
		// [index*Scale + Disp]
		if (_decodedOperandPtr->mem.disp.has_displacement)
		{

			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{
				Scale = CreateBVVIR(_decodedOperandPtr->mem.scale, 32);
				irList.push_back(Scale);
				IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
				ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
				irList.push_back(ScaleIndex);

				Disp = GetImmValue(_decodedOperandPtr->mem.disp.value, 32, irList);
				EAValue = CraeteBinaryIR(ScaleIndex, Disp, IR::OPR_ADD);
				irList.push_back(EAValue);
			}

			else
			{
				EAValue = CreateBVVIR(_decodedOperandPtr->mem.disp.value, 32);
				irList.push_back(EAValue);
			}
		}

		// [index * Scale]
		else
		{
			Scale = CreateBVVIR(_decodedOperandPtr->mem.scale, 32);
			irList.push_back(Scale);
			IndexValue = GetRegisterValue(_decodedOperandPtr->mem.index, _regdump, irList);
			ScaleIndex = CraeteBinaryIR(Scale, IndexValue, IR::OPR_MUL);
			irList.push_back(ScaleIndex);
		}
	}

	return EAValue;
}

//#define DEBUG
Value* GetMemoryValue(ZydisDecodedInstruction* decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* BaseValue = nullptr;
	Value* Disp = nullptr;
	Value* op3;
	IR* offset1;
	IR* offset2;
	IR* offset3;
	IR* offset4;

	Value* offsetImm1;
	Value* offsetImm2;
	Value* offsetImm3;
	Value* offsetImm4;

	Value* offset1Value;
	Value* offset2Value;
	Value* offset3Value;
	Value* offset4Value;

	Value* foldedValue;

	DWORD targetMem1 = 0;
	DWORD targetMem2 = 0;
	DWORD targetMem3 = 0;
	DWORD targetMem4 = 0;

	DWORD op1ConstValue = 0;
	DWORD op2ConstValue = 0;
	DWORD op3ConstValue = 0;
	DWORD op4ConstValue = 0;
	DWORD foldedConstValue = 0;

	BYTE readByte;

	// 1. EA를 구한다.
	targetMem1 = GetEffectiveAddress32(_decodedOperandPtr, _regdump);
	targetMem2 = targetMem1 + 1;
	targetMem3 = targetMem1 + 2;
	targetMem4 = targetMem1 + 3;

	// 2. EA에 대한 MemValue를 찾는다. Operand Size에 따라서 1바이트(offset1Value1), 2바이트(Concat offset1Value1~offset1Valu2), 4바이트(Concat offset1Value1~offset1Valu4) 단위로 찾는다.
	if (_decodedOperandPtr->size == 32)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CreateBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
			irList.push_back(rstIR0);
#ifdef DEBUG
			DbgMemRead(targetMem1, &readByte, 1);
			_plugin_logprintf("[GetMemoryValue] [%p] %p :%x\n", _regdump.regcontext.cip, targetMem1, readByte);
#endif
		}
		offset1Value = MemValue[targetMem1].back();
#ifdef DEBUG
		DbgMemRead(targetMem1, &readByte, 1);
		_plugin_logprintf("[GetMemoryValue] [%p] %p :%x\n", _regdump.regcontext.cip, targetMem1, readByte);
#endif
		// Target Memory Address + 1에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem2) == MemValue.end())
		{
			DbgMemRead(targetMem2, &readByte, 1);
			rstIR1 = CreateBVVIR(readByte, 8);
			MemValue[targetMem2].push_back(rstIR1);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem2, readByte);
#endif
			irList.push_back(rstIR1);
		}
		offset2Value = MemValue[targetMem2].back();

		// Target Memory Address + 1 에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem3) == MemValue.end())
		{
			DbgMemRead(targetMem3, &readByte, 1);
			rstIR2 = CreateBVVIR(readByte, 8);
			MemValue[targetMem3].push_back(rstIR2);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem3, readByte);
#endif
			irList.push_back(rstIR2);
		}
		offset3Value = MemValue[targetMem3].back();

		if (MemValue.find(targetMem4) == MemValue.end())
		{
			DbgMemRead(targetMem4, &readByte, 1);
			rstIR3 = CreateBVVIR(readByte, 8);
			MemValue[targetMem4].push_back(rstIR3);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem4, readByte);
#endif
			irList.push_back(rstIR3);
		}
		offset4Value = MemValue[targetMem4].back();

		/*
		if (dynamic_cast<IR*>(offset1Value) &&
			dynamic_cast<IR*>(offset2Value) &&
			dynamic_cast<IR*>(offset3Value) &&
			dynamic_cast<IR*>(offset4Value))
		{
			if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_EXTRACT8L &&
				dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(offset3Value)->opr == IR::OPR::OPR_EXTRACT8HL &&
				dynamic_cast<IR*>(offset4Value)->opr == IR::OPR::OPR_EXTRACT8HH)
			{
				if (dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr == dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr == dynamic_cast<IR*>(offset3Value)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(offset3Value)->Operands[0]->valuePtr == dynamic_cast<IR*>(offset4Value)->Operands[0]->valuePtr)
						{
							if (dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)
							{
								irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr));
								return dynamic_cast<IR*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr);
							}
						}
					}
				}
			}
		}
		*/

		/*
		if (dynamic_cast<IR*>(offset1Value) &&
			dynamic_cast<IR*>(offset2Value) &&
			dynamic_cast<IR*>(offset3Value) &&
			dynamic_cast<IR*>(offset4Value))
		{
			if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(offset3Value)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(offset4Value)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)->intVar << 24;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)->intVar << 16;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset3Value)->Operands[0]->valuePtr)->intVar << 8;
				op4ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset4Value)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue | op4ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				//_plugin_logprintf("[GetMemoryValue] %p foldedConstValue %p :%x\n", _regdump.regcontext.cip, targetMem4, readByte);
				return rstIR;
			}

		}
		*/

		rstIR = new IR(IR::OPR::OPR_CONCAT, offset4Value, offset3Value, offset2Value, offset1Value);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_MEMORY;

		//_plugin_logprintf("\n");
		irList.push_back(rstIR);
	}

	else if (_decodedOperandPtr->size == 16)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CreateBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem1, readByte);
#endif
			irList.push_back(rstIR0);
		}
		offset1Value = MemValue[targetMem1].back();

		// Target Memory Address + 1에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem2) == MemValue.end())
		{
			DbgMemRead(targetMem2, &readByte, 1);
			rstIR1 = CreateBVVIR(readByte, 8);
			MemValue[targetMem2].push_back(rstIR1);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem2, readByte);
#endif
			irList.push_back(rstIR1);
		}
		offset2Value = MemValue[targetMem2].back();

		//if (dynamic_cast<IR*>(offset1Value) &&
		//	dynamic_cast<IR*>(offset2Value))
		//{
		//	if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_BVV &&
		//		dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_BVV)
		//	{
		//		op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)->intVar << 24;
		//		op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)->intVar << 16;
		//		foldedConstValue = op1ConstValue | op2ConstValue;

		//		rstIR = CraeteBVVIR(foldedConstValue, 16);
		//		irList.push_back(rstIR);
		//		return rstIR;
		//	}

		//}

		rstIR = new IR(IR::OPR::OPR_CONCAT, offset1Value, offset2Value);
		rstIR->Size = 16;
		rstIR->OperandType = rstIR->OPERANDTYPE_MEMORY;
		irList.push_back(rstIR);
	}

	else if (_decodedOperandPtr->size == 8)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CreateBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem1, readByte);
#endif
			irList.push_back(rstIR0);
		}
		offset1Value = MemValue[targetMem1].back();
		return offset1Value;
	}

	return rstIR;
}

Value* GetStackMemoryValue(ZydisDecodedInstruction* decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* BaseValue = nullptr;
	Value* Disp = nullptr;
	Value* op3;
	IR* offset1;
	IR* offset2;
	IR* offset3;
	IR* offset4;

	Value* offsetImm1;
	Value* offsetImm2;
	Value* offsetImm3;
	Value* offsetImm4;

	Value* offset1Value;
	Value* offset2Value;
	Value* offset3Value;
	Value* offset4Value;

	Value* foldedValue;

	DWORD targetMem1 = 0;
	DWORD targetMem2 = 0;
	DWORD targetMem3 = 0;
	DWORD targetMem4 = 0;

	DWORD op1ConstValue = 0;
	DWORD op2ConstValue = 0;
	DWORD op3ConstValue = 0;
	DWORD op4ConstValue = 0;
	DWORD foldedConstValue = 0;

	BYTE readByte;

	// 1. EA를 구한다.
	targetMem1 = _regdump.regcontext.csp;
	targetMem2 = targetMem1 + 1;
	targetMem3 = targetMem1 + 2;
	targetMem4 = targetMem1 + 3;

	// 2. EA에 대한 MemValue를 찾는다. Operand Size에 따라서 1바이트(offset1Value1), 2바이트(Concat offset1Value1~offset1Valu2), 4바이트(Concat offset1Value1~offset1Valu4) 단위로 찾는다.
	if (_decodedOperandPtr->size == 32)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CreateBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] [%p] %p :%x\n", _regdump.regcontext.cip, targetMem1, readByte);
#endif
			irList.push_back(rstIR0);
		}
		offset1Value = MemValue[targetMem1].back();
		//_plugin_logprintf("[GetMemoryValue] [%p] %p :%x\n", _regdump.regcontext.cip, targetMem1, readByte);

		// Target Memory Address + 1에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem2) == MemValue.end())
		{
			DbgMemRead(targetMem2, &readByte, 1);
			rstIR1 = CreateBVVIR(readByte, 8);
			MemValue[targetMem2].push_back(rstIR1);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem2, readByte);
#endif
			irList.push_back(rstIR1);
		}
		offset2Value = MemValue[targetMem2].back();

		// Target Memory Address + 1 에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem3) == MemValue.end())
		{
			DbgMemRead(targetMem3, &readByte, 1);
			rstIR2 = CreateBVVIR(readByte, 8);
			MemValue[targetMem3].push_back(rstIR2);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem3, readByte);
#endif
			irList.push_back(rstIR2);
		}
		offset3Value = MemValue[targetMem3].back();

		if (MemValue.find(targetMem4) == MemValue.end())
		{
			DbgMemRead(targetMem4, &readByte, 1);
			rstIR3 = CreateBVVIR(readByte, 8);
			MemValue[targetMem4].push_back(rstIR3);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem4, readByte);
#endif
			irList.push_back(rstIR3);
		}
		offset4Value = MemValue[targetMem4].back();

		/*
		if (dynamic_cast<IR*>(offset1Value) &&
			dynamic_cast<IR*>(offset2Value) &&
			dynamic_cast<IR*>(offset3Value) &&
			dynamic_cast<IR*>(offset4Value))
		{
			if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_EXTRACT8L &&
				dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_EXTRACT8H &&
				dynamic_cast<IR*>(offset3Value)->opr == IR::OPR::OPR_EXTRACT8HL &&
				dynamic_cast<IR*>(offset4Value)->opr == IR::OPR::OPR_EXTRACT8HH)
			{
				if (dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr == dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)
				{
					if (dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr == dynamic_cast<IR*>(offset3Value)->Operands[0]->valuePtr)
					{
						if (dynamic_cast<IR*>(offset3Value)->Operands[0]->valuePtr == dynamic_cast<IR*>(offset4Value)->Operands[0]->valuePtr)
						{
							if (dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)
							{
								irList.push_back(dynamic_cast<IR*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr));
								return dynamic_cast<IR*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr);
							}
						}
					}
				}
			}
		}
		*/

		//if (dynamic_cast<IR*>(offset1Value) &&
		//	dynamic_cast<IR*>(offset2Value) &&
		//	dynamic_cast<IR*>(offset3Value) &&
		//	dynamic_cast<IR*>(offset4Value))
		//{
		//	if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_BVV &&
		//		dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_BVV &&
		//		dynamic_cast<IR*>(offset3Value)->opr == IR::OPR::OPR_BVV &&
		//		dynamic_cast<IR*>(offset4Value)->opr == IR::OPR::OPR_BVV)
		//	{
		//		op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)->intVar << 24;
		//		op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)->intVar << 16;
		//		op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset3Value)->Operands[0]->valuePtr)->intVar << 8;
		//		op4ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset4Value)->Operands[0]->valuePtr)->intVar & 0xff;
		//		foldedConstValue = op1ConstValue | op2ConstValue | op3ConstValue | op4ConstValue;

		//		rstIR = CraeteBVVIR(foldedConstValue, 32);
		//		irList.push_back(rstIR);
		//		//_plugin_logprintf("[GetMemoryValue] %p foldedConstValue %p :%x\n", _regdump.regcontext.cip, targetMem4, readByte);
		//		return rstIR;
		//	}

		//}

		rstIR = new IR(IR::OPR::OPR_CONCAT, offset4Value, offset3Value, offset2Value, offset1Value);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_MEMORY;

		//_plugin_logprintf("\n");
		irList.push_back(rstIR);
	}

	else if (_decodedOperandPtr->size == 16)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CreateBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem1, readByte);
#endif
			irList.push_back(rstIR0);
		}
		offset1Value = MemValue[targetMem1].back();

		// Target Memory Address + 1에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem2) == MemValue.end())
		{
			DbgMemRead(targetMem2, &readByte, 1);
			rstIR1 = CreateBVVIR(readByte, 8);
			MemValue[targetMem2].push_back(rstIR1);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem2, readByte);
#endif
			irList.push_back(rstIR1);
		}
		offset2Value = MemValue[targetMem2].back();

		/*
		if (dynamic_cast<IR*>(offset1Value) &&
			dynamic_cast<IR*>(offset2Value))
		{
			if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)->intVar << 24;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)->intVar << 16;
				foldedConstValue = op1ConstValue | op2ConstValue;

				rstIR = CreateBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}
		*/
		rstIR = new IR(IR::OPR::OPR_CONCAT, offset1Value, offset2Value);
		rstIR->Size = 16;
		rstIR->OperandType = rstIR->OPERANDTYPE_MEMORY;
		irList.push_back(rstIR);
	}

	else if (_decodedOperandPtr->size == 8)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CreateBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem1, readByte);
#endif
			irList.push_back(rstIR0);
		}
		offset1Value = MemValue[targetMem1].back();
		return offset1Value;
	}

	return rstIR;
}

Value* GetImmValue(ZydisDecodedOperand* _decodedOperandPtr, vector<IR*>& irList)
{
	IR* immValue;
	if (_decodedOperandPtr->size == 32)
	{
		immValue = CreateBVVIR(_decodedOperandPtr->imm.value.u, 32);
		irList.push_back(immValue);
		return immValue;
	}

	if (_decodedOperandPtr->size == 16)
	{
		immValue = CreateBVVIR(_decodedOperandPtr->imm.value.u, 16);
		irList.push_back(immValue);
		return immValue;
	}

	if (_decodedOperandPtr->size == 8)
	{
		immValue = CreateBVVIR(_decodedOperandPtr->imm.value.u, 8);
		irList.push_back(immValue);
		return immValue;
	}
}

Value* GetImmValue(DWORD _immValue, BYTE _size, vector<IR*>& irList)
{
	IR* immValue;
	immValue = CreateBVVIR(_immValue, _size);
	irList.push_back(immValue);
	return immValue;

	//if (_size == 32)
	//{
	//	immValue = CraeteBVVIR(_immValue, _size);
	//	irList.push_back(immValue);
	//	return immValue;
	//}

	//if (_size == 16)
	//{
	//	immValue = CraeteBVVIR(_immValue, 16);
	//	irList.push_back(immValue);
	//	return immValue;
	//}

	//if (_size == 8)
	//{
	//	immValue = CraeteBVVIR(_immValue, 8);
	//	irList.push_back(immValue);
	//	return immValue;
	//}
}

Value* GetOperand(ZydisDecodedInstruction* _decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, REGDUMP& _regdump, vector<IR*>& irList)
{
	switch (_decodedOperandPtr->type)
	{
	case ZYDIS_OPERAND_TYPE_REGISTER:
		return GetRegisterValue(_decodedOperandPtr->reg.value, _regdump, irList);
		break;

	case ZYDIS_OPERAND_TYPE_MEMORY:
		return GetMemoryValue(_decodedInstPtr, _decodedOperandPtr, _regdump, irList);
		break;

	case ZYDIS_OPERAND_TYPE_IMMEDIATE:
		return GetImmValue(_decodedOperandPtr, irList);
		break;
	}
}

void SetOperand(ZydisDecodedInstruction* _decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, Value* _valPtr, REGDUMP& _regdump, vector<IR*>& irList)
{
	switch (_decodedOperandPtr->type)
	{
	case ZYDIS_OPERAND_TYPE_REGISTER:
		return SaveRegisterValue(_decodedOperandPtr->reg.value, _valPtr, _decodedOperandPtr->size, irList);
		break;

	case ZYDIS_OPERAND_TYPE_MEMORY:
		return SaveMemoryValue(_decodedInstPtr, _decodedOperandPtr, _valPtr, _regdump, irList);
		break;
	}
}

IR* createParityCalcIR(Value* rst)
{
	Value* initValue;
	Value* Op1;
	Value* Op1temp;
	Value* Op2;
	Value* Op3;
	Value* Op4;
	Value* Op5;
	Value* Op6;
	Value* Op7;
	IR* Op8;

	initValue = CreateBVVIR(1, 1);

	if (initValue->z3ExprPtr == nullptr)
	{
		_plugin_logprintf("[%p] initValue z3ExprPtr is null\n");
	}

	//_plugin_logprintf("[%p] %s\n",initValue->z3ExprPtr->to_string().c_str());
	//MessageBoxA(0, "w4tg", "4g3ty", 0);
	

	Op1temp = CreateExtractIR(7, 7, rst);
	if (Op1temp->z3ExprPtr == nullptr)
	{
		_plugin_logprintf("[%p] Op1temp z3ExprPtr is null\n");
	}

	Op1 = CraeteBinaryIR(initValue, Op1temp, IR::OPR::OPR_XOR);
	if (Op1->z3ExprPtr == nullptr)
	{
		_plugin_logprintf("[%p] Op1 z3ExprPtr is null\n");
	}

	Op2 = CraeteBinaryIR(Op1, CreateExtractIR(6, 6, rst), IR::OPR::OPR_XOR);
	Op3 = CraeteBinaryIR(Op2, CreateExtractIR(5, 5, rst), IR::OPR::OPR_XOR);
	Op4 = CraeteBinaryIR(Op3, CreateExtractIR(4, 4, rst), IR::OPR::OPR_XOR);
	Op5 = CraeteBinaryIR(Op4, CreateExtractIR(3, 3, rst), IR::OPR::OPR_XOR);
	Op6 = CraeteBinaryIR(Op5, CreateExtractIR(2, 2, rst), IR::OPR::OPR_XOR);
	Op7 = CraeteBinaryIR(Op6, CreateExtractIR(1, 1, rst), IR::OPR::OPR_XOR);

	if (Op7->z3ExprPtr == nullptr)
	{
		_plugin_logprintf("[%p] Op7 z3ExprPtr is null\n");
	}

	Op8 = CraeteBinaryIR(Op7, CreateExtractIR(0, 0, rst), IR::OPR::OPR_XOR);

	if (Op8->z3ExprPtr == nullptr)
	{
		_plugin_logprintf("[%p] Op8 z3ExprPtr is null\n");
	}

	return Op8;
}

int CreateIR(ZydisDecodedInstruction* ptr_di, ZydisDecodedOperand* operandPtr, REGDUMP& _regdump, DWORD _offset)
{
	Value* Op1 = nullptr;
	IR* Op1Extend = nullptr;
	Value* Op2 = nullptr;
	IR* Op2Extend = nullptr;
	Value* toExtendVal = nullptr;

	Value* eaxValue = nullptr;
	Value* edxValue = nullptr;

	Value* countValue = nullptr;
	IR* countMaskValue = nullptr;
	IR* tempCountValue = nullptr;
	IR* remains = nullptr;
	IR* body = nullptr;

	Value* StackOp1 = nullptr;
	Value* StackOp2 = nullptr;

	Value* RealOperand;

	IR* rst = nullptr;
	IR* sflagIR = nullptr;
	IR* zflagIR = nullptr;
	IR* oflagIR = nullptr;
	IR* cflagIR = nullptr;
	IR* pflagIR = nullptr;
	IR* ExtractIR = nullptr;
	IR* storeIR = nullptr;
	IR* tempRst = nullptr;
	IR* remaRst = nullptr;

	IR* eaxRst = nullptr;
	IR* edxRst = nullptr;
	IR* concatEdxEaxRst = nullptr;

	vector<IR*> irList;

	//_plugin_logprintf("CreateIR\n");

	switch (ptr_di->mnemonic)
	{
	case ZYDIS_MNEMONIC_ADD:
	{
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.
		if (ptr_di->opcode == 0x83)
		{
			Op2 = CreateBVVIR(operandPtr[1].imm.value.s, Op1->Size);
		}
		else
		{
			Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.
		}
			// 두 개의 오퍼랜드는 동일해야 한다.
			if (Op1->Size != Op2->Size)
			{
				_plugin_logprintf("GenerateOPR_ADD Error (Operand is not matched %p %d %d)\n", _regdump.regcontext.cip, Op1->Size, Op2->Size);
				MessageBoxA(0, "enerateOPR_ADD Error (Operand is not matched %p %d %d)", "test", 0);
				return 0;
			}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_ADD);
		irList.push_back(rst);

		if (Op1->z3ExprPtr == nullptr)
		{
			_plugin_logprintf("[%p] ", _offset);
			MessageBoxA(0, "ADD Op1->z3ExprPtr==nullptr", "test", 0);
		}

		if (Op2->z3ExprPtr == nullptr)
		{
			_plugin_logprintf("[%p] ", _offset);
			MessageBoxA(0, "ADD Op2->z3ExprPtr==nullptr", "test", 0);
		}

		// EFLAG 관련 IR 추가
		if (operandPtr[0].size == 32)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF32);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(rst, CreateBVVIR(0, rst->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF
			// (op1 & op2) | ( (op1 | op2) & (~rst))

			// PF
			if (Op1->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ADD Op1->z3ExprPtr == nullptr\n", cntd);
			}
			if (Op2->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ADD Op2->z3ExprPtr == nullptr\n", cntd);
			}
			if (rst->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ADD rst->z3ExprPtr == nullptr\n", cntd);
			}
			pflagIR = createParityCalcIR(rst);
			//_plugin_logprintf("Test Z3 : %s\n", pflagIR->z3ExprPtr->to_string().c_str());
		}

		else if (operandPtr[0].size == 16)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF16);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(Op1, CreateBVVIR(0, Op1->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		else if (operandPtr[0].size == 8)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF8);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(Op1, CreateBVVIR(0, Op1->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		//_plugin_logprintf("[%p] VM_ADD :%p\n", _regdump.regcontext.cip);
		//vaddList.insert(make_pair(_regdump.regcontext.cip, rst));

		/*
		if (_offset == 0x401156)
		{
			z3::solver test(*z3Context);
			test.add(*rst->z3ExprPtr == z3::expr(z3Context->bv_val((unsigned int)targetByte[g_idx], rst->Size))); // 0x
			auto rstTest = test.check();
			g_idx++;
			if (rstTest)
			{
				_plugin_logprintf("z3 add rst :%s\n", Z3_model_to_string(*z3Context, test.get_model()));
				MessageBoxA(0, "dsggda", "dsgsa", 0);
			}
		}
		*/

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));

		return 1;
	}
		break;

	case ZYDIS_MNEMONIC_MOV:
		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
		SetOperand(ptr_di, &operandPtr[0], Op2, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;

	case ZYDIS_MNEMONIC_SUB:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (ptr_di->opcode == 0x83)
			operandPtr[1].size = operandPtr[0].size;

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("GenerateOPR_SUB Error (Operand is not matched %p %d %d)\n", _regdump.regcontext.cip, Op1->Size, Op2->Size);
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SUB);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가
		if (operandPtr[0].size == 32)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF32);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(rst, CreateBVVIR(0, rst->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF
			// (op1 & op2) | ( (op1 | op2) & (~rst))

		}

		else if (operandPtr[0].size == 16)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF16);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(Op1, CreateBVVIR(0, Op1->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		else if (operandPtr[0].size == 8)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF8);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(Op1, CreateBVVIR(0, Op1->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		//SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;

		//case ZYDIS_MNEMONIC_BTS:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Bts, Op1, Op2, _offset);
		//	return 1;
		//	break;

	case ZYDIS_MNEMONIC_SAR:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (operandPtr[1].size != operandPtr[0].size)
		{
			toExtendVal = CreateZeroExtendIR(Op2, operandPtr[0].size, operandPtr[1].size, irList);
		}

		//if (ptr_di->opcode == 0xC1 || ptr_di->opcode == 0xD0 || ptr_di->opcode == 0xD3)
		//	Op2->Size = Op1->Size;

		//// 두 개의 오퍼랜드는 동일해야 한다.
		//if (Op1->Size != Op2->Size)
		//{
		//	_plugin_logprintf("GenerateOPR_SAR Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
		//	return 0;
		//}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		if (operandPtr[1].size != operandPtr[0].size)
		{
			rst = CraeteBinaryIR(Op1, toExtendVal, IR::OPR::OPR_SAR);
		}
		else
		{
			rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SAR);
		}
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
		//case ZYDIS_MNEMONIC_SHR:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Shr, Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_SHL:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Shl, Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_ROR:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Ror, Op1, Op2, _offset);
		//	return 1;
		//	break;
	case ZYDIS_MNEMONIC_ROL:
		// https://www.geeksforgeeks.org/rotate-bits-of-an-integer/
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.
		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
		countValue = CreateZeroExtendIR(Op2, operandPtr[0].size, operandPtr[1].size, irList);

		if (operandPtr[0].size == 64)
		{
			countMaskValue = CreateBVVIR(0x3f, 64);
		}

		else if (operandPtr[0].size == 32)
		{
			countMaskValue = CreateBVVIR(0x1f, 32);

			// tempCOUNT = (COUNT & COUNTMASK) MOD SIZE
			tempCountValue = CraeteBinaryIR(CraeteBinaryIR(countValue, countMaskValue, IR::OPR::OPR_AND),
				CreateBVVIR(operandPtr[0].size, 32), IR::OPR::OPR_UREM);
			irList.push_back(tempCountValue);

			// Op1 >> (INT_BITS - tempCOUNT)
			remains = CraeteBinaryIR(Op1,
				CraeteBinaryIR(CreateBVVIR(operandPtr[0].size, 32), tempCountValue, IR::OPR::OPR_SUB),
				IR::OPR::OPR_SHR);
			irList.push_back(remains);

			// (Op1 << tempCOUNT) | (INT_BITS - tempCOUNT)
			body = CraeteBinaryIR(Op1, tempCountValue, IR::OPR::OPR_SHL);
			irList.push_back(body);
		}

		else if (operandPtr[0].size == 16)
		{
			countMaskValue = CreateBVVIR(0x1f, 16);

			// tempCOUNT = (COUNT & COUNTMASK) MOD SIZE
			tempCountValue = CraeteBinaryIR(CraeteBinaryIR(countValue, countMaskValue, IR::OPR::OPR_AND),
				CreateBVVIR(operandPtr[0].size, 16), IR::OPR::OPR_UREM);
			irList.push_back(tempCountValue);

			// Op1 >> (INT_BITS - tempCOUNT)
			remains = CraeteBinaryIR(Op1,
				CraeteBinaryIR(CreateBVVIR(operandPtr[0].size, 16), tempCountValue, IR::OPR::OPR_SUB),
				IR::OPR::OPR_SHR);
			irList.push_back(remains);

			// Op1 << tempCOUNT)
			body = CraeteBinaryIR(Op1, tempCountValue, IR::OPR::OPR_SHL);
			irList.push_back(body);
		}

		else if (operandPtr[0].size == 8)
		{
			countMaskValue = CreateBVVIR(0x1f, 8);

			// tempCOUNT = (COUNT & COUNTMASK) MOD SIZE
			tempCountValue = CraeteBinaryIR(CraeteBinaryIR(countValue, countMaskValue, IR::OPR::OPR_AND),
				CreateBVVIR(operandPtr[0].size, 8), IR::OPR::OPR_UREM);
			irList.push_back(tempCountValue);

			// Op1 >> (INT_BITS - tempCOUNT)
			remains = CraeteBinaryIR(Op1,
				CraeteBinaryIR(CreateBVVIR(operandPtr[0].size, 8), tempCountValue, IR::OPR::OPR_SUB),
				IR::OPR::OPR_SHR);
			irList.push_back(remains);

			// (Op1 << tempCOUNT)
			body = CraeteBinaryIR(Op1, tempCountValue, IR::OPR::OPR_SHL);
			irList.push_back(body);
		}

		else
		{
			break;
		}

		// (Op1 << tempCOUNT) | (INT_BITS - tempCOUNT)
		rst = CraeteBinaryIR(body, remains, IR::OPR::OPR_OR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		break;

	case ZYDIS_MNEMONIC_OR:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (ptr_di->opcode == 0x83)
			operandPtr[1].size = operandPtr[0].size;

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

														 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_OR Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_OR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_ADC:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("GenerateOPR_ADC Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_ADC);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_SBB:
	{
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.
		
				// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("GenerateOPR_ADC Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			return 0;
		}

		Value* cfValue = nullptr;
		IR* toSub = nullptr;

		if (RegValue[REG_CFLAG].size() != 0)
		{
			cfValue = RegValue[REG_CFLAG].back();
			auto zextCf = CreateZeroExtendIR(cfValue, Op2->Size, cfValue->Size, irList);
			toSub = CraeteBinaryIR(Op2, zextCf, IR::OPR::OPR_ADD);
			irList.push_back(toSub);
			rst = CraeteBinaryIR(Op1, toSub, IR::OPR::OPR_SUB);
			irList.push_back(rst);
		}

		else
		{
			rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SBB);
			irList.push_back(rst);
		}

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
	}
		break;
	case ZYDIS_MNEMONIC_AND:
		Value* tempOp2;
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.
		tempOp2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (ptr_di->opcode == 0x83)
		{
			if (operandPtr[0].size != operandPtr[1].size)
			{
				Op2 = CreateSignExtendIR(tempOp2, operandPtr[0].size, operandPtr[1].size, irList);
				//_plugin_logprintf("[%p] ", _offset);
				if (Op2->z3ExprPtr == nullptr)
					MessageBoxA(0, "AND Signextend Op2->z3ExprPtr==nullptr", "test", 0);
			}
			else
				Op2 = tempOp2;
		}

		else
			Op2 = tempOp2;// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("Generate OPR_AND Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			MessageBoxA(0, "Test", "AND", 0);
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_AND);
		irList.push_back(rst);

		if (Op1->z3ExprPtr == nullptr)
		{
			_plugin_logprintf("[%p] ", _offset);
			MessageBoxA(0, "AND Op1->z3ExprPtr==nullptr", "test", 0);
		}

		if (Op2->z3ExprPtr == nullptr)
		{
			_plugin_logprintf("[%p] ", _offset);
			MessageBoxA(0, "AND Op2->z3ExprPtr==nullptr", "test", 0);
		}

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		//SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_DAA:
		break;
	case ZYDIS_MNEMONIC_DAS:
		break;
	case ZYDIS_MNEMONIC_XOR:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (ptr_di->opcode == 0x83)
			operandPtr[1].size = operandPtr[0].size;
		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_XOR Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_XOR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가
		if (operandPtr[0].size == 32)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF32);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CreateBVVIR(0, 1);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF
			// (op1 & op2) | ( (op1 | op2) & (~rst))

		}

		else if (operandPtr[0].size == 16)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF16);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CreateBVVIR(0, 1);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		else if (operandPtr[0].size == 8)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF8);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CreateBVVIR(0, 1);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_AAA:
		break;
	case ZYDIS_MNEMONIC_CMP:
	{
		Value* op2RealValue = nullptr;

		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.
		
		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			if (Op1->Size == 32 && Op2->Size == 8)
			{
				op2RealValue = CreateZeroExtendIR(Op2, 32, 8, irList);
			}

			else
			{
				_plugin_logprintf("GenerateOPR_CMP Error (Operand is not matched %p %d %d)\n", _regdump.regcontext.cip, Op1->Size, Op2->Size);
				return 0;
			}
		}

		else
		{
			op2RealValue = Op2;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, op2RealValue, IR::OPR::OPR_SUB);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가
		if (operandPtr[0].size == 32)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF32);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(rst, CreateBVVIR(0, rst->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF
			// (op1 & op2) | ( (op1 | op2) & (~rst))

		}

		else if (operandPtr[0].size == 16)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF16);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(rst, CreateBVVIR(0, rst->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}

		else if (operandPtr[0].size == 8)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF8);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			zflagIR = CraeteBinaryIR(rst, CreateBVVIR(0, rst->Size), IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			// (op1 ^ rst) & (op2 ^ rst)
			oflagIR;

			// CF

		}
		IRList.insert(make_pair(_offset, irList));
	}
	break;
	case ZYDIS_MNEMONIC_AAS:
		break;
	case ZYDIS_MNEMONIC_INC:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = CreateBVVIR(1, operandPtr[0].size);
		irList.push_back(dynamic_cast<IR*>(Op2));

		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_ADD);
		irList.push_back(rst);
		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_DEC:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = CreateBVVIR(1, operandPtr[0].size);
		irList.push_back(dynamic_cast<IR*>(Op2));

		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SUB);
		irList.push_back(rst);
		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		return 1;
	case ZYDIS_MNEMONIC_PUSH:
	{
		Value* srcValue = nullptr;
		// ESP		
		Op1 = GetRegisterValue(ZYDIS_REGISTER_ESP, _regdump, irList);
		Op2 = GetImmValue(4, 32, irList);
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SUB);
		irList.push_back(rst);
		SaveRegisterValue(ZYDIS_REGISTER_ESP, rst, operandPtr[0].size, irList);
		srcValue = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
		
		if (ptr_di->operand_width != operandPtr[0].size)
		{
			RealOperand = CreateSignExtendIR(srcValue, ptr_di->operand_width, operandPtr[0].size, irList);
		}

		else
		{
			RealOperand = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
		}
		SaveMemoryValue(ZYDIS_REGISTER_ESP, RealOperand, operandPtr[0].size, _regdump, irList);

		IRList.insert(make_pair(_offset, irList));
	}
		break;
	case ZYDIS_MNEMONIC_POP:
	{
		// 예상
		// t1 = Load ESP t2 = ESP 
		Op1 = GetStackMemoryValue(ptr_di, &operandPtr[0], _regdump, irList); // Stack의 값을 읽는다.

		//Op2 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // Stack에서 읽어온 값을 가져온다.

		StackOp1 = GetRegisterValue(ZYDIS_REGISTER_ESP, _regdump, irList);
		StackOp2 = GetImmValue(4, 32, irList);
		rst = CraeteBinaryIR(StackOp1, StackOp2, IR::OPR::OPR_ADD);
		irList.push_back(rst); // ESP = ESP + 4

		SetOperand(ptr_di, &operandPtr[0], Op1, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
	}
		break;
	case ZYDIS_MNEMONIC_PUSHAD:
		break;
	case ZYDIS_MNEMONIC_POPAD:
		break;
	case ZYDIS_MNEMONIC_ARPL:
		break;
	case ZYDIS_MNEMONIC_BOUND:
		break;
	case ZYDIS_MNEMONIC_IMUL:
	{	
		if (ptr_di->operand_count == 1)
		{
			if (operandPtr[0].size == 32)
			{
				// Zero Extend Op1
				Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 64, 32, irList);

				// Get EAX
				Op2 = GetRegisterValue(ZYDIS_REGISTER_EAX, _regdump, irList);
				Op2Extend = CreateSignExtendIR(Op2, 64, 32, irList);

				rst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);

				eaxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT32L);
				SaveRegisterValue(ZYDIS_REGISTER_EAX, eaxRst, 32, irList);

				edxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT32H);
				SaveRegisterValue(ZYDIS_REGISTER_EDX, edxRst, 32, irList);
			}

			if (operandPtr[0].size == 16)
			{
				Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 32, 16, irList);

				// Get EAX
				Op2 = GetRegisterValue(ZYDIS_REGISTER_AX, _regdump, irList);
				Op2Extend = CreateSignExtendIR(Op2, 32, 16, irList);

				rst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);

				eaxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT16L);
				SaveRegisterValue(ZYDIS_REGISTER_AX, eaxRst, 16, irList);

				edxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT16H);
				SaveRegisterValue(ZYDIS_REGISTER_DX, edxRst, 16, irList);
			}

			if (operandPtr[0].size == 8)
			{
				Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 16, 8, irList);

				// Get AL
				Op2 = GetRegisterValue(ZYDIS_REGISTER_AL, _regdump, irList);
				Op2Extend = CreateSignExtendIR(Op2, 16, 8, irList);

				eaxRst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
				SaveRegisterValue(ZYDIS_REGISTER_AX, eaxRst, 16, irList);
			}
		}

		else if (ptr_di->operand_count == 2)
		{
			if (operandPtr[0].size == 32)
			{
				// Sign Extend Op1
				Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 64, 32, irList);
				irList.push_back(Op1Extend);

				// Sign Extend Op2
				Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
				Op2Extend = CreateSignExtendIR(Op2, 64, 32, irList);
				irList.push_back(Op2Extend);

				tempRst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
				irList.push_back(tempRst);

				rst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT32L);
				irList.push_back(rst);

				SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
				IRList.insert(make_pair(_offset, irList));
			}

			if (operandPtr[0].size == 16)
			{
				// Sign Extend Op1
				Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 32, 16, irList);
				irList.push_back(Op1Extend);

				// Sign Extend Op2
				Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
				Op2Extend = CreateSignExtendIR(Op2, 32, 16, irList);
				irList.push_back(Op2Extend);

				tempRst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
				irList.push_back(tempRst);

				rst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT16L);
				irList.push_back(rst);

				SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
				IRList.insert(make_pair(_offset, irList));
			}
		}

		else if (ptr_di->operand_count == 4)
		{
			if (operandPtr[0].size == 32)
			{
				// Sign Extend Op1
				Op1 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 64, 32, irList);
				irList.push_back(Op1Extend);

				// Sign Extend Op2
				Op2 = GetOperand(ptr_di, &operandPtr[2], _regdump, irList);
				Op2Extend = CreateZeroExtendIR(Op2, 64, operandPtr[2].size, irList);
				irList.push_back(Op2Extend);

				tempRst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
				irList.push_back(tempRst);

				rst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT32L);
				irList.push_back(rst);

				SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
				IRList.insert(make_pair(_offset, irList));
			}

			if (operandPtr[0].size == 16)
			{
				// Sign Extend Op1
				Op1 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
				Op1Extend = CreateSignExtendIR(Op1, 32, 16, irList);
				irList.push_back(Op1Extend);

				// Sign Extend Op2
				Op2 = GetOperand(ptr_di, &operandPtr[2], _regdump, irList);
				Op2Extend = CreateZeroExtendIR(Op2, 32, operandPtr[2].size, irList);
				irList.push_back(Op2Extend);

				tempRst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
				irList.push_back(tempRst);

				rst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT16L);
				irList.push_back(rst);

				SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
				IRList.insert(make_pair(_offset, irList));
			}
		}
	}
		break;
	case ZYDIS_MNEMONIC_INSB:
		break;
	case ZYDIS_MNEMONIC_INSW:
		break;
	case ZYDIS_MNEMONIC_INSD:
		break;
	case ZYDIS_MNEMONIC_OUTSB:
		break;
	case ZYDIS_MNEMONIC_OUTSW:
		break;
	case ZYDIS_MNEMONIC_OUTSD:
		break;
	case ZYDIS_MNEMONIC_JO:
		break;
	case ZYDIS_MNEMONIC_JNO:
		break;
	case ZYDIS_MNEMONIC_JB:
		break;
	case ZYDIS_MNEMONIC_JNB:
		break;
	case ZYDIS_MNEMONIC_JZ:
		if (ptr_di->machine_mode == ZYDIS_MACHINE_MODE_LONG_COMPAT_32)
		{

			// t1 = GetZF()
			// t2 = GetEIP
			// t3 = CreateBVV(length)
			// ite (t1 0) (eip = t2 + )(eip = add t2 t3)

			ZyanU64 brTrue = 0;
			ZyanU64 brFalse = 0;

			Value* brTrueValue = nullptr;
			Value* brFalseValue = nullptr;
			Value* zfValue = nullptr;
			Value* condValue = nullptr;
			Value* eipValue = nullptr;

			ZydisCalcAbsoluteAddress(ptr_di, &operandPtr[0], _offset, &brTrue);
			brFalse = _offset + ptr_di->length;

			brTrueValue = GetImmValue(brTrue, 32, irList);
			brFalseValue = GetImmValue(brFalse, 32, irList);

			if (brTrueValue == nullptr || brFalseValue == nullptr)
			{
				_plugin_logprintf("brTrueValue or brFalseValue is nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			zfValue = RegValue[REG_ZFLAG].back();
			if (zfValue == nullptr)
			{
				_plugin_logprintf("zfValue == nullptr\n");
				MessageBoxA(0, "Test", "zfValue == nullptr", 0);
			}

			// ZF == 1
			condValue = CraeteBinaryIR(zfValue, CreateBVVIR(1, zfValue->Size), IR::OPR::OPR_ISEQUAL);
			if (condValue == nullptr)
			{
				_plugin_logprintf("condValue == nullptr\n");
				MessageBoxA(0, "Test", "condValue == nullptr", 0);
			}

			if (condValue->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("condValue->z3ExprPtr == nullptr\n");
				MessageBoxA(0, "Test", "condValue->z3ExprPtr == nullptr", 0);
			}
			if (condValue->z3ExprPtr->is_bool())
				eipValue = new IR(IR::OPR::OPR_BRC, condValue, brTrueValue, brFalseValue);

			// brTrue와 brFalse 중 취해야 할 경로를 판단한다			
			if ((eipValue->z3ExprPtr != nullptr) && (brTrueValue->z3ExprPtr != nullptr) && (brFalseValue->z3ExprPtr != nullptr))
			{

				z3::solver test(*z3Context);
				//test.add(z3::ite(*zfValue->z3ExprPtr == 1, *brTrueValue->z3ExprPtr, *brFalseValue->z3ExprPtr) == *brTrueValue->z3ExprPtr);
				//test.add(*eipValue->z3ExprPtr == *brTrueValue->z3ExprPtr);
				test.add(z3::expr(*eipValue->z3ExprPtr != z3::expr(z3Context->bv_val((unsigned int)0x4011C3, 32))));
				test.add(*(MemValue[0x400000 + 0x3391].front()->z3ExprPtr) < z3::expr(z3Context->bv_val((unsigned int)0x7f, 8)));
				//test.add(*(MemValue[0x400000 + 0x336d].front()->z3ExprPtr) < z3::expr(z3Context->bv_val((unsigned int)0x7f, 8)));
				//_plugin_logprintf("z3 jz test %s\n", z3::expr(*eipValue->z3ExprPtr == *brTrueValue->z3ExprPtr).simplify().to_string().c_str());
				auto rstTest = test.check();

				if (rstTest)
					_plugin_logprintf("z3 jz rst :%s\n", Z3_model_to_string(*z3Context, test.get_model()));
			}

		}
		break;
	case ZYDIS_MNEMONIC_JNZ:
		if (ptr_di->machine_mode == ZYDIS_MACHINE_MODE_LONG_COMPAT_32)
		{

			// t1 = GetZF()
			// t2 = GetEIP
			// t3 = CreateBVV(length)
			// ite (t1 0) (eip = t2 + )(eip = add t2 t3)

			ZyanU64 brTrue = 0;
			ZyanU64 brFalse = 0;

			Value* brTrueValue = nullptr;
			Value* brFalseValue = nullptr;
			Value* zfValue = nullptr;
			Value* condValue = nullptr;
			Value* eipValue = nullptr;

			ZydisCalcAbsoluteAddress(ptr_di, &operandPtr[0], _offset, &brTrue);
			brFalse = _offset + ptr_di->length;

			brTrueValue = GetImmValue(brTrue, 32, irList);
			brFalseValue = GetImmValue(brFalse, 32, irList);

			if (brTrueValue == nullptr | brFalseValue == nullptr)
			{
				_plugin_logprintf("brTrueValue or brFalseValue is nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			zfValue = RegValue[REG_ZFLAG].back();
			if (zfValue == nullptr)
			{
				_plugin_logprintf("zfValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			// ZF == 0
			condValue = CraeteBinaryIR(zfValue, CreateBVVIR(0, zfValue->Size), IR::OPR::OPR_ISEQUAL);
			if (condValue == nullptr)
			{
				_plugin_logprintf("condValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			if (condValue->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("condValue->z3ExprPtr == nullptr zfValue size :%d\n", zfValue->Size);
				MessageBoxA(0, "Test", "Test", 0);
			}

			if (condValue->z3ExprPtr->is_bool())
				eipValue = new IR(IR::OPR::OPR_BRC, condValue, brTrueValue, brFalseValue);

			// brTrue와 brFalse 중 취해야 할 경로를 판단한다			
			if ((eipValue->z3ExprPtr != nullptr) && (brTrueValue->z3ExprPtr != nullptr) && (brFalseValue->z3ExprPtr != nullptr))
			{				
				z3::solver test(*z3Context);
				//test.add(z3::ite(*zfValue->z3ExprPtr == 1, *brTrueValue->z3ExprPtr, *brFalseValue->z3ExprPtr) == *brFalseValue->z3ExprPtr);
				test.add(z3::expr(*eipValue->z3ExprPtr != z3::expr(z3Context->bv_val((unsigned int)0x4011C3, 32)))); //004011C3
				test.add(*(MemValue[0x400000 + 0x339D].front()->z3ExprPtr) < z3::expr(z3Context->bv_val((unsigned int)0x7f, 8)));
				//test.add(*eipValue->z3ExprPtr == *brFalseValue->z3ExprPtr);
				//_plugin_logprintf("[%p] z3 jnz test %s\n", _offset, z3::expr(*eipValue->z3ExprPtr == *brFalseValue->z3ExprPtr).simplify().to_string().c_str());
				auto rstTest = test.check();

				if (rstTest)
					_plugin_logprintf("[%p] z3 jnz rst :%s\n", _offset, Z3_model_to_string(*z3Context, test.get_model()));
			}

		}
		break;
	case ZYDIS_MNEMONIC_JBE:
		break;
	case ZYDIS_MNEMONIC_JNBE:
		break;
	case ZYDIS_MNEMONIC_JS:
		if (ptr_di->machine_mode == ZYDIS_MACHINE_MODE_LONG_COMPAT_32)
		{

			// t1 = GetZF()
			// t2 = GetEIP
			// t3 = CreateBVV(length)
			// ite (t1 0) (eip = t2 + )(eip = add t2 t3)

			ZyanU64 brTrue = 0;
			ZyanU64 brFalse = 0;

			Value* brTrueValue = nullptr;
			Value* brFalseValue = nullptr;
			Value* sfValue = nullptr;
			Value* condValue = nullptr;
			Value* eipValue = nullptr;

			ZydisCalcAbsoluteAddress(ptr_di, &operandPtr[0], _offset, &brTrue);
			brFalse = _offset + ptr_di->length;

			brTrueValue = GetImmValue(brTrue, 32, irList);
			brFalseValue = GetImmValue(brFalse, 32, irList);

			if (brTrueValue == nullptr | brFalseValue == nullptr)
			{
				_plugin_logprintf("brTrueValue or brFalseValue is nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			sfValue = RegValue[REG_SFLAG].back();
			if (sfValue == nullptr)
			{
				_plugin_logprintf("zfValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			// SF == 1
			condValue = CraeteBinaryIR(sfValue, CreateBVVIR(1, 1), IR::OPR::OPR_ISEQUAL);
			if (condValue == nullptr)
			{
				_plugin_logprintf("condValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}
			if (condValue->z3ExprPtr->is_bool())
				eipValue = new IR(IR::OPR::OPR_BRC, condValue, brTrueValue, brFalseValue);

			if (eipValue == nullptr)
			{
				_plugin_logprintf("eipValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			// brTrue와 brFalse 중 취해야 할 경로를 판단한다			
			if ((eipValue->z3ExprPtr != nullptr) && (brTrueValue->z3ExprPtr != nullptr) && (brFalseValue->z3ExprPtr != nullptr))
			{

				z3::solver test(*z3Context);
				//test.add(z3::ite(*zfValue->z3ExprPtr == 1, *brTrueValue->z3ExprPtr, *brFalseValue->z3ExprPtr) == *brTrueValue->z3ExprPtr);
				test.add(*eipValue->z3ExprPtr == *brTrueValue->z3ExprPtr);
				_plugin_logprintf("z3 js test %s\n", z3::expr(*eipValue->z3ExprPtr == *brTrueValue->z3ExprPtr).to_string().c_str());
				auto rstTest = test.check();

				if (rstTest)
					_plugin_logprintf("z3 js rst :%s\n", Z3_model_to_string(*z3Context, test.get_model()));
			}

		}
		break;
	case ZYDIS_MNEMONIC_JNS:
		if (ptr_di->machine_mode == ZYDIS_MACHINE_MODE_LONG_COMPAT_32)
		{

			// t1 = GetZF()
			// t2 = GetEIP
			// t3 = CreateBVV(length)
			// ite (t1 0) (eip = t2 + )(eip = add t2 t3)

			ZyanU64 brTrue = 0;
			ZyanU64 brFalse = 0;

			Value* brTrueValue = nullptr;
			Value* brFalseValue = nullptr;
			Value* sfValue = nullptr;
			Value* condValue = nullptr;
			Value* eipValue = nullptr;

			ZydisCalcAbsoluteAddress(ptr_di, &operandPtr[0], _offset, &brTrue);
			brFalse = _offset + ptr_di->length;

			brTrueValue = GetImmValue(brTrue, 32, irList);
			brFalseValue = GetImmValue(brFalse, 32, irList);

			if (brTrueValue == nullptr | brFalseValue == nullptr)
			{
				_plugin_logprintf("brTrueValue or brFalseValue is nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			sfValue = RegValue[REG_SFLAG].back();
			if (sfValue == nullptr)
			{
				_plugin_logprintf("zfValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			// SF == 1
			condValue = CraeteBinaryIR(sfValue, CreateBVVIR(0, 1), IR::OPR::OPR_ISEQUAL);
			if (condValue == nullptr)
			{
				_plugin_logprintf("condValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}
			if (condValue->z3ExprPtr->is_bool())
				eipValue = new IR(IR::OPR::OPR_BRC, condValue, brTrueValue, brFalseValue);

			if (eipValue == nullptr)
			{
				_plugin_logprintf("eipValue == nullptr\n");
				MessageBoxA(0, "Test", "Test", 0);
			}

			// brTrue와 brFalse 중 취해야 할 경로를 판단한다			
			if ((eipValue->z3ExprPtr != nullptr) && (brTrueValue->z3ExprPtr != nullptr) && (brFalseValue->z3ExprPtr != nullptr))
			{

				z3::solver test(*z3Context);
				//test.add(z3::ite(*zfValue->z3ExprPtr == 1, *brTrueValue->z3ExprPtr, *brFalseValue->z3ExprPtr) == *brTrueValue->z3ExprPtr);
				test.add(*eipValue->z3ExprPtr == *brTrueValue->z3ExprPtr);
				_plugin_logprintf("z3 js test %s\n", z3::expr(*eipValue->z3ExprPtr == *brTrueValue->z3ExprPtr).to_string().c_str());
				auto rstTest = test.check();

				if (rstTest)
					_plugin_logprintf("z3 js rst :%s\n", Z3_model_to_string(*z3Context, test.get_model()));
			}

		}
		break;
	case ZYDIS_MNEMONIC_JP:
		break;
	case ZYDIS_MNEMONIC_JNP:
		break;
	case ZYDIS_MNEMONIC_JL:
		break;
	case ZYDIS_MNEMONIC_JNL:
		break;
	case ZYDIS_MNEMONIC_JLE:
		break;
	case ZYDIS_MNEMONIC_JNLE:
		break;
	case ZYDIS_MNEMONIC_TEST:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("[%p] GenerateOPR_TEST Error (Operand is not matched)\n", _regdump.regcontext.cip);
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_AND);
		irList.push_back(rst);

		if (operandPtr[0].size == 32)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF32);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			auto tempZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempZero);
			zflagIR = CraeteBinaryIR(rst, tempZero,IR::OPR::OPR_ITE);
			irList.push_back(zflagIR);
			RegValue[REG_ZFLAG].push_back(zflagIR);

			// OF
			auto tempOfZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempOfZero);
			RegValue[REG_OFLAG].push_back(tempOfZero);

			// CF
			auto tempCfZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempCfZero);
			RegValue[REG_CFLAG].push_back(tempCfZero);

			// PF
			pflagIR = createParityCalcIR(rst);
			if (pflagIR)
				irList.push_back(pflagIR);
		}

		else if (operandPtr[0].size == 16)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF16);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			auto tempZfZero = CreateBVVIR(0, Op1->Size);
			irList.push_back(tempZfZero);
			zflagIR = CraeteBinaryIR(Op1, tempZfZero, IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			auto tempOfZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempOfZero);
			RegValue[REG_OFLAG].push_back(tempOfZero);

			// CF
			auto tempCfZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempCfZero);
			RegValue[REG_CFLAG].push_back(tempCfZero);

			// PF
			pflagIR = createParityCalcIR(rst);
			if (pflagIR)
				irList.push_back(pflagIR);

		}

		else if (operandPtr[0].size == 8)
		{
			// SF
			sflagIR = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACTSF8);
			RegValue[REG_SFLAG].push_back(sflagIR);
			irList.push_back(sflagIR);

			// ZF
			auto tempZfZero = CreateBVVIR(0, Op1->Size);
			irList.push_back(tempZfZero);
			zflagIR = CraeteBinaryIR(Op1, tempZfZero, IR::OPR::OPR_ITE);
			RegValue[REG_ZFLAG].push_back(zflagIR);
			irList.push_back(zflagIR);

			// OF
			auto tempOfZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempOfZero);
			RegValue[REG_OFLAG].push_back(tempOfZero);

			// CF
			auto tempCfZero = CreateBVVIR(0, rst->Size);
			irList.push_back(tempCfZero);
			RegValue[REG_CFLAG].push_back(tempCfZero);

			// PF
			pflagIR = createParityCalcIR(rst);
			if (pflagIR)
				irList.push_back(pflagIR);

		}

		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_XCHG:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
		SetOperand(ptr_di, &operandPtr[0], Op2, _regdump, irList);
		SetOperand(ptr_di, &operandPtr[1], Op1, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_LEA:
		Op2 = GetEAValue(ptr_di, &operandPtr[1], _regdump, irList);
		SetOperand(ptr_di, &operandPtr[0], Op2, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_CWD:
		break;
	case ZYDIS_MNEMONIC_CBW:
		break;
	case ZYDIS_MNEMONIC_CDQ:
	{
		Op1 = GetRegisterValue(ZYDIS_REGISTER_EAX, _regdump, irList);
		IR* signExtIR = nullptr;
		IR* edxIR = nullptr;
		if (ptr_di->operand_width == 32)
		{
			signExtIR = CreateSignExtendIR(Op1, 64, 32, irList);
			edxIR = CraeteUnaryIR(signExtIR, IR::OPR::OPR_EXTRACT32H);
			SaveRegisterValue(ZYDIS_REGISTER_EDX, edxIR, 32, irList);
		}
	}
		break;
	case ZYDIS_MNEMONIC_PUSHFD:
		// ESP		
		Op1 = GetRegisterValue(ZYDIS_REGISTER_ESP, _regdump, irList);
		Op2 = GetImmValue(4, 32, irList);
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SUB);
		irList.push_back(rst);
		SaveRegisterValue(ZYDIS_REGISTER_ESP, rst, 32, irList);
		//RealOperand = RegValue[REG_EFLAGS].back();

		//SaveMemoryValue(ZYDIS_REGISTER_ESP, RealOperand, 32, _regdump, irList);
		//IRList.insert(make_pair(_offset, irList));
		//_plugin_logprintf("[%p] PUSHFD\n", _regdump.regcontext.cip);
		break;
	case ZYDIS_MNEMONIC_POPFD:
		Op1 = GetStackMemoryValue(ptr_di, &operandPtr[0], _regdump, irList); // Stack의 값을 읽는다.
		StackOp1 = GetRegisterValue(ZYDIS_REGISTER_ESP, _regdump, irList);
		StackOp2 = GetImmValue(4, 32, irList);
		rst = CraeteBinaryIR(StackOp1, StackOp2, IR::OPR::OPR_ADD);
		irList.push_back(rst); // ESP = ESP + 4

		//RegValue[REG_EFLAGS].push_back(Op1);
		//SetOperand(ptr_di, &operandPTr[0], Op1, _regdump, irList);
		//IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_SAHF:
		break;
	case ZYDIS_MNEMONIC_LAHF:
		break;
	case ZYDIS_MNEMONIC_MOVSB:
		break;
	case ZYDIS_MNEMONIC_MOVSW:
		break;
	case ZYDIS_MNEMONIC_MOVSD:
		break;
	case ZYDIS_MNEMONIC_CMPSB:
	case ZYDIS_MNEMONIC_CMPSW:
	case ZYDIS_MNEMONIC_CMPSD:
		break;
	case ZYDIS_MNEMONIC_STOSB:
		break;
	case ZYDIS_MNEMONIC_STOSW:
		break;
	case ZYDIS_MNEMONIC_STOSD:
		break;
	case ZYDIS_MNEMONIC_LODSB:
		break;
	case ZYDIS_MNEMONIC_LODSW:
		break;
	case ZYDIS_MNEMONIC_LODSD:
		break;
	case ZYDIS_MNEMONIC_SCASB:
		break;
	case ZYDIS_MNEMONIC_SCASW:
		break;
	case ZYDIS_MNEMONIC_SCASD:
		break;
	case ZYDIS_MNEMONIC_SHL:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.
		if (operandPtr[1].size != operandPtr[0].size)
		{
			toExtendVal = CreateZeroExtendIR(Op2, operandPtr[0].size, operandPtr[1].size, irList);
		}
		//if (ptr_di->opcode == 0xC1 || ptr_di->opcode == 0xD0 || ptr_di->opcode == 0xD2 || ptr_di->opcode == 0xD3)
		//	Op2->Size = Op1->Size;

		//// 두 개의 오퍼랜드는 동일해야 한다.

		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		if (operandPtr[1].size != operandPtr[0].size)
		{
			rst = CraeteBinaryIR(Op1, toExtendVal, IR::OPR::OPR_SHL);
		}
		else
		{
			rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SHL);
		}
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		//SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_SHR:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (operandPtr[1].size != operandPtr[0].size)
		{
			toExtendVal = CreateZeroExtendIR(Op2, operandPtr[0].size, operandPtr[1].size, irList);
			if (toExtendVal->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ", _offset);
				MessageBoxA(0, "SHR toExtendVal->z3ExprPtr == nullptr", "test", 0);
			}
		}
		//if (ptr_di->opcode == 0xC1 || ptr_di->opcode == 0xD0 || ptr_di->opcode == 0xD2 ||ptr_di->opcode == 0xD3)
		//	Op2->Size = Op1->Size;

		//// 두 개의 오퍼랜드는 동일해야 한다.
		//if (Op1->Size != Op2->Size)
		//{
		//	_plugin_logprintf("GenerateOPR_SHR Error (Operand is not matched %p) %d %d\n", _regdump.regcontext.cip, Op1->Size, Op2->Size);
		//	return 0;
		//}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		if (operandPtr[1].size != operandPtr[0].size)
		{
			rst = CraeteBinaryIR(Op1, toExtendVal, IR::OPR::OPR_SHR);
			if (Op1->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ", _offset);
				MessageBoxA(0, "SHR Op1->z3ExprPtr == nullptr", "test", 0);
			}
		}
		else
		{
			rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SHR);
			if (Op1->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ", _offset);
				MessageBoxA(0, "SHR Op1->z3ExprPtr == nullptr", "test", 0);
			}

			if (Op2->z3ExprPtr == nullptr)
			{
				_plugin_logprintf("[%p] ", _offset);
				MessageBoxA(0, "SHR Op2->z3ExprPtr == nullptr", "test", 0);
			}
		}
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		//SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_RET:
		break;
	case ZYDIS_MNEMONIC_LES:
		break;
	case ZYDIS_MNEMONIC_LDS:
		break;
	case ZYDIS_MNEMONIC_ENTER:
		break;
	case ZYDIS_MNEMONIC_LEAVE:
		break;
	case ZYDIS_MNEMONIC_INT3:
		break;
	case ZYDIS_MNEMONIC_INT:
		break;
	case ZYDIS_MNEMONIC_INTO:
		break;
	case ZYDIS_MNEMONIC_IRETD:
		break;
	case ZYDIS_MNEMONIC_RCL:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		//if (Op1->Size != Op2->Size)
		//{
		//	printf("GenerateOPR_RCL Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
		//	return 0;
		//}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_RCL);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPtr[0].reg.value, rst, operandPtr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_RCR:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		//if (ptr_di->opcode == 0xD3 || ptr_di->opcode == 0xD0)
		//	Op2->Size = Op1->Size;

		//// 두 개의 오퍼랜드는 동일해야 한다.
		//if (Op1->Size != Op2->Size)
		//{
		//	_plugin_logprintf("GenerateOPR_RCR Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
		//	return 0;
		//}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_RCR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		//SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_ROR:
		// https://www.geeksforgeeks.org/rotate-bits-of-an-integer/
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.
		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
		countValue = CreateZeroExtendIR(Op2, operandPtr[0].size, operandPtr[1].size, irList);

		if (operandPtr[0].size == 64)
		{
			countMaskValue = CreateBVVIR(0x3f, 64);
		}

		else if (operandPtr[0].size == 32)
		{
			countMaskValue = CreateBVVIR(0x1f, 32);

			// tempCOUNT = (COUNT & COUNTMASK) MOD SIZE
			tempCountValue = CraeteBinaryIR(CraeteBinaryIR(countValue, countMaskValue, IR::OPR::OPR_AND),
				CreateBVVIR(operandPtr[0].size, 32), IR::OPR::OPR_UREM);
			irList.push_back(tempCountValue);

			// Op1 << (INT_BITS - tempCOUNT)
			remains = CraeteBinaryIR(Op1,
				CraeteBinaryIR(CreateBVVIR(operandPtr[0].size, 32), tempCountValue, IR::OPR::OPR_SUB),
				IR::OPR::OPR_SHL);
			irList.push_back(remains);

			// (Op1 >> tempCOUNT)
			body = CraeteBinaryIR(Op1, tempCountValue, IR::OPR::OPR_SHR);
			irList.push_back(body);
		}

		else if (operandPtr[0].size == 16)
		{
			countMaskValue = CreateBVVIR(0x1f, 16);

			// tempCOUNT = (COUNT & COUNTMASK) MOD SIZE
			tempCountValue = CraeteBinaryIR(CraeteBinaryIR(countValue, countMaskValue, IR::OPR::OPR_AND),
				CreateBVVIR(operandPtr[0].size, 16), IR::OPR::OPR_UREM);
			irList.push_back(tempCountValue);

			// Op1 << (INT_BITS - tempCOUNT)
			remains = CraeteBinaryIR(Op1,
				CraeteBinaryIR(CreateBVVIR(operandPtr[0].size, 16), tempCountValue, IR::OPR::OPR_SUB),
				IR::OPR::OPR_SHL);
			irList.push_back(remains);

			// (Op1 >> tempCOUNT)
			body = CraeteBinaryIR(Op1, tempCountValue, IR::OPR::OPR_SHR);
			irList.push_back(body);
		}

		else if (operandPtr[0].size == 8)
		{
			countMaskValue = CreateBVVIR(0x1f, 8);

			// tempCOUNT = (COUNT & COUNTMASK) MOD SIZE
			tempCountValue = CraeteBinaryIR(CraeteBinaryIR(countValue, countMaskValue, IR::OPR::OPR_AND),
				CreateBVVIR(operandPtr[0].size, 8), IR::OPR::OPR_UREM);
			irList.push_back(tempCountValue);

			// Op1 << (INT_BITS - tempCOUNT)
			remains = CraeteBinaryIR(Op1,
				CraeteBinaryIR(CreateBVVIR(operandPtr[0].size, 8), tempCountValue, IR::OPR::OPR_SUB),
				IR::OPR::OPR_SHL);
			irList.push_back(remains);

			// (Op1 >> tempCOUNT)
			body = CraeteBinaryIR(Op1, tempCountValue, IR::OPR::OPR_SHR);
			irList.push_back(body);
		}

		else
		{
			break;
		}

		// (Op1 >> tempCOUNT) | (Op1 << (INT_BITS - tempCOUNT))
		rst = CraeteBinaryIR(body, remains, IR::OPR::OPR_OR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_AAM:
		break;
	case ZYDIS_MNEMONIC_AAD:
		break;
	case ZYDIS_MNEMONIC_SALC:
		break;
	case ZYDIS_MNEMONIC_XLAT:
		break;
	case ZYDIS_MNEMONIC_LOOP:
		break;
	case ZYDIS_MNEMONIC_LOOPE:
		break;
	case ZYDIS_MNEMONIC_LOOPNE:
		break;
	case ZYDIS_MNEMONIC_JCXZ:
		break;
	case ZYDIS_MNEMONIC_IN:
		break;
	case ZYDIS_MNEMONIC_OUT:
		break;
	case ZYDIS_MNEMONIC_CALL:
		break;
	case ZYDIS_MNEMONIC_JMP:
		//Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
		Op2 = CreateBVVIR(_regdump.regcontext.cip + operandPtr[0].imm.value.s +5 , 32);
		RegValue[REG_EIP].push_back(Op2);
		break;
	case ZYDIS_MNEMONIC_CMC:
		break;
	case ZYDIS_MNEMONIC_NOT:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);

		rst = CraeteUnaryIR(Op1, IR::OPR::OPR_NOT);
		irList.push_back(rst);

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_NEG:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);

		rst = CraeteUnaryIR(Op1, IR::OPR::OPR_NEG);
		irList.push_back(rst);

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_MUL:
		if (operandPtr[0].size == 32)
		{
			// Zero Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateZeroExtendIR(Op1, 64, 32, irList);

			// Zero Extend EAX
			Op2 = GetRegisterValue(ZYDIS_REGISTER_EAX, _regdump, irList);
			Op2Extend = CreateZeroExtendIR(Op2, 64, 32, irList);

			// ZEXT(Op1) * ZEXT(Op2)
			rst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
			irList.push_back(rst);

			// EAX = (31:0 )(ZEXT(Op1) * ZEXT(Op2))
			eaxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT32L);		
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_EAX, eaxRst, 32, irList);

			// EDX = (63:32 )(ZEXT(Op1) * ZEXT(Op2))
			edxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT32H);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_EDX, edxRst, 32, irList);
		}

		if (operandPtr[0].size == 16)
		{
			// Zero Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateZeroExtendIR(Op1, 32, 16, irList);

			// Zero Extend AX
			Op2 = GetRegisterValue(ZYDIS_REGISTER_AX, _regdump, irList);
			Op2Extend = CreateZeroExtendIR(Op2, 32, 16, irList);

			// ZEXT(Op1) * ZEXT(Op2)
			rst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
			irList.push_back(rst);

			// AX = (7:0)(ZEXT(Op1) * ZEXT(Op2))
			eaxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT16L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AX, eaxRst, 16, irList);

			// DX = (15:8)(ZEXT(Op1) * ZEXT(Op2))
			edxRst = CraeteUnaryIR(rst, IR::OPR::OPR_EXTRACT16H);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_DX, edxRst, 16, irList);
		}

		if (operandPtr[0].size == 8)
		{
			// Zero Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateZeroExtendIR(Op1, 16, 8, irList);

			// Get AL
			Op2 = GetRegisterValue(ZYDIS_REGISTER_AL, _regdump, irList);
			Op2Extend = CreateZeroExtendIR(Op2, 16, 8, irList);

			eaxRst = CraeteBinaryIR(Op1Extend, Op2Extend, IR::OPR::OPR_MUL);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AX, eaxRst, 16, irList);
		}
		break;

	case ZYDIS_MNEMONIC_DIV:
		if (operandPtr[0].size == 32)
		{
			// Get EAX
			eaxValue = GetRegisterValue(ZYDIS_REGISTER_EAX, _regdump, irList);

			// Get EDX
			edxValue = GetRegisterValue(ZYDIS_REGISTER_EDX, _regdump, irList);

			// EDX:EAX
			concatEdxEaxRst = CraeteBinaryIR(edxValue, eaxValue, IR::OPR::OPR_CONCAT);
			irList.push_back(concatEdxEaxRst);

			// Zero Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateZeroExtendIR(Op1, 64, 32, irList);

			// EAX = (31:0) (EDX:EAX / Op1)
			tempRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_UDIV);
			irList.push_back(tempRst);
			eaxRst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT32L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_EAX, eaxRst, 32, irList);

			//  EDX = (31:0) (EDX:EAX  mod Op1)
			remaRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_UREM);
			irList.push_back(remaRst);
			edxRst = CraeteUnaryIR(remaRst, IR::OPR::OPR_EXTRACT32L);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_EDX, edxRst, 32, irList);

		}

		if (operandPtr[0].size == 16)
		{
			// Get AX
			eaxValue = GetRegisterValue(ZYDIS_REGISTER_AX, _regdump, irList);

			// Get DX
			edxValue = GetRegisterValue(ZYDIS_REGISTER_DX, _regdump, irList);

			// DX:AX
			concatEdxEaxRst = CraeteBinaryIR(edxValue, eaxValue, IR::OPR::OPR_CONCAT);
			irList.push_back(concatEdxEaxRst);

			// Zero Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateZeroExtendIR(Op1, 32, 16, irList);

			// AX = (15:0) DX:AX / Op1
			tempRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_UDIV);
			irList.push_back(tempRst);
			eaxRst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT16L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AX, eaxRst, 16, irList);

			// DX = DX:AX  mod Op1
			remaRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_UREM);
			irList.push_back(remaRst);
			edxRst = CraeteUnaryIR(remaRst, IR::OPR::OPR_EXTRACT16L);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_DX, edxRst, 16, irList);
		}

		if (operandPtr[0].size == 8)
		{
			// Get AX
			eaxValue = GetRegisterValue(ZYDIS_REGISTER_AX, _regdump, irList);

			// Zero Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateZeroExtendIR(Op1, 16, 8, irList);

			// AX / Op1
			tempRst = CraeteBinaryIR(eaxValue, Op1Extend, IR::OPR::OPR_UDIV);
			irList.push_back(tempRst);
			eaxRst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT8L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AL, eaxRst, 8, irList);

			//  EDX:EAX  mod Op1
			remaRst = CraeteBinaryIR(eaxValue, Op1Extend, IR::OPR::OPR_UREM);
			irList.push_back(remaRst);
			edxRst = CraeteUnaryIR(remaRst, IR::OPR::OPR_EXTRACT8L);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AH, edxRst, 8, irList);
		}
		break;
	case ZYDIS_MNEMONIC_IDIV:
		if (operandPtr[0].size == 32)
		{
			// Get EAX
			eaxValue = GetRegisterValue(ZYDIS_REGISTER_EAX, _regdump, irList);

			// Get EDX
			edxValue = GetRegisterValue(ZYDIS_REGISTER_EDX, _regdump, irList);

			if (eaxValue->z3ExprPtr == nullptr)
			{
				MessageBoxA(0, "idiv eaxValue->z3Ptr null", "d", 0);
			}

			// EDX:EAX
			concatEdxEaxRst = CraeteBinaryIR(edxValue, eaxValue, IR::OPR::OPR_CONCAT);
			irList.push_back(concatEdxEaxRst);

			// Sign Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateSignExtendIR(Op1, 64, 32, irList);

			// EDX:EAX / Op1
			tempRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_SDIV);
			irList.push_back(tempRst);
			eaxRst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT32L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_EAX, eaxRst, 32, irList);

			//  EDX:EAX  mod Op1
			remaRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_UREM);
			irList.push_back(remaRst);
			edxRst = CraeteUnaryIR(remaRst, IR::OPR::OPR_EXTRACT32L);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_EDX, edxRst, 32, irList);

		}

		if (operandPtr[0].size == 16)
		{
			// Get EAX
			eaxValue = GetRegisterValue(ZYDIS_REGISTER_AX, _regdump, irList);

			// Get EDX
			edxValue = GetRegisterValue(ZYDIS_REGISTER_DX, _regdump, irList);

			// EDX:EAX
			concatEdxEaxRst = CraeteBinaryIR(edxValue, eaxValue, IR::OPR::OPR_CONCAT);
			irList.push_back(concatEdxEaxRst);

			// Sign Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateSignExtendIR(Op1, 32, 16, irList);

			// EDX:EAX / Op1
			tempRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_SDIV);
			irList.push_back(tempRst);
			eaxRst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT16L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AX, eaxRst, 16, irList);

			//  EDX:EAX  mod Op1
			remaRst = CraeteBinaryIR(concatEdxEaxRst, Op1Extend, IR::OPR::OPR_SREM);
			irList.push_back(remaRst);
			edxRst = CraeteUnaryIR(remaRst, IR::OPR::OPR_EXTRACT16L);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_DX, edxRst, 16, irList);
		}

		if (operandPtr[0].size == 8)
		{
			// Get AX
			eaxValue = GetRegisterValue(ZYDIS_REGISTER_AX, _regdump, irList);

			// Sign Extend Op1
			Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
			Op1Extend = CreateSignExtendIR(Op1, 16, 8, irList);

			// AX / Op1
			tempRst = CraeteBinaryIR(eaxValue, Op1Extend, IR::OPR::OPR_SDIV);
			irList.push_back(tempRst);
			eaxRst = CraeteUnaryIR(tempRst, IR::OPR::OPR_EXTRACT8L);
			irList.push_back(eaxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AL, eaxRst, 8, irList);

			//  EDX:EAX  mod Op1
			remaRst = CraeteBinaryIR(eaxValue, Op1Extend, IR::OPR::OPR_SREM);
			irList.push_back(remaRst);
			edxRst = CraeteUnaryIR(remaRst, IR::OPR::OPR_EXTRACT8L);
			irList.push_back(edxRst);
			SaveRegisterValue(ZYDIS_REGISTER_AH, edxRst, 8, irList);
		}
		IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_CLC:
		break;
	case ZYDIS_MNEMONIC_STC:
		break;
	case ZYDIS_MNEMONIC_CLI:
		break;
	case ZYDIS_MNEMONIC_STI:
		break;
	case ZYDIS_MNEMONIC_CLD:
		break;
	case ZYDIS_MNEMONIC_STD:
		break;

	case ZYDIS_MNEMONIC_LLDT:
		break;
	case ZYDIS_MNEMONIC_SLDT:
		break;
	case ZYDIS_MNEMONIC_VERR:
		break;
	case ZYDIS_MNEMONIC_VERW:
		break;
	case ZYDIS_MNEMONIC_LGDT:
		break;
	case ZYDIS_MNEMONIC_SGDT:
		break;
	case ZYDIS_MNEMONIC_LMSW:
		break;
	case ZYDIS_MNEMONIC_SMSW:
		break;
	case ZYDIS_MNEMONIC_LAR:
		break;
	case ZYDIS_MNEMONIC_LSL:
		break;
	case ZYDIS_MNEMONIC_CLTS:
		break;
	case ZYDIS_MNEMONIC_INVD:
		break;
	case ZYDIS_MNEMONIC_WBINVD:
		break;
	case ZYDIS_MNEMONIC_WRMSR:
		break;
	case ZYDIS_MNEMONIC_RDTSC:
		break;
	case ZYDIS_MNEMONIC_RDMSR:
		break;
	case ZYDIS_MNEMONIC_RDPMC:
		break;
	case ZYDIS_MNEMONIC_SYSENTER:
		break;
	case ZYDIS_MNEMONIC_SYSEXIT:
		break;
	case ZYDIS_MNEMONIC_CMOVO:
		break;
	case ZYDIS_MNEMONIC_CMOVNO:
		break;
	case ZYDIS_MNEMONIC_CMOVB:
		break;
	case ZYDIS_MNEMONIC_CMOVNB:
		break;
	case ZYDIS_MNEMONIC_CMOVZ:
		break;
	case ZYDIS_MNEMONIC_CMOVNZ:
		break;
	case ZYDIS_MNEMONIC_CMOVBE:
		break;
	case ZYDIS_MNEMONIC_CMOVNBE:
		break;
	case ZYDIS_MNEMONIC_CMOVS:
		break;
	case ZYDIS_MNEMONIC_CMOVNS:
		break;
	case ZYDIS_MNEMONIC_CMOVP:
		break;
	case ZYDIS_MNEMONIC_CMOVNP:
		break;
	case ZYDIS_MNEMONIC_CMOVL:
		break;
	case ZYDIS_MNEMONIC_CMOVNL:
		break;
	case ZYDIS_MNEMONIC_CMOVLE:
		break;
	case ZYDIS_MNEMONIC_CMOVNLE:
		break;
	case ZYDIS_MNEMONIC_SETO:
		break;
	case ZYDIS_MNEMONIC_CPUID:
		break;
	case ZYDIS_MNEMONIC_BSF:
		/*
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_BSF Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_BSF);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		*/
		break;
	case ZYDIS_MNEMONIC_BSR:
		break;
	case ZYDIS_MNEMONIC_BSWAP:
	
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList);
		if (operandPtr[0].size == 32)
		{
			auto offset1 = CreateExtractIR(31, 24, Op1);
			irList.push_back(offset1);

			auto offset2 = CreateExtractIR(23, 16, Op1);
			irList.push_back(offset2);

			auto offset3 = CreateExtractIR(15, 8, Op1);
			irList.push_back(offset3);

			auto offset4 = CreateExtractIR(7, 0, Op1);
			irList.push_back(offset4);

			rst = new IR(IR::OPR::OPR_CONCAT, offset4, offset3, offset2, offset1);
			irList.push_back(rst);
			SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		}
		
		else if (operandPtr[0].size == 16)
		{
			rst = CreateBVVIR(0, operandPtr[0].size);
			irList.push_back(rst);
			SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList);
		}
		

		else
		{
			rst = CraeteUnaryIR(Op1, IR::OPR::OPR_BSWAP);
			irList.push_back(rst);
		}


		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_BT:
	case ZYDIS_MNEMONIC_BTS:
	case ZYDIS_MNEMONIC_BTR:
	case ZYDIS_MNEMONIC_BTC:
		/*
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_BTC Error (Operand is not matched %p)\n", _regdump.regcontext.cip);
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_BTC);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		*/
		break;
	case ZYDIS_MNEMONIC_MOVSX:
		Op1 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
		rst = CreateSignExtendIR(Op1, operandPtr[0].size, operandPtr[1].size, irList);
		irList.push_back(rst);
		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_MOVZX:
		// t1 = OP2
		// t2 = OPR_ZEXT t1
		// OP1 = t2
		Op1 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);
		rst = CreateZeroExtendIR(Op1, operandPtr[0].size, operandPtr[1].size, irList);
		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_CMPXCHG:
		break;
	case ZYDIS_MNEMONIC_CMPXCHG8B:
		break;
	case ZYDIS_MNEMONIC_COMISD:
		break;
	case ZYDIS_MNEMONIC_COMISS:
		break;
	case ZYDIS_MNEMONIC_FCMOVB:
		break;
	case ZYDIS_MNEMONIC_FCOMI:
		break;
	case ZYDIS_MNEMONIC_FCOMIP:
		break;
	case ZYDIS_MNEMONIC_FUCOMI:
		break;
	case ZYDIS_MNEMONIC_FUCOMIP:
		break;
	case ZYDIS_MNEMONIC_HLT:
		break;
	case ZYDIS_MNEMONIC_INVLPG:
		break;
	case ZYDIS_MNEMONIC_UCOMISD:
		break;
	case ZYDIS_MNEMONIC_UCOMISS:
		break;
	case ZYDIS_MNEMONIC_IRET:
		break;
	case ZYDIS_MNEMONIC_LSS:
		break;
	case ZYDIS_MNEMONIC_LFS:
		break;
	case ZYDIS_MNEMONIC_LGS:
		break;
	case ZYDIS_MNEMONIC_LIDT:
		break;
	case ZYDIS_MNEMONIC_LTR:
		break;
	case ZYDIS_MNEMONIC_NOP:
		break;
	case ZYDIS_MNEMONIC_SHLD:
		break;
	case ZYDIS_MNEMONIC_SHRD:
		break;
	case ZYDIS_MNEMONIC_XADD:
		Op1 = GetOperand(ptr_di, &operandPtr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (ptr_di->opcode == 0x83)
			operandPtr[1].size = operandPtr[0].size;

		Op2 = GetOperand(ptr_di, &operandPtr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("GenerateOPR_ADD Error (Operand is not matched %p %d %d)\n", _regdump.regcontext.cip, Op1->Size, Op2->Size);
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_ADD);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가
		if (Op1->isTainted || Op2->isTainted)
		{

		}
		SetOperand(ptr_di, &operandPtr[1], Op1, _regdump, irList);
		SetOperand(ptr_di, &operandPtr[0], rst, _regdump, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		break;
	default:
		//_plugin_logprintf("Not Implemented Instruction :%s\n", ZydisMnemonicGetString(ptr_di->mnemonic));
		return 0;
		break;
	}
}

ULONG_PTR GetRegisterValueFromRegdump(ZydisRegister _zydisRegIdx, REGDUMP& regdump)
{

	switch (_zydisRegIdx)
	{
	case ZYDIS_REGISTER_EAX:
	case ZYDIS_REGISTER_AX:
	case ZYDIS_REGISTER_AH:
	case ZYDIS_REGISTER_AL:
		//m_RegisterInfo.GeneralPurposeRegister[0].Bit32 = _execptPtr->ContextRecord->Eax;
		return regdump.regcontext.cax;
		break;

	case ZYDIS_REGISTER_EBX:
	case ZYDIS_REGISTER_BX:
	case ZYDIS_REGISTER_BH:
	case ZYDIS_REGISTER_BL:
		//m_RegisterInfo.GeneralPurposeRegister[3].Bit32 = _execptPtr->ContextRecord->Ebx;
		return regdump.regcontext.cbx;
		break;

	case ZYDIS_REGISTER_ECX:
	case ZYDIS_REGISTER_CX:
	case ZYDIS_REGISTER_CH:
	case ZYDIS_REGISTER_CL:
		return regdump.regcontext.ccx;
		break;

	case ZYDIS_REGISTER_EDX:
	case ZYDIS_REGISTER_DX:
	case ZYDIS_REGISTER_DH:
	case ZYDIS_REGISTER_DL:
		return regdump.regcontext.cdx;
		break;

	case ZYDIS_REGISTER_ESP:
	case ZYDIS_REGISTER_SP:
		return regdump.regcontext.csp;
		break;

	case ZYDIS_REGISTER_EBP:
	case ZYDIS_REGISTER_BP:
		return regdump.regcontext.cbp;
		break;

	case ZYDIS_REGISTER_ESI:
	case ZYDIS_REGISTER_SI:
		return regdump.regcontext.csi;
		break;

	case ZYDIS_REGISTER_EDI:
	case ZYDIS_REGISTER_DI:
		return regdump.regcontext.cdi;
		break;

	default:
		MessageBoxA(0, "ZydisRegIndexToDbiRegIndex32 Register Operand Error", "Error", 0);
		return 0;
		break;
	}

}

DWORD GetEffectiveAddress32(ZydisDecodedOperand* _decodedOperandPtr, REGDUMP& regdump)
{
	ZydisOperandType OP_ADDR = _decodedOperandPtr->type;
	if (OP_ADDR == ZYDIS_OPERAND_TYPE_MEMORY)
	{
		DWORD base = _decodedOperandPtr->mem.base == ZYDIS_REGISTER_NONE ? 0 : GetRegisterValueFromRegdump(_decodedOperandPtr->mem.base, regdump);
		DWORD si = _decodedOperandPtr->mem.index == ZYDIS_REGISTER_NONE ? 0 : GetRegisterValueFromRegdump(_decodedOperandPtr->mem.index, regdump);
		int disp = _decodedOperandPtr->mem.disp.has_displacement == TRUE ? _decodedOperandPtr->mem.disp.value : 0;
#ifdef DEBUG
		_plugin_logprintf("GetEffectiveAddress32 %p\n", base + si + disp);
#endif
		return base + si + disp;
		// 실제로 Effective Address 주소를 구하기 위해서 계산할 Displacement를 결정
	}

	else
	{
		printf("OP_ADDR:%d\n", OP_ADDR);
		MessageBoxA(0, "[GetEffectiveAddress32] Operand is not memory operand!", "Error", 0);
		return 0;
	}
}

void printIR(IR* _irPtr)
{
	if (dynamic_cast<ConstInt*>(_irPtr))
	{
		if (dynamic_cast<ConstInt*>(_irPtr)->opr == IR::OPR::OPR_BVV)
		{

			_plugin_logprintf("%x\n", dynamic_cast<ConstInt*>(_irPtr)->intVar);
		}
	}
	switch (_irPtr->opr)
	{
	case IR::OPR::OPR_CONCAT:
		if (_irPtr->Operands.size() == 2)
		{
			_plugin_logprintf("%s = CONCAT %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		}

		if (_irPtr->Operands.size() == 3)
		{
			_plugin_logprintf("%s = CONCAT %s %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str(), _irPtr->Operands[2]->valuePtr->Name.c_str());
		}

		else if (_irPtr->Operands.size() == 4)
		{
			_plugin_logprintf("%s = CONCAT %s %s %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str(), _irPtr->Operands[2]->valuePtr->Name.c_str(), _irPtr->Operands[3]->valuePtr->Name.c_str());
		}
		break;
	case IR::OPR::OPR_BVV:
		_plugin_logprintf("%s =  BVV(%x, %d)\n", _irPtr->Name.c_str(), dynamic_cast<ConstInt*>(_irPtr->Operands[0]->valuePtr)->intVar, dynamic_cast<ConstInt*>(_irPtr->Operands[1]->valuePtr)->intVar);
		break;
	case IR::OPR::OPR_EXTRACT16H:
		_plugin_logprintf("%s = OPR_EXTRACT16H %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_EXTRACT8HH:
		_plugin_logprintf("%s = OPR_EXTRACT8HH %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_EXTRACT8HL:
		_plugin_logprintf("%s = OPR_EXTRACT8HL %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_EXTRACT8H:
		_plugin_logprintf("%s = OPR_EXTRACT8H %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_EXTRACT8L:
		_plugin_logprintf("%s = OPR_EXTRACT8L %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_ZEXT:
		_plugin_logprintf("%s = OPR_ZEXT %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SEXT:
		_plugin_logprintf("%s = OPR_SEXT %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_ADD:
		_plugin_logprintf("%s = OPR_ADD %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_ADC:
		_plugin_logprintf("%s = OPR_ADC %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SUB:
		_plugin_logprintf("%s = OPR_SUB %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_MUL:
		_plugin_logprintf("%s = OPR_MUL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_AND:
		_plugin_logprintf("%s = OPR_AND %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_OR:
		_plugin_logprintf("%s = OPR_OR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_XOR:
		_plugin_logprintf("%s = OPR_XOR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_NOT:
		_plugin_logprintf("%s = OPR_NOT %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_NEG:
		_plugin_logprintf("%s = OPR_NEG %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_RCL:
		_plugin_logprintf("%s = OPR_RCL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_RCR:
		_plugin_logprintf("%s = OPR_RCR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SHL:
		_plugin_logprintf("%s = OPR_SHL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SHR:
		_plugin_logprintf("%s = OPR_SHR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SAR:
		_plugin_logprintf("%s = OPR_SAR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_BTC:
		_plugin_logprintf("%s = OPR_BTC %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_BSF:
		_plugin_logprintf("%s = OPR_BSF %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_BSWAP:
		_plugin_logprintf("%s = OPR_BSWAP %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_LOAD:
		_plugin_logprintf("%s = LOAD %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_STORE:
		_plugin_logprintf("STORE %s %s\n", _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	default:
		_plugin_logprintf("Not Implemented print :%d\n", _irPtr->opr);
		break;
	}
}

string IR::printOpr(OPR _opr)
{
	switch (_opr)
	{
	case IR::OPR::OPR_CONCAT:
		return string("Concat");
		break;
	case IR::OPR::OPR_BVV:
		return string("OPR_BVV");
		break;
	case IR::OPR::OPR_GET32INT:
		return string("OPR_GET32INT");
		break;
	case IR::OPR::OPR_GET8INT:
		return string("OPR_GET8INT");
		break;
	case IR::OPR::OPR_EXTRACT16H:
		return string("EXTRACT16H");
		break;
	case IR::OPR::OPR_EXTRACT8HH:
		return string("EXTRACT8HH");
		break;
	case IR::OPR::OPR_EXTRACT8HL:
		return string("EXTRACT8HL");
		break;
	case IR::OPR::OPR_EXTRACT8H:
		return string("EXTRACT8H");
		break;
	case IR::OPR::OPR_EXTRACT8L:
		return string("EXTRACT8L");
		break;
	case IR::OPR::OPR_ZEXT:
		return string("ZEXT");
		break;
	case IR::OPR::OPR_SEXT:
		return string("SEXT");
		break;
	case IR::OPR::OPR_ADD:
		return string("OPR_ADD");
		break;
	case IR::OPR::OPR_SUB:
		return string("OPR_SUB");
		break;
	case IR::OPR::OPR_UREM:
		return string("OPR_UREM");
		break;
	case IR::OPR::OPR_OR:
		return string("OPR_OR");
		break;
	case IR::OPR::OPR_AND:
		return string("OPR_AND");
		break;
	case IR::OPR::OPR_XOR:
		return string("OPR_XOR");
		break;
	case IR::OPR::OPR_SAR:
		return string("OPR_SAR");
		break;
	case IR::OPR::OPR_LOAD:
		return string("OPR_LOAD");
		break;
	case IR::OPR::OPR_STORE:
		return string("OPR_STORE");
		break;
	default:
		return string("OPR_STORE");
		break;
	}
}

string EvaluateExpTree(std::shared_ptr<BTreeNode> bt)
{
	string op1, op2, op3, op4;
	string exprString;

	if (bt->left == NULL)
	{
		_plugin_logprintf("bt->left == NULL bt->nodeName %s\n", bt->nodeName.c_str());
		if (bt->m_NodeType == NT_SYMVAR)
		{
			if (symbolExprMap.find(bt->nodeName) != symbolExprMap.end())
			{
				//z3Equation = symbolExprMap[bt->nodeName];
			}
		}
		return bt->nodeName;
	}

	op1 = EvaluateExpTree(bt->left);
	if (bt->right != NULL)
	{
		op2 = EvaluateExpTree(bt->right);
	}

	if (bt->nodeName.compare("OPR_ADD") == 0)
	{
		//*z3Equation = 
		return "bvadd " + op1 + " " + op2;
	}

	else if (bt->nodeName.compare("Concat") == 0)
	{
		if (bt->third != NULL)
			op3 = EvaluateExpTree(bt->third);
		if (bt->fourth != NULL)
			op4 = EvaluateExpTree(bt->fourth);
		if (bt->fourth != nullptr)
		{
			//z3::concat_expr
			return "(Concat " + op1 + " " + op2 + " " + op3 + " " + op4 + ")";
		}

		else if (bt->third != nullptr && bt->fourth == nullptr)
		{
			return "";//"(Concat " + op1 + " " + op2 + " " + op3 + ")";
		}

		else
		{
			return "";//"(Concat " + op1 + " " + op2 + ")";
		}
	}

	else if (bt->nodeName.compare("EXTRACT8HH") == 0)
	{
		return "";//"(Extract 31 24  " + op1 + ")";
	}

	else if (bt->nodeName.compare("EXTRACT8HL") == 0)
	{
		return "";//"(Extract 23 16  " + op1 + ")";
	}

	else if (bt->nodeName.compare("EXTRACT16H") == 0)
	{
		return "";//"(Extract 31 16  " + op1 + ")";
	}

	else if (bt->nodeName.compare("EXTRACT8H") == 0)
	{
		return "";//"(Extract 15 8  " + op1 + ")";
	}

	else if (bt->nodeName.compare("EXTRACT8L") == 0)
	{
		return "";//"(Extract 7 0  " + op1 + ")";
	}

	else
	{
		//_plugin_logprintf("bt->nodeName %s\n", bt->nodeName.c_str());
		if (bt->nodeName.empty() == false)
		{
			//if (bt->m_NodeType == NT_SYMVAR)
			//{
			//	if (symbolExprMap.find(bt->nodeName) != symbolExprMap.end())
			//	{
			//		symbolExprMap[bt->m_NodeType]
			//	}
			//}

			//else
			//{
			//	
			//}
			//printf("[test] %s\n", bt->nodeName.c_str());
			exprString = bt->nodeName;
			return exprString;
		}

		else
		{
			exprString = "di rlagkstn2";
		}
	}

	return "";
}

bool isConstantFolding(Value* _op1, Value* _op2)
{
	IR* op1IR = nullptr;
	IR* op2IR = nullptr;

	op1IR = dynamic_cast<IR*>(_op1);
	op2IR = dynamic_cast<IR*>(_op2);

	if (op1IR && op2IR)
	{
		if (op1IR->opr == IR::OPR::OPR_BVV && op1IR->opr == IR::OPR::OPR_BVV)
			return true;
	}
	return false;
}

z3::expr GetZ3ExprFromTree(std::shared_ptr<BTreeNode> bt)
{
	if (bt->left == NULL)
	{
		if (bt->m_NodeType == NT_SYMVAR)
		{
			if (symbolExprMap.find(bt->nodeName) != symbolExprMap.end())
			{
				return *symbolExprMap[bt->nodeName];
			}
		}

		if (bt->valPtr == NULL)
		{
			_plugin_logprintf("valptr is NULL\n");
		}

		else
		{
			if (dynamic_cast<ConstInt*>(bt->valPtr))
			{
				//_plugin_logprintf("Test : %x\n", (unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar);
				if (dynamic_cast<ConstInt*>(bt->valPtr)->Size == 32)
				{

					z3::expr z3Expression = z3Context->bv_val((unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar, 32);
					return z3Context->bv_val((unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar, 32);
				}

				else if (dynamic_cast<ConstInt*>(bt->valPtr)->Size == 16)
				{
					z3::expr z3Expression = z3Context->bv_val((unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar, 16);
					return z3Context->bv_val((unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar, 16);
				}

				else if (dynamic_cast<ConstInt*>(bt->valPtr)->Size == 8)
				{
					z3::expr z3Expression = z3Context->bv_val((unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar, 8);
					return z3Context->bv_val((unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar, 8);
				}

				else
				{

				}
			}
		}
	}

	else
	{
		z3::expr op1 = GetZ3ExprFromTree(bt->left);
		if (bt->right != NULL)
		{
			z3::expr op2 = GetZ3ExprFromTree(bt->right);
		}

		if (bt->nodeName.compare("OPR_BVV") == 0)
		{
			return op1;

		}

		else if (bt->nodeName.compare("OPR_ADD") == 0) {

			z3::expr op2 = GetZ3ExprFromTree(bt->right);
			return op1 + op2;
		}

		else if (bt->nodeName.compare("OPR_SUB") == 0) {

			z3::expr op2 = GetZ3ExprFromTree(bt->right);
			return op1 - op2;
		}

		else if (bt->nodeName.compare("OPR_AND") == 0) {

			z3::expr op2 = GetZ3ExprFromTree(bt->right);
			return op1 & op2;
		}

		else if (bt->nodeName.compare("OPR_XOR") == 0) {

			z3::expr op2 = GetZ3ExprFromTree(bt->right);
			return op1 ^ op2;
		}

		else if (bt->nodeName.compare("Concat") == 0)
		{
			if (bt->third != NULL)
			{
				z3::expr op3 = GetZ3ExprFromTree(bt->third);
			}
			if (bt->fourth != NULL)
				z3::expr op4 = GetZ3ExprFromTree(bt->fourth);
			if (bt->fourth != nullptr)
			{
				z3::expr_vector exprVec(*z3Context);
				z3::expr op2 = GetZ3ExprFromTree(bt->right);
				z3::expr op3 = GetZ3ExprFromTree(bt->third);
				z3::expr op4 = GetZ3ExprFromTree(bt->fourth);

				exprVec.push_back(op1);
				exprVec.push_back(op2);
				exprVec.push_back(op3);
				exprVec.push_back(op4);


				return z3::concat(exprVec);
			}

			else if (bt->third != nullptr && bt->fourth == nullptr)
			{
				z3::expr_vector exprVec(*z3Context);
				z3::expr op2 = GetZ3ExprFromTree(bt->right);
				z3::expr op3 = GetZ3ExprFromTree(bt->third);

				exprVec.push_back(op1);
				exprVec.push_back(op2);
				exprVec.push_back(op3);


				return z3::concat(exprVec);
			}

			else
			{
				printf("Concat 2 Operands\n");
				z3::expr op2 = GetZ3ExprFromTree(bt->right);
				return z3::concat(op1, op2);
			}
		}

		else if (bt->nodeName.compare("EXTRACT16H") == 0)
		{
			return op1.extract(31, 16);
		}

		else if (bt->nodeName.compare("EXTRACT8H") == 0)
		{
			return op1.extract(15, 8);
		}

		else if (bt->nodeName.compare("EXTRACT8L") == 0)
		{
			return op1.extract(7, 0);
		}

		else
		{
			if (bt->nodeName.empty() == false)
			{
				//if (bt->m_NodeType == NT_SYMVAR)
				//{
				//	if (symbolExprMap.find(bt->nodeName) != symbolExprMap.end())
				//	{
				//		symbolExprMap[bt->m_NodeType]
				//	}
				//}

				//else
				//{
				//	
				//}
				//printf("[test] %s\n", bt->nodeName.c_str());

			}
		}
	}
}

void IR::CreateZ3Expr(IR* _irPtr)
{
	switch (_irPtr->opr)
	{
	case IR::OPR::OPR_CONCAT:
		if (_irPtr->Operands.size() == 2)
		{
			z3::expr_vector fuck(*z3Context);
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					fuck.push_back(*_irPtr->Operands[0]->valuePtr->z3ExprPtr);
					fuck.push_back(*_irPtr->Operands[1]->valuePtr->z3ExprPtr);
					_irPtr->z3ExprPtr = new z3::expr(z3::concat(fuck));
					//*_irPtr->z3ExprPtr = _irPtr->z3ExprPtr->simplify();
				}
			}
		}

		if (_irPtr->Operands.size() == 3)
		{
			z3::expr_vector fuck(*z3Context);
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr) && (_irPtr->Operands[2]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr) &&
					(_irPtr->Operands[2]->valuePtr->z3ExprPtr != nullptr))
				{
					fuck.push_back(*_irPtr->Operands[0]->valuePtr->z3ExprPtr);
					fuck.push_back(*_irPtr->Operands[1]->valuePtr->z3ExprPtr);
					fuck.push_back(*_irPtr->Operands[2]->valuePtr->z3ExprPtr);
					_irPtr->z3ExprPtr = new z3::expr(z3::concat(fuck));
					//*_irPtr->z3ExprPtr = _irPtr->z3ExprPtr->simplify();
				}
			}
		}

		else if (_irPtr->Operands.size() == 4)
		{
			z3::expr_vector fuck(*z3Context);
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr) && (_irPtr->Operands[2]->valuePtr != nullptr) && (_irPtr->Operands[3]->valuePtr != nullptr))
			{
				if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr &&
					_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr &&
					_irPtr->Operands[2]->valuePtr->z3ExprPtr != nullptr &&
					_irPtr->Operands[3]->valuePtr->z3ExprPtr != nullptr)
				{
					fuck.push_back(*_irPtr->Operands[0]->valuePtr->z3ExprPtr);
					fuck.push_back(*_irPtr->Operands[1]->valuePtr->z3ExprPtr);
					fuck.push_back(*_irPtr->Operands[2]->valuePtr->z3ExprPtr);
					fuck.push_back(*_irPtr->Operands[3]->valuePtr->z3ExprPtr);
					_irPtr->z3ExprPtr = new z3::expr(z3::concat(fuck));
					//*_irPtr->z3ExprPtr = _irPtr->z3ExprPtr->simplify();
				}
			}
		}
		break;

	case IR::OPR::OPR_BVV:
		if (_irPtr->Operands.size() == 2)
		{
			if (dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)) &&
				dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[1]->valuePtr)))
			{
				DWORD intVal = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr))->intVar;
				DWORD size = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[1]->valuePtr))->intVar;
				_irPtr->z3ExprPtr = new z3::expr(z3Context->bv_val((unsigned int)intVal, size));
			}

		}
		break;
	case IR::OPR::OPR_EXTRACT:
		if (_irPtr->Operands.size() == 3)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr) && (_irPtr->Operands[2]->valuePtr != nullptr))
			{
				if (dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->Operands[0]->valuePtr) &&
					dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[1]->valuePtr)->Operands[0]->valuePtr))
				{					
					if (_irPtr->Operands[2]->valuePtr->z3ExprPtr != nullptr)
					{
						DWORD op1 = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->Operands[0]->valuePtr)->intVar;
						DWORD op2 = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[1]->valuePtr)->Operands[0]->valuePtr)->intVar;
						_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[2]->valuePtr->z3ExprPtr->extract(op1, op2));
					}
				}
			}
		}
		break;
	case IR::OPR::OPR_EXTRACT32H:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(63, 32));
		}
		break;
	case IR::OPR::OPR_EXTRACT32L:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(31, 0));
		}
		break;
	case IR::OPR::OPR_EXTRACT16H:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(31, 16));
		}
		break;
	case IR::OPR::OPR_EXTRACT8HH:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(31, 24));
		}
		break;
	case IR::OPR::OPR_EXTRACT8HL:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(23, 16));
		}
		break;
	case IR::OPR::OPR_EXTRACT8H:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(15, 8));
		}
		break;
	case IR::OPR::OPR_EXTRACT8L:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(7, 0));
		}
		break;
	case IR::OPR::OPR_EXTRACTSF64:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(63, 63));
		}
		break;
	case IR::OPR::OPR_EXTRACTSF32:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(31, 31));
		}
		break;
	case IR::OPR::OPR_EXTRACTSF16:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(15, 15));
		}
		break;
	case IR::OPR::OPR_EXTRACTSF8:
		if (_irPtr->Operands[0]->valuePtr != nullptr)
		{
			if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
				_irPtr->z3ExprPtr = new z3::expr(_irPtr->Operands[0]->valuePtr->z3ExprPtr->extract(7, 7));
		}
		break;
	case IR::OPR::OPR_ZEXT:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)))
				{
					if (dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->Operands[0]->valuePtr))
					{
						if (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr)
						{
							_irPtr->z3ExprPtr = new z3::expr(z3::zext(*_irPtr->Operands[1]->valuePtr->z3ExprPtr,
								dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->Operands[0]->valuePtr)->intVar));
						}

					}
				}
			}
		}
		break;
	case IR::OPR::OPR_SEXT:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)))
				{
					if (dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->Operands[0]->valuePtr))
					{
						if (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr)
						{
							_irPtr->z3ExprPtr = new z3::expr(z3::sext(*_irPtr->Operands[1]->valuePtr->z3ExprPtr,
								dynamic_cast<ConstInt*>(dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->Operands[0]->valuePtr)->intVar));
						}

					}
				}
			}
		}
		break;
		break;
	case IR::OPR::OPR_ADD:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr + *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		break;
	case IR::OPR::OPR_ADC:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr + *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		break;
	case IR::OPR::OPR_SUB:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != NULL) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr - *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		break;
	case IR::OPR::OPR_MUL:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr * *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		break;
	case IR::OPR::OPR_SDIV:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr / *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
				}
			}
		}
		break;

	case IR::OPR::OPR_SREM:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					//_irPtr->z3ExprPtr = new z3::expr(z3::srem(*_irPtr->Operands[0]->valuePtr->z3ExprPtr, *_irPtr->Operands[1]->valuePtr->z3ExprPtr));
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr % *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
				}
			}
		}
		break;
	case IR::OPR::OPR_UREM:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(z3::srem(*_irPtr->Operands[0]->valuePtr->z3ExprPtr, *_irPtr->Operands[1]->valuePtr->z3ExprPtr));
				}
			}
		}
		break;
	case IR::OPR::OPR_AND:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr & *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		break;
	case IR::OPR::OPR_OR:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr | *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		break;
	case IR::OPR::OPR_XOR:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
					_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr ^ *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
			}
		}
		else
		{
			_plugin_logprintf("[%p] ir\n", cntd);
			printIR(_irPtr);
			MessageBoxA(0, "dsg", "34ty", 0);
		}
		break;
	case IR::OPR::OPR_NOT:
		if (_irPtr->Operands.size() == 1)
		{
			if (_irPtr->Operands[0]->valuePtr != nullptr)
			{
				if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
					_irPtr->z3ExprPtr = new z3::expr(~(*_irPtr->Operands[0]->valuePtr->z3ExprPtr));
			}
		}
		//_plugin_logprintf("%s = OPR_NOT %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_NEG:
		if (_irPtr->Operands.size() == 1)
		{
			if (_irPtr->Operands[0]->valuePtr != nullptr)
			{
				if (_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr)
					_irPtr->z3ExprPtr = new z3::expr(-(*_irPtr->Operands[0]->valuePtr->z3ExprPtr));
			}
		}
		//_plugin_logprintf("%s = OPR_NOT %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_RCL:
		//_plugin_logprintf("%s = OPR_RCL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_RCR:
		//_plugin_logprintf("%s = OPR_RCR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SHL:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(z3::shl(*_irPtr->Operands[0]->valuePtr->z3ExprPtr, *_irPtr->Operands[1]->valuePtr->z3ExprPtr));
				}
			}
		}
		break;
	case IR::OPR::OPR_SHR:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(z3::lshr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr, *_irPtr->Operands[1]->valuePtr->z3ExprPtr));
				}
			}
		}
		break;
	case IR::OPR::OPR_SAR:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(z3::ashr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr, *_irPtr->Operands[1]->valuePtr->z3ExprPtr));
				}
			}
		}
		break;
	case IR::OPR::OPR_BTC:
		//_plugin_logprintf("%s = OPR_BTC %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_BSF:
		//_plugin_logprintf("%s = OPR_BSF %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_BSWAP:
		//_plugin_logprintf("%s = OPR_BSWAP %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_LOAD:
		//_plugin_logprintf("%s = LOAD %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_STORE:
		//_plugin_logprintf("STORE %s %s\n", _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case OPR_ITE:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					//_irPtr->z3ExprPtr = new z3::expr(z3::bv2int(*_irPtr->Operands[0]->valuePtr->z3ExprPtr, 0) == z3::bv2int(*_irPtr->Operands[1]->valuePtr->z3ExprPtr, 0));
					//_irPtr->z3ExprPtr = new z3::expr(*_irPtr->Operands[0]->valuePtr->z3ExprPtr > *_irPtr->Operands[1]->valuePtr->z3ExprPtr);
					//_irPtr->z3ExprPtr = new z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr) == (*_irPtr->Operands[1]->valuePtr->z3ExprPtr));

					if (_irPtr->Size == 64)
					{
						_irPtr->z3ExprPtr = new z3::expr(z3::ite(z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr) == (*_irPtr->Operands[1]->valuePtr->z3ExprPtr)),
							z3::expr(z3Context->bv_val(1, 64)), z3Context->bv_val(0, 64)));
					}

					else if (_irPtr->Size == 32)
					{
						_irPtr->z3ExprPtr = new z3::expr(z3::ite(z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr) == (*_irPtr->Operands[1]->valuePtr->z3ExprPtr)),
							z3::expr(z3Context->bv_val(1, 32)), z3Context->bv_val(0, 32)));
					}

					else if (_irPtr->Size == 16)
					{
						_irPtr->z3ExprPtr = new z3::expr(z3::ite(z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr) == (*_irPtr->Operands[1]->valuePtr->z3ExprPtr)),
							z3::expr(z3Context->bv_val(1, 16)), z3Context->bv_val(0, 16)));
					}

					else if (_irPtr->Size == 8)
					{
						_irPtr->z3ExprPtr = new z3::expr(z3::ite(z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr) == (*_irPtr->Operands[1]->valuePtr->z3ExprPtr)),
							z3::expr(z3Context->bv_val(1, 8)), z3Context->bv_val(0, 8)));
					}

					else
					{
						MessageBoxA(0, "Test", "Test", 0);
					}
				}
			}
		}
		break;

	case OPR_ISEQUAL:
		if (_irPtr->Operands.size() == 2)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr) == (*_irPtr->Operands[1]->valuePtr->z3ExprPtr)));
				}
			}
		}
		break;

	case OPR_BRC:
		if (_irPtr->Operands.size() == 3)
		{
			if ((_irPtr->Operands[0]->valuePtr != nullptr) && (_irPtr->Operands[1]->valuePtr != nullptr) && (_irPtr->Operands[2]->valuePtr != nullptr))
			{
				if ((_irPtr->Operands[0]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[1]->valuePtr->z3ExprPtr != nullptr) && (_irPtr->Operands[2]->valuePtr->z3ExprPtr != nullptr))
				{
					_irPtr->z3ExprPtr = new z3::expr(z3::ite(z3::expr((*_irPtr->Operands[0]->valuePtr->z3ExprPtr)),
						*_irPtr->Operands[1]->valuePtr->z3ExprPtr,
						*_irPtr->Operands[2]->valuePtr->z3ExprPtr));
				}
			}
		}
		break;
	default:
		//_plugin_logprintf("Not Implemented print :%d\n", _irPtr->opr);
		break;
	}
}