#include "Value.h"
#include <map>

extern vector<Value*> StatusFlagsValue[5];
extern vector<Value*> RegValue[ZYDIS_REGISTER_MAX_VALUE];
extern map<DWORD, vector<Value*>> MemValue;
extern map<DWORD, vector<IR*>> IRList;

extern z3::context* z3Context;
extern z3::expr* z3Equation;
extern map<string, z3::expr*> symbolExprMap;

DWORD tmpNum = 0;

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

IR* CraeteBVVIR(DWORD intVal, BYTE size)
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
	testAddIR->ast->m_NodeType = NT_OPERATOR;
	//testAddIR->isHiddenRHS = true;
	return testAddIR;
}

IR* CraeteLoadIR(Value* op1, IR::OPR _opr)
{
	IR* testAddIR = new IR(IR::OPR_LOAD, op1);
	testAddIR->isHiddenRHS = true;
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
			RegValue[REG_EAX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EAX8H", RegValue[REG_EAX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EAX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EAX8L", RegValue[REG_EAX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EAX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EBX:
			reg16hIR = new IR("EBX16H", RegValue[REG_EBX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_EBX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EBX8H", RegValue[REG_EBX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EBX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EBX8L", RegValue[REG_EBX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EBX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_ECX:
			reg16hIR = new IR("ECX16H", RegValue[REG_ECX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_ECX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("ECX8H", RegValue[REG_ECX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_ECX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("ECX8L", RegValue[REG_ECX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_ECX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EDX:
			reg16hIR = new IR("EDX16H", RegValue[REG_EDX_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_EDX_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EDX8H", RegValue[REG_EDX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EDX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EDX8L", RegValue[REG_EDX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EDX_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EBP:
			reg16hIR = new IR("EBP16H", RegValue[REG_EBP_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_EBP_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EBP8H", RegValue[REG_EBP_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EBP_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EBP8L", RegValue[REG_EBP_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EBP_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_ESI:
			reg16hIR = new IR("ESI16H", RegValue[REG_ESI_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_ESI_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("ESI8H", RegValue[REG_ESI_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_ESI_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("ESI8LH", RegValue[REG_ESI_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_ESI_8L].push_back(reg8lIR);
			irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_EDI:
			reg16hIR = new IR("EDI16H", RegValue[REG_EDI_16H].size(), IR::OPR::OPR_EXTRACT16H, _regValue);
			RegValue[REG_EDI_16H].push_back(reg16hIR);
			irList.push_back(reg16hIR);

			reg8hIR = new IR("EDI8H", RegValue[REG_EDI_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EDI_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR("EDI8L", RegValue[REG_EDI_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
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
				RegValue[REG_EAX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				RegValue[REG_EAX_8L].push_back(reg8lIR);
				break;
			}

			else
			{
				reg8hIR = new IR("EAX8H", RegValue[REG_EAX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				RegValue[REG_EAX_8H].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
				RegValue[REG_EAX_8L].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				break;
			}

		case ZYDIS_REGISTER_BX:
			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				RegValue[REG_EBX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				RegValue[REG_EBX_8L].push_back(reg8lIR);
				break;
			}
			else
			{
				reg8hIR = new IR("EBX8H", RegValue[REG_EBX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				RegValue[REG_EBX_8H].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				reg8lIR = new IR("EBX8L", RegValue[REG_EBX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
				RegValue[REG_EBX_8L].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				break;
			}

		case ZYDIS_REGISTER_CX:
			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				RegValue[REG_ECX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				RegValue[REG_ECX_8L].push_back(reg8lIR);
				break;
			}

			else
			{
				reg8hIR = new IR("ECX8H", RegValue[REG_ECX_8H].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				RegValue[REG_ECX_8H].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				reg8lIR = new IR("ECX8L", RegValue[REG_ECX_8L].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
				RegValue[REG_ECX_8L].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				break;
			}

		case ZYDIS_REGISTER_DX:

			if (dynamic_cast<ConstInt*>(_regValue))
			{
				op2ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff00) >> 8;
				Value* reg8hIR = new ConstInt(IR::OPR_GET8INT, op2ConstValue);
				RegValue[REG_EDX_8H].push_back(reg8hIR);

				op3ConstValue = (dynamic_cast<ConstInt*>(_regValue)->intVar & 0xff);
				Value* reg8lIR = new ConstInt(IR::OPR_GET8INT, op3ConstValue);
				RegValue[REG_EDX_8L].push_back(reg8lIR);
				break;
			}

			reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EDX_8H].push_back(reg8hIR);
			irList.push_back(reg8hIR);

			reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
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
			//reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EAX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_BH:
			//reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EBX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_CH:
			//reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_ECX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_DH:
			//reg8hIR = new IR(IR::OPR::OPR_EXTRACT8H, _regValue);
			RegValue[REG_EDX_8H].push_back(_regValue);
			//irList.push_back(reg8hIR);
			break;

		case ZYDIS_REGISTER_AL:
			//reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EAX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_BL:
			//reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EBX_8H].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_CL:
			//reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_ECX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;

		case ZYDIS_REGISTER_DL:
			//reg8lIR = new IR(IR::OPR::OPR_EXTRACT8L, _regValue);
			RegValue[REG_EDX_8L].push_back(_regValue);
			//irList.push_back(reg8lIR);
			break;
		}
	}
}
#define DEBUG
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
			rstIR = CraeteBVVIR((_regdump.regcontext.cax)>>16, 16);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.cax) & 0xffff) >> 8, 8);
			RegValue[REG_EAX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = RegValue[REG_EAX_8H].back();		

		if (RegValue[REG_EAX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cax) & 0xff, 8);
			RegValue[REG_EAX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.cax) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = RegValue[REG_EAX_8L].back();

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		// Todo op1, op2, op3이 OPC_EXTRACT
		// OptimizeConcat(IR* concatIRPtr)

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_REGISTER;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EBX:
		
		if (RegValue[REG_EBX_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbx) >> 16, 16);
			RegValue[REG_EBX_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EBX_16H].back());		

		if (RegValue[REG_EBX_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
			RegValue[REG_EBX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBX_8H].back());
		
		if (RegValue[REG_EBX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_EBX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8L :%x\n", (_regdump.regcontext.cbx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EBX_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_REGISTER;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_ECX:		
		if (RegValue[REG_ECX_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.ccx) >> 16, 16);
			RegValue[REG_ECX_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_ECX_16H].back());
		
		if (RegValue[REG_ECX_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.ccx) & 0xffff) >> 8, 8);
			RegValue[REG_ECX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8H :%x\n", ((_regdump.regcontext.ccx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ECX_8H].back());

		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.ccx) & 0xff, 8);
			RegValue[REG_ECX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8L :%x\n", (_regdump.regcontext.ccx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ECX_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_REGISTER;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EDX:
		
		if (RegValue[REG_EDX_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cdx) >> 16, 16);
			RegValue[REG_EDX_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EDX_16H].back());
		
		if (RegValue[REG_EDX_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cdx) & 0xffff) >> 8, 8);
			RegValue[REG_EDX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8H :%x\n", ((_regdump.regcontext.cdx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDX_8H].back());
		
		if (RegValue[REG_EDX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cdx) & 0xff, 8);
			RegValue[REG_EDX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8L :%x\n", (_regdump.regcontext.cdx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EDX_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EBP:
		
		if (RegValue[REG_EBP_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbp) >> 16, 16);
			RegValue[REG_EBP_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EBP_16H].back());
		
		if (RegValue[REG_EBP_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cbp) & 0xffff) >> 8, 8);
			RegValue[REG_EBP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBP 8H :%x\n", ((_regdump.regcontext.cbp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBP_8H].back());
		
		if (RegValue[REG_EBP_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbp) & 0xff, 8);
			RegValue[REG_EBP_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBP 8L :%x\n", (_regdump.regcontext.cbp) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EBP_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_ESP:
		
		if (RegValue[REG_ESP_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.csp) >> 16, 16);
			RegValue[REG_ESP_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_ESP_16H].back());
		
		if (RegValue[REG_ESP_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.csp) & 0xffff) >> 8, 8);
			RegValue[REG_ESP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESP 8H :%x\n", ((_regdump.regcontext.csp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESP_8H].back());

		if (RegValue[REG_ESP_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.csp) & 0xff, 8);
			RegValue[REG_ESP_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.csp) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ESP_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;

		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_ESI:
		
		if (RegValue[REG_ESI_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.csi) >> 16, 16);
			RegValue[REG_ESI_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_ESI_16H].back());
		
		if (RegValue[REG_ESI_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.csi) & 0xffff) >> 8, 8);
			RegValue[REG_ESI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESI_8H].back());
		
		if (RegValue[REG_ESI_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.csi) & 0xff, 8);
			RegValue[REG_ESI_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8L :%x\n", (_regdump.regcontext.csi) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ESI_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_EDI:
		
		if (RegValue[REG_EDI_16H].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cdi) >> 16, 16);
			RegValue[REG_EDI_16H].push_back(rstIR);
			irList.push_back(rstIR);
		}
		op1 = (RegValue[REG_EDI_16H].back());
		
		if (RegValue[REG_EDI_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cdi) & 0xffff) >> 8, 8);
			RegValue[REG_EDI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8H :%x\n", ((_regdump.regcontext.cdi) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDI_8H].back());
		
		if (RegValue[REG_EDI_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cdi) & 0xff, 8);
			RegValue[REG_EDI_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8L :%x\n", (_regdump.regcontext.cdi) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_EDI_8L].back());

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op1, op2, op3);
		rstIR->Size = 32;
		irList.push_back(rstIR);
		return rstIR;
		break;

		// 16 비트 레지스터
	case ZYDIS_REGISTER_AX:
		if (RegValue[REG_EAX_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cax) & 0xffff) >> 8, 8);
			RegValue[REG_EAX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = RegValue[REG_EAX_8H].back();

		if (RegValue[REG_EAX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cax) & 0xff, 8);
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

				rstIR = CraeteBVVIR(foldedConstValue, 16);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.cbx) & 0xffff)>>8, 8);
			RegValue[REG_EBX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBX_8H].back());

		if (RegValue[REG_EBX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
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

				rstIR = CraeteBVVIR(foldedConstValue, 16);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
			RegValue[REG_ECX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8H :%x\n", ((_regdump.regcontext.cbx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ECX_8H].back());

		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
			RegValue[REG_ECX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ECX 8L :%x\n", (_regdump.regcontext.cbx) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op3 = (RegValue[REG_ECX_8L].back());

		if (dynamic_cast<IR*>(op2) &&
			dynamic_cast<IR*>(op3))
		{
			if (dynamic_cast<IR*>(op2)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(op3)->opr == IR::OPR::OPR_BVV)
			{
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op2)->Operands[0]->valuePtr)->intVar << 8;
				op3ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(op3)->Operands[0]->valuePtr)->intVar & 0xff;
				foldedConstValue = op2ConstValue | op3ConstValue;

				rstIR = CraeteBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_DX:
		if (RegValue[REG_EDX_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cdx) & 0xffff) >> 8, 8);
			RegValue[REG_EDX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDX 8H :%x\n", ((_regdump.regcontext.cdx) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDX_8H].back());

		if (RegValue[REG_EDX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cdx) & 0xff, 8);
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

				rstIR = CraeteBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, op2, op3);
		rstIR->Size = 16;
		//irList.push_back(rstIR);
		return rstIR;
		break;
	case ZYDIS_REGISTER_BP:
		if (RegValue[REG_EBP_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cbp) & 0xffff) >> 8, 8);
			RegValue[REG_EBP_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EBP 8H :%x\n", ((_regdump.regcontext.cbp) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EBP_8H].back());

		if (RegValue[REG_ECX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
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

				rstIR = CraeteBVVIR(foldedConstValue, 16);
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
		break;
	case ZYDIS_REGISTER_SI:
		if (RegValue[REG_ESI_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.csi) & 0xffff) >> 8, 8);
			RegValue[REG_ESI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg ESI 8H :%x\n", ((_regdump.regcontext.csi) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_ESI_8H].back());

		if (RegValue[REG_ESI_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.csi) & 0xff, 8);
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

				rstIR = CraeteBVVIR(foldedConstValue, 16);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.cdi) & 0xffff) >> 8, 8);
			RegValue[REG_EDI_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EDI 8H :%x\n", ((_regdump.regcontext.cdi) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EDI_8H].back());

		if (RegValue[REG_EDI_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cdi) & 0xff, 8);
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

				rstIR = CraeteBVVIR(foldedConstValue, 16);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.cax) & 0xffff) >> 8, 8);
			RegValue[REG_EAX_8H].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8H :%x\n", ((_regdump.regcontext.cax) & 0xffff) >> 8);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EAX_8H].back());
		return op2;
		break;

	case ZYDIS_REGISTER_AL:
		if (RegValue[REG_EAX_8L].size() == 0)
		{
			rstIR = CraeteBVVIR((_regdump.regcontext.cax) & 0xff, 8);
			RegValue[REG_EAX_8L].push_back(rstIR);
#ifdef DEBUG
			_plugin_logprintf("Reg EAX 8L :%x\n", (_regdump.regcontext.cax) & 0xff);
#endif
			irList.push_back(rstIR);
		}
		op2 = (RegValue[REG_EAX_8L].back());
		return op2;
		break;

	case ZYDIS_REGISTER_BH:
		if (RegValue[REG_EBX_8H].size() == 0)
		{
			rstIR = CraeteBVVIR(((_regdump.regcontext.cbx) & 0xffff) >> 8, 8);
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
			rstIR = CraeteBVVIR((_regdump.regcontext.cbx) & 0xff, 8);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.ccx) & 0xffff) >> 8, 8);
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
			rstIR = CraeteBVVIR((_regdump.regcontext.ccx) & 0xff, 8);
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
			rstIR = CraeteBVVIR(((_regdump.regcontext.cdx) & 0xffff) >> 8, 8);
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
			rstIR = CraeteBVVIR((_regdump.regcontext.cdx) & 0xff, 8);
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

void SaveMemoryValue(ZydisDecodedInstruction* decodedInstPtr, ZydisDecodedOperand* _decodedOperandPtr, Value* _regValue, REGDUMP& _regdump, vector<IR*>& irList)
{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* BaseValue;
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

	Value* offset0Value;
	Value* offset1Value;
	Value* offset2Value;
	Value* offset3Value;

	DWORD _targetMem;
	_targetMem = GetEffectiveAddress32(_decodedOperandPtr, _regdump);

	// Base
	if (_decodedOperandPtr->mem.base != ZYDIS_REGISTER_NONE)
	{
		BaseValue = GetRegisterValue(_decodedOperandPtr->mem.base, _regdump, irList);

		if (_decodedOperandPtr->mem.disp.has_displacement)
		{
			// [Base + Index*Scale + Disp]
			Value* BaseDisp = CraeteBinaryIR(BaseValue, Disp, IR::OPR_ADD);
			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{

			}

			// [Base + Disp]
			else
			{

			}
		}

		// Displacement가 없는경우
		else
		{
			// [Base + Index*Scale]
			if (_decodedOperandPtr->mem.index != ZYDIS_REGISTER_NONE)
			{

			}
			// [Base]
			else
			{
				if (decodedInstPtr->address_width == 32)
				{
					// Base 레지스터의 Value가 상수인 경우 EA는 상수이므로 해당 EA에 대한 Memory Value Pool에 저장한다.					
					{
						DWORD _memAddr = _targetMem;
						IR* reg8hh;
						IR* reg8hlIR;
						IR* reg8hIR;
						IR* reg8lIR;

						// 메모리에 저장할 Value가 상수인 경우
						if (dynamic_cast<ConstInt*>(_regValue))
						{
							printf("SaveMemoryValue -> Value is Constnat\n");
						}

						// 메모리에 저장할 Value가 상수가 아닌 경우
						else
						{
							if (MemValue.find(_memAddr + 3) != MemValue.end())
							{
								reg8hh = new IR(_regValue->Name, MemValue[_memAddr + 3].size(), IR::OPR::OPR_EXTRACT8HH, _regValue);
							}
							else
								reg8hh = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8HH, _regValue);

							MemValue[_memAddr + 3].push_back(reg8hh);
							irList.push_back(reg8hh);

							if (MemValue.find(_memAddr + 2) != MemValue.end())
							{
								reg8hlIR = new IR(_regValue->Name, MemValue[_memAddr + 2].size(), IR::OPR::OPR_EXTRACT8HL, _regValue);
							}
							else
								reg8hlIR = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8HL, _regValue);

							MemValue[_memAddr + 2].push_back(reg8hlIR);
							irList.push_back(reg8hlIR);

							if (MemValue.find(_memAddr + 1) != MemValue.end())
							{
								reg8hIR = new IR(_regValue->Name, MemValue[_memAddr + 1].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
							}
							else
								reg8hIR = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8H, _regValue);

							MemValue[_memAddr + 1].push_back(reg8hIR);
							irList.push_back(reg8hIR);

							if (MemValue.find(_memAddr) != MemValue.end())
							{
								reg8lIR = new IR(_regValue->Name, MemValue[_memAddr].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
							}
							else
								reg8lIR = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8L, _regValue);
							MemValue[_memAddr].push_back(reg8lIR);
							irList.push_back(reg8lIR);
							rstIR = CraeteStoreIR(BaseValue, _regValue, IR::OPR_STORE);
							rstIR->isHiddenRHS = true;
							irList.push_back(rstIR);
						}
					}

					// EA 가 상수가 아닌 경우 (EA가 알 수 없는 경우)
					// Store EA, Value

					{
						// rstIR0 = Load BaseValue
						rstIR = CraeteStoreIR(BaseValue, _regValue, IR::OPR_STORE);
						rstIR->isHiddenRHS = true;
						irList.push_back(rstIR);
					}
				}
			}
		}
	}

	// SIB

	// Disp
	return;
}

void SaveMemoryValue(ZydisRegister zydisRegister, Value* _regValue, BYTE _size, REGDUMP& _regdump, vector<IR*>& irList)

{
	IR* rstIR0 = nullptr;
	IR* rstIR1 = nullptr;
	IR* rstIR2 = nullptr;
	IR* rstIR3 = nullptr;
	IR* rstIR = nullptr;

	Value* BaseValue;
	Value* Disp;
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

	// Base
	BaseValue = GetRegisterValue(zydisRegister, _regdump, irList);

	// [Base]


	if (_size == 32)
	{
		// Base 레지스터의 Value가 상수인 경우 EA는 상수이므로 해당 EA에 대한 Memory Value Pool에 저장한다.
		if (dynamic_cast<IR*>(BaseValue)->opr == IR::OPR::OPR_BVV)
		{
			printf("SaveMemoryValue -> BaseValue is Constant\n");
			DWORD _memAddr = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(BaseValue)->Operands[0]->valuePtr)->intVar;
			IR* reg8hh;
			IR* reg8hlIR;
			IR* reg8hIR;
			IR* reg8lIR;

			// 메모리에 저장할 Value가 상수인 경우
			if (dynamic_cast<IR*>(_regValue)->opr == IR::OPR::OPR_BVV)
			{
				printf("SaveMemoryValue -> Value is Constnat\n");
			}

			// 메모리에 저장할 Value가 상수가 아닌 경우
			else
			{
				if (MemValue.find(_memAddr + 3) != MemValue.end())
				{
					reg8hh = new IR(_regValue->Name, MemValue[_memAddr + 3].size(), IR::OPR::OPR_EXTRACT8HH, _regValue);
				}
				else
					reg8hh = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8HH, _regValue);

				MemValue[_memAddr + 3].push_back(reg8hh);
				irList.push_back(reg8hh);

				if (MemValue.find(_memAddr + 2) != MemValue.end())
				{
					reg8hlIR = new IR(_regValue->Name, MemValue[_memAddr + 2].size(), IR::OPR::OPR_EXTRACT8HL, _regValue);
				}
				else
					reg8hlIR = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8HL, _regValue);

				MemValue[_memAddr + 2].push_back(reg8hlIR);
				irList.push_back(reg8hlIR);

				if (MemValue.find(_memAddr + 1) != MemValue.end())
				{
					reg8hIR = new IR(_regValue->Name, MemValue[_memAddr + 1].size(), IR::OPR::OPR_EXTRACT8H, _regValue);
				}
				else
					reg8hIR = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8H, _regValue);

				MemValue[_memAddr + 1].push_back(reg8hIR);
				irList.push_back(reg8hIR);

				if (MemValue.find(_memAddr) != MemValue.end())
				{
					reg8lIR = new IR(_regValue->Name, MemValue[_memAddr].size(), IR::OPR::OPR_EXTRACT8L, _regValue);
				}
				else
					reg8lIR = new IR(_regValue->Name, 0, IR::OPR::OPR_EXTRACT8L, _regValue);
				MemValue[_memAddr].push_back(reg8lIR);
				irList.push_back(reg8lIR);
				rstIR = CraeteStoreIR(BaseValue, _regValue, IR::OPR_STORE);
				rstIR->isHiddenRHS = true;
				irList.push_back(rstIR);
			}
		}

		// EA 가 상수가 아닌 경우 (EA가 알 수 없는 경우)
		// Store EA, Value
		else
		{
			// rstIR0 = Load BaseValue
			rstIR = CraeteStoreIR(BaseValue, _regValue, IR::OPR_STORE);
			rstIR->isHiddenRHS = true;
			irList.push_back(rstIR);
		}
	}



	return;
}

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
		if (MemValue.find(targetMem1)==MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CraeteBVVIR(readByte, 8);
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
			rstIR1 = CraeteBVVIR(readByte, 8);
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
			rstIR2 = CraeteBVVIR(readByte, 8);
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
			rstIR3 = CraeteBVVIR(readByte, 8);
			MemValue[targetMem4].push_back(rstIR3);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem4, readByte);
#endif
			irList.push_back(rstIR3);
		}
		offset4Value = MemValue[targetMem4].back();

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

				rstIR = CraeteBVVIR(foldedConstValue, 32);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

		rstIR = new IR(IR::OPR::OPR_CONCAT, offset1Value, offset2Value, offset3Value, offset4Value);
		rstIR->Size = 32;
		rstIR->OperandType = rstIR->OPERANDTYPE_MEMORY;
		irList.push_back(rstIR);
	}

	else if (_decodedOperandPtr->size == 16)
	{
		// Target Memory Address에 대한 Value* 를 얻는다.
		if (MemValue.find(targetMem1) == MemValue.end())
		{
			DbgMemRead(targetMem1, &readByte, 1);
			rstIR0 = CraeteBVVIR(readByte, 8);
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
			rstIR1 = CraeteBVVIR(readByte, 8);
			MemValue[targetMem2].push_back(rstIR1);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem2, readByte);
#endif
			irList.push_back(rstIR1);
		}
		offset2Value = MemValue[targetMem2].back();

		if (dynamic_cast<IR*>(offset1Value) &&
			dynamic_cast<IR*>(offset2Value))
		{
			if (dynamic_cast<IR*>(offset1Value)->opr == IR::OPR::OPR_BVV &&
				dynamic_cast<IR*>(offset2Value)->opr == IR::OPR::OPR_BVV)
			{
				op1ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset1Value)->Operands[0]->valuePtr)->intVar << 24;
				op2ConstValue = dynamic_cast<ConstInt*>(dynamic_cast<IR*>(offset2Value)->Operands[0]->valuePtr)->intVar << 16;
				foldedConstValue = op1ConstValue | op2ConstValue;

				rstIR = CraeteBVVIR(foldedConstValue, 16);
				irList.push_back(rstIR);
				return rstIR;
			}

		}

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
			rstIR0 = CraeteBVVIR(readByte, 8);
			MemValue[targetMem1].push_back(rstIR0);
#ifdef DEBUG
			_plugin_logprintf("[GetMemoryValue] %p :%x\n", targetMem1, readByte);
#endif
			irList.push_back(rstIR0);
		}
		offset1Value = MemValue[targetMem1].back();
		return offset1Value;
	}

#ifdef DEBUG
	MessageBoxA(0, "rst", "rst", 0);
#endif

	return rstIR;
}

Value* GetImmValue(ZydisDecodedOperand* _decodedOperandPtr, vector<IR*>& irList)
{
	IR* immValue;
	if (_decodedOperandPtr->size == 32)
	{
		immValue = CraeteBVVIR(_decodedOperandPtr->imm.value.u, 32);
		irList.push_back(immValue);
		return immValue;
	}

	if (_decodedOperandPtr->size == 16)
	{
		immValue = CraeteBVVIR(_decodedOperandPtr->imm.value.u, 16);
		irList.push_back(immValue);
		return immValue;
	}

	if (_decodedOperandPtr->size == 8)
	{
		immValue = CraeteBVVIR(_decodedOperandPtr->imm.value.u, 8);
		irList.push_back(immValue);
		return immValue;
	}
}

Value* GetImmValue(DWORD _immValue, BYTE _size, vector<IR*>& irList)
{
	IR* immValue;
	if (_size == 32)
	{
		immValue = CraeteBVVIR(_immValue, 32);
		irList.push_back(immValue);
		return immValue;
	}

	if (_size == 16)
	{
		immValue = CraeteBVVIR(_immValue, 16);
		irList.push_back(immValue);
		return immValue;
	}

	if (_size == 8)
	{
		immValue = CraeteBVVIR(_immValue, 8);
		irList.push_back(immValue);
		return immValue;
	}
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

int CreateIR(ZydisDecodedInstruction* ptr_di, ZydisDecodedOperand* operandPTr, REGDUMP& _regdump, DWORD _offset)
{
	Value* Op1 = nullptr;
	Value* Op2 = nullptr;
	Value* RealOperand;

	IR* rst;
	IR* ExtractIR;
	IR* storeIR;

	vector<IR*> irList;

	_plugin_logprintf("CreateIR\n");

	switch (ptr_di->mnemonic)
	{
	case ZYDIS_MNEMONIC_ADD:
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("GenerateOPR_ADD Error (Operand is not matched)\n");
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_ADD);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;

	case ZYDIS_MNEMONIC_MOV:
		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);
		SetOperand(ptr_di, &operandPTr[0], Op2, _regdump, irList);
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;

	case ZYDIS_MNEMONIC_SUB:
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		if (ptr_di->opcode == 0x83)
			operandPTr[1].size = operandPTr[0].size;

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

		// 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			_plugin_logprintf("GenerateOPR_SUB Error (Operand is not matched)\n");
			return 0;
		}
		// Constant Folding 가능한 경우 CreateBinaryIR를 호출하지 않고 Imm Value 생성

		// Constant Folding 가능 조건이 아닌 경우 IR 생성
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SUB);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;

		//case ZYDIS_MNEMONIC_AND:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Xor, Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_XOR:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Xor, Op1, Op2, _offset);
		//	return 1;
		//	break;

		//case ZYDIS_MNEMONIC_BTS:
		//	GenerateBinaryAndDestinationIsOpIR(BinaryOp::BinaryOps::Bts, Op1, Op2, _offset);
		//	return 1;
		//	break;

	case ZYDIS_MNEMONIC_SAR:
		CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SAR);
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
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_ROL Error (Operand is not matched)\n");
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_ROL);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;

	case ZYDIS_MNEMONIC_OR:
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

														 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_OR Error (Operand is not matched)\n");
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_OR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_ADC:
		break;
	case ZYDIS_MNEMONIC_SBB:
		break;
	case ZYDIS_MNEMONIC_AND:
		break;
	case ZYDIS_MNEMONIC_DAA:
		break;
	case ZYDIS_MNEMONIC_DAS:
		break;
	case ZYDIS_MNEMONIC_XOR:
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_XOR Error (Operand is not matched)\n");
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_XOR);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_AAA:
		break;
	case ZYDIS_MNEMONIC_CMP:
		break;
	case ZYDIS_MNEMONIC_AAS:
		break;
	case ZYDIS_MNEMONIC_INC:
		break;
	case ZYDIS_MNEMONIC_DEC:
		break;
	case ZYDIS_MNEMONIC_PUSH:
		// ESP		
		Op1 = GetRegisterValue(ZYDIS_REGISTER_ESP, _regdump, irList);
		Op2 = GetImmValue(4, operandPTr[0].size, irList);
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_SUB);
		irList.push_back(rst);
		SaveRegisterValue(ZYDIS_REGISTER_ESP, rst, operandPTr[0].size, irList);

		RealOperand = GetOperand(ptr_di, &operandPTr[0], _regdump, irList);
		SaveMemoryValue(ZYDIS_REGISTER_ESP, RealOperand, operandPTr[0].size, _regdump, irList);

		IRList.insert(make_pair(_offset, irList));
		break;
	case ZYDIS_MNEMONIC_POP:
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
		break;
	case ZYDIS_MNEMONIC_JNZ:
		break;
	case ZYDIS_MNEMONIC_JBE:
		break;
	case ZYDIS_MNEMONIC_JNBE:
		break;
	case ZYDIS_MNEMONIC_JS:
		break;
	case ZYDIS_MNEMONIC_JNS:
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
		break;
	case ZYDIS_MNEMONIC_XCHG:
		break;
	case ZYDIS_MNEMONIC_LEA:
		break;
	case ZYDIS_MNEMONIC_CWD:
		break;
	case ZYDIS_MNEMONIC_CBW:
		break;
	case ZYDIS_MNEMONIC_CDQ:
		break;
	case ZYDIS_MNEMONIC_PUSHFD:
		break;
	case ZYDIS_MNEMONIC_POPFD:
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
		break;
	case ZYDIS_MNEMONIC_SHR:
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
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_RCL Error (Operand is not matched)\n");
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_RCL);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_RCR:
		break;
	case ZYDIS_MNEMONIC_ROR:
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
		break;
	case ZYDIS_MNEMONIC_CMC:
		break;
	case ZYDIS_MNEMONIC_NOT:
		break;
	case ZYDIS_MNEMONIC_NEG:
		break;
	case ZYDIS_MNEMONIC_MUL:
		break;
	case ZYDIS_MNEMONIC_DIV:
		break;
	case ZYDIS_MNEMONIC_IDIV:
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
	case ZYDIS_MNEMONIC_BSR:
		break;
	case ZYDIS_MNEMONIC_BSWAP:
		break;
	case ZYDIS_MNEMONIC_BT:
	case ZYDIS_MNEMONIC_BTS:
	case ZYDIS_MNEMONIC_BTR:
	case ZYDIS_MNEMONIC_BTC:
		Op1 = GetOperand(ptr_di, &operandPTr[0], _regdump, irList); // x86 오퍼랜드를 Get하는 IR을 생성한다.

		Op2 = GetOperand(ptr_di, &operandPTr[1], _regdump, irList);// x86 오퍼랜드를 Get하는 IR을 생성한다.

												 // 두 개의 오퍼랜드는 동일해야 한다.
		if (Op1->Size != Op2->Size)
		{
			printf("GenerateOPR_RCL Error (Operand is not matched)\n");
			return 0;
		}
		rst = CraeteBinaryIR(Op1, Op2, IR::OPR::OPR_RCL);
		irList.push_back(rst);

		// EFLAG 관련 IR 추가

		SaveRegisterValue(operandPTr[0].reg.value, rst, operandPTr[0].size, irList); // x86 오퍼랜드를 Set하는 IR을 생성한다.
		IRList.insert(make_pair(_offset, irList));
		return 1;
		break;
	case ZYDIS_MNEMONIC_MOVSX:
		break;
	case ZYDIS_MNEMONIC_MOVZX:
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
		break;
		//	break;
		//case ZYDIS_MNEMONIC_MOV:
		//	//CreateMovIR(UnaryOp::UnaryOps::Mov, ptr_di, _offset);
		//	GenerateMovIR(Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_MOVZX:
		//	//CreateMovIR(UnaryOp::UnaryOps::Mov, ptr_di, _offset);
		//	GenerateMovIR(Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_MOVSX:
		//	//CreateMovIR(UnaryOp::UnaryOps::Mov, ptr_di, _offset);
		//	GenerateMovIR(Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_CMOVNBE:
		//	//CreateMovIR(UnaryOp::UnaryOps::Mov, ptr_di, _offset);
		//	GenerateMovIR(Op1, Op2, _offset);
		//	return 1;
		//	break;
		//case ZYDIS_MNEMONIC_PUSH:
		//	CreatePush32IR(UnaryOp::UnaryOps::Mov, ptr_di, Op1, _offset);
		//	return 2;
		//	break;
		//case ZYDIS_MNEMONIC_PUSHFD:
		//	CreatePushfd32IR(UnaryOp::UnaryOps::Mov, ptr_di, _offset);
		//	return 2;
		//	break;
		//case ZYDIS_MNEMONIC_POP:
		//	CreatePop32IR(UnaryOp::UnaryOps::Mov, ptr_di, Op1, _offset);
		//	return 2;
		//	break;
		//case ZYDIS_MNEMONIC_XCHG:
		//	GenerateXchgIR(Op1, Op2, _offset);
		//	return 3;
		//	break;
	default:
		printf("Not Implemented Instruction :%s\n", ZydisMnemonicGetString(ptr_di->mnemonic));
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
		_plugin_logprintf("GetEffectiveAddress32 %p\n", base + si + disp);
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
	case IR::OPR::OPR_ADD:
		_plugin_logprintf("%s = OPR_ADD %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SUB:
		_plugin_logprintf("%s = OPR_SUB %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_XOR:
		_plugin_logprintf("%s = OPR_XOR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_ROL:
		_plugin_logprintf("%s = OPR_ROL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_SAR:
		_plugin_logprintf("%s = OPR_SAR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_LOAD:
		_plugin_logprintf("%s = LOAD %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case IR::OPR::OPR_STORE:
		_plugin_logprintf("STORE %s %s\n", _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
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
	case IR::OPR::OPR_ADD:
		return string("OPR_ADD");
		break;
	case IR::OPR::OPR_SUB:
		return string("OPR_SUB");
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
	case IR::OPR::OPR_ROL:
		return string("OPR_ROL");
		break;
	case IR::OPR::OPR_SAR:
		return string("OPR_SAR");
		break;
	case IR::OPR::OPR_LOAD:
		return string("OPR_LOAD");
		break;
	case IR::OPR::OPR_STORE:
		return string("OPR_X");
		break;
	default:
		return string("OPR_STORE");
		break;
	}
}

//string printOpr(IR::OPR _opr)
//{
//	switch (_opr)
//	{
//	case IR::OPR::OPR_CONCAT:
//		return string("Concat");
//		break;
//	case IR::OPR::OPR_BVV:
//		return string("OPR_BVV");
//		break;
//	case IR::OPR::OPR_EXTRACT16H:
//		return string("EXTRACT16H");
//		break;
//	case IR::OPR::OPR_EXTRACT8HH:
//		return string("EXTRACT8HH");
//		break;
//	case IR::OPR::OPR_EXTRACT8HL:
//		return string("EXTRACT8HL");
//		break;
//	case IR::OPR::OPR_EXTRACT8H:
//		return string("EXTRACT8H");
//		break;
//	case IR::OPR::OPR_EXTRACT8L:
//		return string("EXTRACT8L");
//		break;
//	case IR::OPR::OPR_ADD:
//		return string("OPR_ADD");
//		break;
//	case IR::OPR::OPR_SUB:
//		return string("OPR_SUB");
//		break;
//	case IR::OPR::OPR_OR:
//		return string("OPR_OR");
//		break;
//	case IR::OPR::OPR_AND:
//		return string("OPR_AND");
//		break;
//	case IR::OPR::OPR_XOR:
//		return string("OPR_XOR");
//		break;
//	case IR::OPR::OPR_ROL:
//		return string("OPR_ROL");
//		break;
//	case IR::OPR::OPR_SAR:
//		return string("OPR_SAR");
//		break;
//	case IR::OPR::OPR_LOAD:
//		return string("OPR_LOAD");
//		break;
//	case IR::OPR::OPR_STORE:
//		return string("OPR_X");
//		break;
//	default:
//		return string("OPR_STORE");
//		break;
//	}
//}

string EvaluateExpTree(std::shared_ptr<BTreeNode> bt)
{
	string op1, op2, op3, op4;
	string exprString;

	if (bt->left == NULL)
	{
		if (bt->m_NodeType == NT_SYMVAR)
		{
			if (symbolExprMap.find(bt->nodeName) != symbolExprMap.end())
			{
				z3Equation = symbolExprMap[bt->nodeName];
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
			return "(Concat " + op1 + " " + op2 + " " + op3 + ")";
		}

		else
		{
			return "(Concat " + op1 + " " + op2 + ")";
		}
	}

	else if (bt->nodeName.compare("EXTRACT16H") == 0)
	{
		return "(Extract 31 16  " + op1 + ")";
	}

	else if (bt->nodeName.compare("EXTRACT8H") == 0)
	{
		return "(Extract 15 8  " + op1 + ")";
	}

	else if (bt->nodeName.compare("EXTRACT8L") == 0)
	{
		return "(Extract 7 0  " + op1 + ")";
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
			exprString = bt->nodeName;
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
				_plugin_logprintf("Test : %x\n", (unsigned int)dynamic_cast<ConstInt*>(bt->valPtr)->intVar);
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