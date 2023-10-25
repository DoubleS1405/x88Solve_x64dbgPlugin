#pragma once
#include <Windows.h>
#include <stdio.h>
#include <Zycore/LibC.h>
#include <Zydis/Zydis.h>
#include <list>
#include <set>
#include <map>
#include <vector>
#include <string>
#include "plugin.h"
#include "AST.h"
#include "z3++.h"


using namespace std;

#define REG_EAX_16H (3*0)+0
#define REG_EAX_8H (3*0)+1
#define REG_EAX_8L (3*0)+2

#define REG_EBX_16H (3*1)+0
#define REG_EBX_8H (3*1)+1
#define REG_EBX_8L (3*1)+2

#define REG_ECX_16H (3*2)+0
#define REG_ECX_8H (3*2)+1
#define REG_ECX_8L (3*2)+2

#define REG_EDX_16H (3*3)+0
#define REG_EDX_8H (3*3)+1
#define REG_EDX_8L (3*3)+2

#define REG_EBP_16H (3*4)+0
#define REG_EBP_8H (3*4)+1
#define REG_EBP_8L (3*4)+2

#define REG_ESP_16H (3*5)+0
#define REG_ESP_8H (3*5)+1
#define REG_ESP_8L (3*5)+2

#define REG_ESI_16H (3*6)+0
#define REG_ESI_8H (3*6)+1
#define REG_ESI_8L (3*6)+2

#define REG_EDI_16H (3*7)+0
#define REG_EDI_8H (3*7)+1
#define REG_EDI_8L (3*7)+2

class OPERAND;
class IR;
extern DWORD cntd;

class Value
{
public:
	enum VALUE_TYPE
	{
		OPERANDTYPE_REGISTER,
		OPERANDTYPE_MEMORY,
		OPERANDTYPE_CONSTANT
	};

	VALUE_TYPE OperandType;
	BYTE Size; // 8, 16, 32
	set<OPERAND*> UseList; // 현재 Value를 Use 하는 오퍼랜드
	std::string Name;
	bool isHiddenRHS = false;
	bool isTainted = false;

	shared_ptr<BTreeNode> ast;

	Value();

	Value::Value(const Value& valRef);

	Value(DWORD val, WORD size);

	Value(string ValName, DWORD idx);

	Value(string ValName, DWORD idx, BOOL _isTainted);

	virtual void test() {};

};

class OPERAND
{
public:
	IR* parent;
	Value* valuePtr;
};

class IR : public Value
{
public:
	enum OPR
	{
		OPR_NONE,
		OPR_CONCAT,
		OPR_EXTRACT,
		OPR_EXTRACT16H,
		OPR_EXTRACT8HH,
		OPR_EXTRACT8HL,
		OPR_EXTRACT8H,
		OPR_EXTRACT8L,
		OPR_ZEXT,
		OPR_SEXT,
		OPR_BVV, // BitVecVal 
		OPR_GET32INT,
		OPR_GET16INT,
		OPR_GET8INT,
		OPR_STORE,
		OPR_LOAD,
		OPR_ADD,
		OPR_ADC,
		OPR_SUB,
		OPR_MUL,
		OPR_AND,
		OPR_OR,
		OPR_XOR,
		OPR_NOT,
		OPR_BT,
		OPR_BTC,
		OPR_BSF,
		OPR_BSWAP,
		OPR_RCL,
		OPR_RCR,
		OPR_ROL,
		OPR_ROR,
		OPR_SAR,
		OPR_SHL,
		OPR_SHR,
	};
	OPR opr;
	vector<OPERAND*> Operands;
	OPERAND* AddOperand(Value* OperandValuePtr)
	{
		OPERAND* oprPtr = new OPERAND;
		oprPtr->parent = this;
		oprPtr->valuePtr = OperandValuePtr;
		Operands.push_back(oprPtr);
		OperandValuePtr->UseList.insert(oprPtr);
		return oprPtr;
	}

	IR(OPR _opr)
	{
		opr = _opr;
	}

	IR(OPR _opr, Value* op1);

	IR(string ValName, DWORD index, OPR _opr, Value* op1);

	IR(OPR _opr, Value* op1, Value* op2);

	IR(OPR _opr, Value* op1, Value* op2, BYTE size);

	IR(string ValName, DWORD index, OPR _opr, Value* op1, Value* op2);

	IR(OPR _opr, Value* op1, Value* op2, Value* op3);
//#define DEBUG
	IR(OPR _opr, Value* op1, Value* op2, Value* op3, Value* op4);
	string printOpr(OPR _opr);
};

class ConstInt : public IR
{
public:
	DWORD intVar;

public:
	ConstInt(OPR opr, DWORD var) :IR(opr)
	{
		switch (opr)
		{
		case OPR_BVV:
			Size = 32;
			break;

		case OPR_GET16INT:
			Size = 16;
			break;

		case OPR_GET8INT:
			Size = 8;
			break;
		}
		intVar = var;
	}
};

IR* CraeteLoadIR(Value* op1, IR::OPR _opr);

IR* CraeteUnaryIR(Value* op1, IR::OPR _opr);

IR* CraeteBinaryIR(Value* op1, Value* op2, IR::OPR _opr);

IR* CreateAddIR(Value* op1, Value* op2);

IR* CreateSubIR(Value* op1, Value* op2);

IR* CraeteStoreIR(Value* op1, Value* op2, IR::OPR _opr);

IR* CraeteBVVIR(DWORD intVal, BYTE size);

IR* CraeteZeroExtendIR(Value* _val, BYTE size, vector<IR*>& irList);

IR* CraeteSignExtendIR(Value* _val, BYTE size, vector<IR*>& irList);

int CreateIR(ZydisDecodedInstruction* ptr_di, ZydisDecodedOperand* operandPTr, REGDUMP& regdump, DWORD _offset);

DWORD GetEffectiveAddress32(ZydisDecodedOperand* _decodedOperandPtr, REGDUMP& regdump);

void printIR(IR* _irPtr);

z3::expr GetZ3ExprFromTree(std::shared_ptr<BTreeNode> bt);