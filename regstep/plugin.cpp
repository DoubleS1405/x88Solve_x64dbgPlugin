#include "plugin.h"
#include <Zycore/LibC.h>
#include <Zydis/Zydis.h>
#include <list>
#include <set>
#include <map>
#include "Value.h"
#include "optimize.h"
#include "StaticValue.h"

using namespace Script;
using namespace std;

static int traceCount = 0;
bool isFirst = true;

map<string, z3::expr*> symbolExprMap;
vector<Value*> StatusFlagsValue[5];
vector<Value*> RegValue[ZYDIS_REGISTER_MAX_VALUE];
extern vector<StaticValue*> RegValue2[ZYDIS_REGISTER_MAX_VALUE];
map<DWORD, vector<Value*>> MemValue;
multimap<DWORD, vector<IR*>> IRList;
set<DWORD> taintList;

extern map<DWORD, IR*> vaddList;

z3::context* z3Context;
z3::expr* z3Equation;

DWORD cntd = 0;

DWORD testNodeCnt = 0;

enum
{
	MENU_ENABLED,
	SET_MEM_SYMBOL
};

std::set<ZydisRegister> regsTainted;
vector<duint> testArr[ZYDIS_REGISTER_MAX_VALUE];
std::set<duint> addressTainted;
map<duint, vector<duint>> MemDefMap;


static bool regstepEnabled = true;
ZydisDecoder Decoder;
duint StartAddress;
duint EndAddress;

void printIRStatic2(StaticIR* _irPtr)
{
	if (dynamic_cast<StaticConstInt*>(_irPtr))
	{
		if (dynamic_cast<StaticConstInt*>(_irPtr)->opr == StaticIR::OPR::OPR_BVV)
		{

			_plugin_logprintf("%x\n", dynamic_cast<StaticConstInt*>(_irPtr)->intVar);
		}
	}
	switch (_irPtr->opr)
	{
	case StaticIR::OPR::OPR_CONCAT:
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
	case StaticIR::OPR::OPR_BVV:
		_plugin_logprintf("%s =  BVV(%x, %d)\n", _irPtr->Name.c_str(), dynamic_cast<StaticConstInt*>(_irPtr->Operands[0]->valuePtr)->intVar, dynamic_cast<StaticConstInt*>(_irPtr->Operands[1]->valuePtr)->intVar);
		break;
	case StaticIR::OPR::OPR_EXTRACT16H:
		_plugin_logprintf("%s = OPR_EXTRACT16H %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_EXTRACT8HH:
		_plugin_logprintf("%s = OPR_EXTRACT8HH %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_EXTRACT8HL:
		_plugin_logprintf("%s = OPR_EXTRACT8HL %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_EXTRACT8H:
		_plugin_logprintf("%s = OPR_EXTRACT8H %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_EXTRACT8L:
		_plugin_logprintf("%s = OPR_EXTRACT8L %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_ZEXT:
		_plugin_logprintf("%s = OPR_ZEXT %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_SEXT:
		_plugin_logprintf("%s = OPR_SEXT %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_ADD:
		_plugin_logprintf("%s = OPR_ADD %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_ADC:
		_plugin_logprintf("%s = OPR_ADC %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_SUB:
		_plugin_logprintf("%s = OPR_SUB %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_MUL:
		_plugin_logprintf("%s = OPR_MUL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_AND:
		_plugin_logprintf("%s = OPR_AND %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_OR:
		_plugin_logprintf("%s = OPR_OR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_XOR:
		_plugin_logprintf("%s = OPR_XOR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_NOT:
		_plugin_logprintf("%s = OPR_NOT %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_NEG:
		_plugin_logprintf("%s = OPR_NEG %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_RCL:
		_plugin_logprintf("%s = OPR_RCL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_RCR:
		_plugin_logprintf("%s = OPR_RCR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_SHL:
		_plugin_logprintf("%s = OPR_SHL %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_SHR:
		_plugin_logprintf("%s = OPR_SHR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_SAR:
		_plugin_logprintf("%s = OPR_SAR %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_BTC:
		_plugin_logprintf("%s = OPR_BTC %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_BSF:
		_plugin_logprintf("%s = OPR_BSF %s %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_BSWAP:
		_plugin_logprintf("%s = OPR_BSWAP %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_LOAD32:
		_plugin_logprintf("%s = LOAD %s\n", _irPtr->Name.c_str(), _irPtr->Operands[0]->valuePtr->Name.c_str());
		break;
	case StaticIR::OPR::OPR_STORE32:
		_plugin_logprintf("STORE %s %s\n", _irPtr->Operands[0]->valuePtr->Name.c_str(), _irPtr->Operands[1]->valuePtr->Name.c_str());
		break;
	default:
		_plugin_logprintf("Not Implemented print :%d\n", _irPtr->opr);
		break;
	}
}

void initReg()
{
	/*
		1. EAX Sym Value 생성
		2. z3Context->bv_const("EAX_SYM", 32)
		3. 16H, 8H, 8L 에 Extract 하여 저장
	*/
	z3Context = new z3::context;

	_plugin_logprintf("initReg() start\n");
	Value* eaxValue = new Value("EAX", 0);
	eaxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(eaxValue->Name));
	eaxValue->ast->m_NodeType = NT_SYMVAR;
	eaxValue->Size = 32;
	z3::expr* z3eax = new z3::expr(z3Context->bv_const("EAX(0)", 32));
	z3Equation = new z3::expr(*z3eax);
	symbolExprMap["EAX(0)"] = z3eax;

	IR* reg16hIR = new IR("EAX16H", RegValue[REG_EAX_16H].size(), IR::OPR::OPR_EXTRACT16H, eaxValue);
	reg16hIR->Size = 16;
	RegValue[REG_EAX_16H].push_back(reg16hIR);

	IR* regEax8hIR = new IR("EAX8H", RegValue[REG_EAX_8H].size(), IR::OPR::OPR_EXTRACT8H, eaxValue);
	regEax8hIR->Size = 8;
	RegValue[REG_EAX_8H].push_back(regEax8hIR);

	IR* regEax8lIR = new IR("EAX8L", RegValue[REG_EAX_8L].size(), IR::OPR::OPR_EXTRACT8L, eaxValue);
	regEax8lIR->Size = 8;
	RegValue[REG_EAX_8L].push_back(regEax8lIR);

	//Value* eax_16h = new Value("EAX.16H", RegValue[REG_EAX_16H].size());//CraeteBVVIR(0xaaaa, 16);
	//eax_16h->ast = std::make_shared<BTreeNode>(MakeBTreeNode(eax_16h->Name));
	//eax_16h->ast->m_NodeType = NT_SYMVAR;
	//eax_16h->Size = 16;
	//RegValue[REG_EAX_16H].push_back(eax_16h);
	//z3::expr*  z3eax_16hVar = new z3::expr(z3Context->bv_const("EAX.16H", 16));
	//z3Equation = new z3::expr(*z3eax_16hVar);
	//symbolExprMap["EAX.16H"] = z3eax_16hVar;
	//eax_16h->isTainted = true;

	//Value* eax_lh = new Value("EAX.8H", RegValue[REG_EAX_8H].size()); // CraeteBVVIR(0xa2, 8);
	//eax_lh->ast = std::make_shared<BTreeNode>(MakeBTreeNode(eax_lh->Name));
	//eax_lh->ast->m_NodeType = NT_SYMVAR;
	//eax_lh->Size = 8;
	//RegValue[REG_EAX_8H].push_back(eax_lh);
	//z3::expr* z3eax_8hVar = new z3::expr(z3Context->bv_const("EAX.8H", 8));
	//symbolExprMap["EAX.8H"] = z3eax_8hVar;
	//eax_lh->isTainted = true;

	//Value* eax_ll = new Value("EAX.8L", RegValue[REG_EAX_8L].size()); //CraeteBVVIR(0xa1, 8);
	//eax_ll->Size = 8;
	//eax_ll->ast = std::make_shared<BTreeNode>(MakeBTreeNode(eax_ll->Name));
	//eax_ll->ast->m_NodeType = NT_SYMVAR;
	//RegValue[REG_EAX_8L].push_back(eax_ll);
	//z3::expr* z3eax_8lVar = new z3::expr(z3Context->bv_const("EAX.8L", 8));
	//symbolExprMap["EAX.8L"] = z3eax_8lVar;
	//eax_ll->isTainted = true;

	//Value* ebxValue = new Value("EBX", 0);
	//ebxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ebxValue->Name));
	//ebxValue->ast->m_NodeType = NT_SYMVAR;
	//ebxValue->Size = 32;
	//z3::expr* z3ebx = new z3::expr(z3Context->bv_const("EBX(0)", 32));
	////z3Equation = new z3::expr(*z3ecx);
	//symbolExprMap["EBX(0)"] = z3ebx;


	//IR* regEbx16hIR = new IR("EBX16H", RegValue[REG_EBX_16H].size(), IR::OPR::OPR_EXTRACT16H, ebxValue);
	//regEbx16hIR->Size = 16;
	//RegValue[REG_EBX_16H].push_back(regEbx16hIR);

	//IR* regEbx8hIR = new IR("EBX8H", RegValue[REG_EBX_8H].size(), IR::OPR::OPR_EXTRACT8H, ebxValue);
	//regEbx8hIR->Size = 8;
	//RegValue[REG_EBX_8H].push_back(regEbx8hIR);

	//IR* regEbx8lIR = new IR("EBX8L", RegValue[REG_EBX_8L].size(), IR::OPR::OPR_EXTRACT8L, ebxValue);
	//regEbx8lIR->Size = 8;
	//RegValue[REG_EBX_8L].push_back(regEbx8lIR);

	Value* ebx_16h = new Value("EBX.16H", RegValue[REG_EBX_16H].size()); // CraeteBVVIR(0xbbbb, 16);
	ebx_16h->Size = 16;
	ebx_16h->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ebx_16h->Name));
	ebx_16h->ast->m_NodeType = NT_SYMVAR;
	//z3::expr* z3ebx_16hVar = new z3::expr(z3Context->bv_const("EBX.16H", 16));
	//symbolExprMap["EBX.16H"] = z3ebx_16hVar;
	RegValue[REG_EBX_16H].push_back(ebx_16h);

	Value* ebx_lh = new Value("EBX.8H", RegValue[REG_EBX_8H].size()); //CraeteBVVIR(0xb1, 8);
	ebx_lh->Size = 8;
	ebx_lh->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ebx_lh->Name));
	ebx_lh->ast->m_NodeType = NT_SYMVAR;
	//z3::expr* z3ebx_8hVar = new z3::expr(z3Context->bv_const("EBX.8H", 8));
	//symbolExprMap["EBX.8H"] = z3ebx_8hVar;
	RegValue[REG_EBX_8H].push_back(ebx_lh);

	Value* ebx_ll = new Value("EBX.8L", RegValue[REG_EBX_8L].size()); //CraeteBVVIR(0xb2, 8);
	ebx_ll->Size = 8;
	ebx_ll->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ebx_ll->Name));
	ebx_ll->ast->m_NodeType = NT_SYMVAR;
	//z3::expr* z3ebx_8lVar = new z3::expr(z3Context->bv_const("EBX.8L", 8));
	//symbolExprMap["EBX.8L"] = z3ebx_8lVar;
	RegValue[REG_EBX_8L].push_back(ebx_ll);

	//Value* ecxValue = new Value("ECX", 0);
	//ecxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ecxValue->Name));
	//ecxValue->ast->m_NodeType = NT_SYMVAR;
	//ecxValue->Size = 32;
	//z3::expr* z3ecx = new z3::expr(z3Context->bv_const("ECX(0)", 32));
	////z3Equation = new z3::expr(*z3ecx);
	//symbolExprMap["ECX(0)"] = z3ecx;


	//IR* regEcx16hIR = new IR("ECX16H", RegValue[REG_ECX_16H].size(), IR::OPR::OPR_EXTRACT16H, ecxValue);
	//regEcx16hIR->Size = 16;
	//RegValue[REG_ECX_16H].push_back(regEcx16hIR);

	//IR* regEcx8hIR = new IR("ECX8H", RegValue[REG_ECX_8H].size(), IR::OPR::OPR_EXTRACT8H, ecxValue);
	//regEcx8hIR->Size = 8;
	//RegValue[REG_ECX_8H].push_back(regEcx8hIR);

	//IR* regEcx8lIR = new IR("ECX8L", RegValue[REG_ECX_8L].size(), IR::OPR::OPR_EXTRACT8L, ecxValue);
	//regEcx8lIR->Size = 8;
	//RegValue[REG_ECX_8L].push_back(regEcx8lIR);

	Value* ECX_16h = new Value("ECX.16H", RegValue[REG_ECX_16H].size());
	ECX_16h->Size = 16;
	ECX_16h->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ECX_16h->Name));
	ECX_16h->ast->m_NodeType = NT_SYMVAR;
	//z3::expr* z3ecx_16hVar = new z3::expr(z3Context->bv_const("ECX.16H", 16));
	//symbolExprMap["ECX.16H(0)"] = z3ecx_16hVar;
	RegValue[REG_ECX_16H].push_back(ECX_16h);

	Value* ECX_lh = new Value("ECX.8H", RegValue[REG_ECX_8H].size());
	ECX_lh->Size = 8;
	ECX_lh->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ECX_lh->Name));
	ECX_lh->ast->m_NodeType = NT_SYMVAR;
	//z3::expr* z3ecx_8hVar = new z3::expr(z3Context->bv_const("ECX.8H", 8));
	//symbolExprMap["ECX.8H(0)"] = z3ecx_8hVar;
	RegValue[REG_ECX_8H].push_back(ECX_lh);

	Value* ECX_ll = new Value("ECX.8L", RegValue[REG_ECX_8L].size());
	ECX_ll->Size = 8;
	ECX_ll->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ECX_ll->Name));
	ECX_ll->ast->m_NodeType = NT_SYMVAR;
	//z3::expr* z3ecx_8lVar = new z3::expr(z3Context->bv_const("ECX.8L", 8));
	//symbolExprMap["ECX.8L(0)"] = z3ecx_8lVar;
	RegValue[REG_ECX_8L].push_back(ECX_ll);

	//Value* edxValue = new Value("EDX", 0);
	//edxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(edxValue->Name));
	//edxValue->ast->m_NodeType = NT_SYMVAR;
	//edxValue->Size = 32;
	//z3::expr* z3edx = new z3::expr(z3Context->bv_const("EDX(0)", 32));
	////z3Equation = new z3::expr(*z3ecx);
	//symbolExprMap["EDX(0)"] = z3edx;


	//IR* regEdx16hIR = new IR("EDX16H", RegValue[REG_EDX_16H].size(), IR::OPR::OPR_EXTRACT16H, ecxValue);
	//regEdx16hIR->Size = 16;
	//RegValue[REG_EDX_16H].push_back(regEdx16hIR);

	//IR* regEdx8hIR = new IR("EDX8H", RegValue[REG_EDX_8H].size(), IR::OPR::OPR_EXTRACT8H, ecxValue);
	//regEdx8hIR->Size = 8;
	//RegValue[REG_EDX_8H].push_back(regEdx8hIR);

	//IR* regEdx8lIR = new IR("EDX8L", RegValue[REG_EDX_8L].size(), IR::OPR::OPR_EXTRACT8L, ecxValue);
	//regEdx8lIR->Size = 8;
	//RegValue[REG_EDX_8L].push_back(regEdx8lIR);

	Value* EDX_16h = new Value("EDX.16H", RegValue[REG_EDX_16H].size());
	EDX_16h->ast = std::make_shared<BTreeNode>(MakeBTreeNode("EDX_16h"));
	EDX_16h->Size = 16;
	RegValue[REG_EDX_16H].push_back(EDX_16h);

	Value* EDX_lh = new Value("EDX.8H", RegValue[REG_EDX_8H].size());
	EDX_lh->ast = std::make_shared<BTreeNode>(MakeBTreeNode("EDX_lh"));
	EDX_lh->Size = 8;
	RegValue[REG_EDX_8H].push_back(EDX_lh);

	Value* EDX_ll = new Value("EDX.8L", RegValue[REG_EDX_8L].size());
	EDX_ll->ast = std::make_shared<BTreeNode>(MakeBTreeNode("EDX_ll"));
	EDX_ll->Size = 8;
	RegValue[REG_EDX_8L].push_back(EDX_ll);

	Value* ESI_16h = new Value("ESI.16H", RegValue[REG_ESI_16H].size());//CraeteBVVIR(0xeeee, 16);
	ESI_16h->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ESI_16h->Name));
	ESI_16h->Size = 16;
	RegValue[REG_ESI_16H].push_back(ESI_16h);

	Value* ESI_lh = new Value("ESI.8H", RegValue[REG_ESI_8H].size());//CraeteBVVIR(0xe2, 8);
	ESI_lh->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ESI_lh->Name));
	ESI_lh->Size = 8;
	RegValue[REG_ESI_8H].push_back(ESI_lh);

	Value* ESI_ll = new Value("ESI.8L", RegValue[REG_ESI_8L].size());//CraeteBVVIR(0xe1, 8);
	ESI_ll->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ESI_ll->Name));
	ESI_ll->Size = 8;
	RegValue[REG_ESI_8L].push_back(ESI_ll);

	Value* EDI_16h = new Value("EDI.16H", RegValue[REG_EDI_16H].size()); // CraeteBVVIR(0xffff, 16);
	EDI_16h->Size = 16;
	RegValue[REG_EDI_16H].push_back(EDI_16h);

	Value* EDI_lh = new Value("EDI.8H", RegValue[REG_EDI_8H].size()); //CraeteBVVIR(0xf2, 8);
	EDI_lh->Size = 8;
	RegValue[REG_EDI_8H].push_back(EDI_lh);

	Value* EDI_ll = new Value("EDI.8L", RegValue[REG_EDI_8L].size()); // CraeteBVVIR(0xf1, 8);
	EDI_ll->Size = 8;
	RegValue[REG_EDI_8L].push_back(EDI_ll);

	Value* EBP_16h = new Value("EBP.16H", RegValue[REG_EBP_16H].size());
	EBP_16h->Size = 16;
	RegValue[REG_EBP_16H].push_back(EBP_16h);

	Value* EBP_lh = new Value("EBP.8H", RegValue[REG_EBP_8H].size());
	EBP_lh->Size = 8;
	RegValue[REG_EBP_8H].push_back(EBP_lh);

	Value* EBP_ll = new Value("EBP.8L", RegValue[REG_EBP_8L].size());
	EBP_ll->Size = 8;
	RegValue[REG_EBP_8L].push_back(EBP_ll);

	Value* ESP_16h = new Value("ESP.16H", RegValue[REG_ESP_16H].size());
	ESP_16h->Size = 16;
	RegValue[REG_ESP_16H].push_back(ESP_16h);

	Value* ESP_lh = new Value("ESP.8H", RegValue[REG_ESP_8H].size());
	ESP_lh->Size = 8;
	RegValue[REG_ESP_8H].push_back(ESP_lh);

	Value* ESP_ll = new Value("ESP.8L", RegValue[REG_ESP_8L].size());
	ESP_ll->Size = 8;
	RegValue[REG_ESP_8L].push_back(ESP_ll);

	//Value* ESP_16h = CraeteBVVIR(0x9000, 16);
	//ESP_16h->Size = 16;
	//RegValue[REG_ESP_16H].push_back(ESP_16h);

	//Value* ESP_lh = CraeteBVVIR(0x10, 8);
	//ESP_lh->Size = 8;
	//RegValue[REG_ESP_8H].push_back(ESP_lh);

	//Value* ESP_ll = CraeteBVVIR(0x00, 8);
	//ESP_ll->Size = 8;
	//RegValue[REG_ESP_8L].push_back(ESP_ll);

	_plugin_logprintf("initReg() end\n");
}

duint GetEA(ZydisRegister _Base, ZydisRegister _Index, duint _Scale, int _Disp)
{
	duint BaseAddress = 0;
	duint IndexAddress = 0;
	duint Scale = _Scale;
	duint EA = 0;

	switch (_Base)
	{
	case ZYDIS_REGISTER_EAX:
		BaseAddress = Script::Register::GetEAX();
		break;
	case ZYDIS_REGISTER_EBX:
		BaseAddress = Script::Register::GetEBX();
		break;
	case ZYDIS_REGISTER_ECX:
		BaseAddress = Script::Register::GetECX();
		break;
	case ZYDIS_REGISTER_EDX:
		BaseAddress = Script::Register::GetEDX();
		break;
	case ZYDIS_REGISTER_EBP:
		BaseAddress = Script::Register::GetEBP();
		break;
	case ZYDIS_REGISTER_ESP:
		BaseAddress = Script::Register::GetESP();
		break;
	case ZYDIS_REGISTER_ESI:
		BaseAddress = Script::Register::GetESI();
		break;
	case ZYDIS_REGISTER_EDI:
		BaseAddress = Script::Register::GetEDI();
		break;
	default:
		BaseAddress = 0;
		break;
	}

	switch (_Index)
	{
	case ZYDIS_REGISTER_EAX:
		IndexAddress = Script::Register::GetEAX();
		break;
	case ZYDIS_REGISTER_EBX:
		IndexAddress = Script::Register::GetEBX();
		break;
	case ZYDIS_REGISTER_ECX:
		IndexAddress = Script::Register::GetECX();
		break;
	case ZYDIS_REGISTER_EDX:
		IndexAddress = Script::Register::GetEDX();
		break;
	case ZYDIS_REGISTER_EBP:
		IndexAddress = Script::Register::GetEBP();
		break;
	case ZYDIS_REGISTER_ESP:
		IndexAddress = Script::Register::GetESP();
		break;
	case ZYDIS_REGISTER_ESI:
		IndexAddress = Script::Register::GetESI();
		break;
	case ZYDIS_REGISTER_EDI:
		IndexAddress = Script::Register::GetEDI();
		break;
	default:
		IndexAddress = 0;
		break;
	}

	EA = BaseAddress + IndexAddress * Scale + _Disp;
	return EA;
}

// 레지스터 오퍼랜드가 Taint 될 경우 해당 레지스터에 포함된 (nested) 하위 레지스터를 Taint 시킨다.
// e.g EAX가 Taint되면 AX, AH, AL 레지스터를 Taint 시킨다.
void TainteRegdEx(ZydisRegister _reg, duint insAddr)
{
	testArr[_reg].push_back(insAddr);
	switch (_reg)
	{
	case ZYDIS_REGISTER_EAX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_AX);
		//_plugin_logprintf("%p Start Taint EAX:%p\n", info->cip, target + 0x1c);
		testArr[ZYDIS_REGISTER_AX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_AH);
		testArr[ZYDIS_REGISTER_AH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_AL);
		testArr[ZYDIS_REGISTER_AL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_AX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_AX);
		testArr[ZYDIS_REGISTER_AX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_AH);
		testArr[ZYDIS_REGISTER_AH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_AL);
		testArr[ZYDIS_REGISTER_AL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_EBX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_BX);
		testArr[ZYDIS_REGISTER_BX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_BH);
		testArr[ZYDIS_REGISTER_BH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_BL);
		testArr[ZYDIS_REGISTER_BL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_BX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_BX);
		testArr[ZYDIS_REGISTER_BX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_BH);
		testArr[ZYDIS_REGISTER_BH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_BL);
		testArr[ZYDIS_REGISTER_BL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_ECX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_CX);
		testArr[ZYDIS_REGISTER_CX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_CH);
		testArr[ZYDIS_REGISTER_CH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_CL);
		testArr[ZYDIS_REGISTER_CL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_CX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_CX);
		testArr[ZYDIS_REGISTER_CX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_CH);
		testArr[ZYDIS_REGISTER_CH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_CL);
		testArr[ZYDIS_REGISTER_CL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_EDX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_DX);
		testArr[ZYDIS_REGISTER_DX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_DH);
		testArr[ZYDIS_REGISTER_DH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_DL);
		testArr[ZYDIS_REGISTER_DL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_DX:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_DX);
		testArr[ZYDIS_REGISTER_DX].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_DH);
		testArr[ZYDIS_REGISTER_DH].push_back(insAddr);
		regsTainted.insert(ZYDIS_REGISTER_DL);
		testArr[ZYDIS_REGISTER_DL].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_EBP:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_BP);
		testArr[ZYDIS_REGISTER_BP].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_BP:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_BP);
		testArr[ZYDIS_REGISTER_BP].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_ESP:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_SP);
		testArr[ZYDIS_REGISTER_SP].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_SP:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_SP);
		testArr[ZYDIS_REGISTER_SP].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_ESI:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_SI);
		testArr[ZYDIS_REGISTER_SI].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_SI:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_SI);
		testArr[ZYDIS_REGISTER_SI].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_EDI:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_DI);
		testArr[ZYDIS_REGISTER_DI].push_back(insAddr);
		break;
	case ZYDIS_REGISTER_DI:
		regsTainted.insert(_reg);
		regsTainted.insert(ZYDIS_REGISTER_DI);
		testArr[ZYDIS_REGISTER_DI].push_back(insAddr);
		break;

	}
}

void UnTainteRegdEx(ZydisRegister _reg)
{
	switch (_reg)
	{
	case ZYDIS_REGISTER_EAX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_AX);
		//_plugin_logprintf("%p Start Taint EAX:%p\n", info->cip, target + 0x1c);
		regsTainted.erase(ZYDIS_REGISTER_AH);
		regsTainted.erase(ZYDIS_REGISTER_AL);
		break;
	case ZYDIS_REGISTER_AX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_AX);
		regsTainted.erase(ZYDIS_REGISTER_AH);
		regsTainted.erase(ZYDIS_REGISTER_AL);
		break;
	case ZYDIS_REGISTER_EBX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_BX);
		regsTainted.erase(ZYDIS_REGISTER_BH);
		regsTainted.erase(ZYDIS_REGISTER_BL);
		break;
	case ZYDIS_REGISTER_BX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_BX);
		regsTainted.erase(ZYDIS_REGISTER_BH);
		regsTainted.erase(ZYDIS_REGISTER_BL);
		break;
	case ZYDIS_REGISTER_ECX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_CX);
		regsTainted.erase(ZYDIS_REGISTER_CH);
		regsTainted.erase(ZYDIS_REGISTER_CL);
		break;
	case ZYDIS_REGISTER_CX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_CX);
		regsTainted.erase(ZYDIS_REGISTER_CH);
		regsTainted.erase(ZYDIS_REGISTER_CL);
		break;
	case ZYDIS_REGISTER_EDX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_DX);
		regsTainted.erase(ZYDIS_REGISTER_DH);
		regsTainted.erase(ZYDIS_REGISTER_DL);
		break;
	case ZYDIS_REGISTER_DX:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_DX);
		regsTainted.erase(ZYDIS_REGISTER_DH);
		regsTainted.erase(ZYDIS_REGISTER_DL);
		break;
	case ZYDIS_REGISTER_EBP:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_BP);
		break;
	case ZYDIS_REGISTER_BP:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_BP);
		break;
	case ZYDIS_REGISTER_ESP:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_SP);
		break;
	case ZYDIS_REGISTER_SP:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_SP);
		break;
	case ZYDIS_REGISTER_ESI:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_SI);
		break;
	case ZYDIS_REGISTER_SI:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_SI);
		break;
	case ZYDIS_REGISTER_EDI:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_DI);
		break;
	case ZYDIS_REGISTER_DI:
		regsTainted.erase(_reg);
		regsTainted.erase(ZYDIS_REGISTER_DI);
		break;

	}
}

void dfsNode(BridgeCFGraph cfg, BridgeCFNode CFNode, std::set <duint>& isVisited, std::set <duint>& s)
{
	isVisited.insert(CFNode.start);
	s.insert(CFNode.start);
	_plugin_logprintf("ghkdrmarud %p\n", CFNode.start);

	for (auto exitIt : CFNode.exits)
	{
		// 방문하지 않음
		if (isVisited.find(exitIt) == isVisited.end())
		{

			dfsNode(cfg, cfg.nodes[exitIt], isVisited, s);
		}
	}

	// exits 을 전부 방문함

}

void dfs(BridgeCFGraph cfg)
{
	duint start = cfg.nodes[cfg.entryPoint].start;
	std::set <duint> isVisited;
	std::set<duint> dfsPath;

	dfsNode(cfg, cfg.nodes[start], isVisited, dfsPath);


	char buf[128];
	for (auto it : dfsPath)
	{
		sprintf_s(buf, "%p", it);
	}

	_plugin_logprintf("%s\n", buf);
}

PLUG_EXPORT void CBANALYZE(CBTYPE cbType, PLUG_CB_ANALYZE* info)
{
	bool isInExit = false;
	auto graph = BridgeCFGraph(&info->graph, true);
	std::vector<duint> toRemoveExits;
	for (int i = 0; i <= graph.nodes.size(); i++)
	{
		for (auto nodeIt = graph.nodes.begin(); nodeIt != graph.nodes.end(); nodeIt++)
			//for (auto& nodeIt : graph.nodes)
		{
			if (nodeIt->second.instrs.size() > 0 && nodeIt->second.instrs.back().data)
			{
				//if (nodeIt.second.exits.size() == 1)
				if (nodeIt->second.instrs.back().data[0] == 0xe9 || nodeIt->second.instrs.back().data[0] == 0xeb)
				{
					if (nodeIt->second.exits.size() == 1)
					{
						duint target = nodeIt->second.exits.back();
						for (auto& nodeIt2 : graph.nodes)
						{
							auto it = find(nodeIt2.second.exits.begin(), nodeIt2.second.exits.end(), target);

							if (nodeIt2.first != nodeIt->first)
							{
								if (it != nodeIt2.second.exits.end())
								{
									isInExit = true;
									break;
								}
							}
						}
						if (isInExit == false)
						{
							auto& node1 = nodeIt->second;
							auto& targetExits = node1.exits;
							auto& instVect = node1.instrs;
							auto targetExit = node1.exits.back();
							//_plugin_logprintf("Node %p is mergerable to node %p\n", nodeIt->first, target);

							BASIC_INSTRUCTION_INFO bii;
							DbgDisasmFastAt(graph.nodes[target].end, &bii);
							duint toMergeBlockSize = graph.nodes[target].end - graph.nodes[target].start + bii.size;
							duint tempTarget = node1.instrs.back().addr;
							duint removedInstSize = 0;
							size_t originalInstVectorSize = instVect.size();
							if (originalInstVectorSize > 1)
							{
								BASIC_INSTRUCTION_INFO bi2;
								DbgDisasmFastAt(node1.instrs.back().addr, &bi2);
								removedInstSize = bi2.size;
								instVect.erase(node1.instrs.end() - 1);
							}
							targetExits.clear();
							targetExits.insert(targetExits.end(),
								graph.nodes[target].exits.begin(), graph.nodes[target].exits.end());

							//instVect.back().data[0] = 0x90;

							auto targetInstVecIt = instVect.end();

							BASIC_INSTRUCTION_INFO biiEnd;
							DbgDisasmFastAt(nodeIt->second.end, &biiEnd);

							instVect.insert(instVect.end(),
								graph.nodes[target].instrs.begin(), graph.nodes[target].instrs.end());

							/*
							// 병합된 Basic Block의 Instruction 주소 보정
							// Delta =  합치려는 Basic Block의 start - 병합될 Basic Block의 start
							int Delta = nodeIt->second.end + biiEnd.size - graph.nodes[target].start;
							for(auto instIt = instVect.begin(); instIt != instVect.end(); instIt++)
							{
								if (instIt->addr < nodeIt->second.start || instIt->addr > nodeIt->second.end)
								{
									BASIC_INSTRUCTION_INFO iterBii;
									DbgDisasmFastAt(graph.nodes[target].end, &iterBii);
									//_plugin_logprintf("\n");
									//addr
									instIt->addr += Delta;
								}
							}
							*/

							if (originalInstVectorSize == 1)
							{
								instVect.front().data[0] = 0x90;
							}
							node1.end += toMergeBlockSize - removedInstSize;


							node1.brtrue = graph.nodes[target].brtrue;
							node1.brfalse = graph.nodes[target].brfalse;

							toRemoveExits.push_back(target);
							//nodeIt = graph.nodes.erase(nodeIt);
						}
					}
				}
			}
			isInExit = false;
		}

		for (auto itVect : toRemoveExits)
		{
			auto& targetNodes = graph.nodes;
			targetNodes.erase(itVect);
		}
		toRemoveExits.clear();
	}

	if (graph.nodes.size() == 1)
	{
		//_plugin_logprintf("Opttimized to 1 Block\n");


		DWORD beforeCnt = 0;
		DWORD afterCnt = 0;
		for (auto& nodeIt = graph.nodes.begin(); nodeIt != graph.nodes.end(); nodeIt++)
		{
			for (auto& instIter = nodeIt->second.instrs.begin(); instIter != nodeIt->second.instrs.end(); instIter++)
			{
				beforeCnt++;
			}
		}

		ZydisDecodedInstruction DecodedInst;
		ZydisDecodedOperand DecodedOperand[5];
		multimap<DWORD, vector<StaticIR*>> IRList;

		for (auto nodeIt = graph.nodes.begin(); nodeIt != graph.nodes.end(); nodeIt++)
		{
			//_plugin_logprintf("----------------------------------------\n");
			for (auto instIt : nodeIt->second.instrs)
			{
				ZydisDecoderDecodeFull(&Decoder, instIt.data, 15, &DecodedInst, DecodedOperand,
					ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);
				//_plugin_logprintf("[%p] %s\n", instIt.addr, ZydisMnemonicGetString(DecodedInst.mnemonic));
				CreateStaticIR(&DecodedInst, DecodedOperand, instIt.addr, IRList);
			}
			//_plugin_logprintf("----------------------------------------\n");
		}

		// Optimize


		for (int i = 0; i < IRList.size() * 20; i++)
		{
			for (auto& irIter = IRList.begin(); irIter != IRList.end(); irIter++)
			{
				//std::vector<StaticIR*>::iterator instIter = irIter.second.begin();
				for (auto& instIter = irIter->second.begin(); instIter != irIter->second.end();)
				{
					StaticIR* temIrPtr = dynamic_cast<StaticIR*>(*instIter);
					if (temIrPtr->UseList.size() == 0 &&
						temIrPtr->isHiddenRHS != true &&
						(temIrPtr != RegValue2[REG_EAX_16H].back() && temIrPtr != RegValue2[REG_EAX_8H].back() && temIrPtr != RegValue2[REG_EAX_8L].back() &&
							temIrPtr != RegValue2[REG_EBX_16H].back() && temIrPtr != RegValue2[REG_EBX_8H].back() && temIrPtr != RegValue2[REG_EBX_8L].back() &&
							temIrPtr != RegValue2[REG_ECX_16H].back() && temIrPtr != RegValue2[REG_ECX_8H].back() && temIrPtr != RegValue2[REG_ECX_8L].back() &&
							temIrPtr != RegValue2[REG_EDX_16H].back() && temIrPtr != RegValue2[REG_EDX_8H].back() && temIrPtr != RegValue2[REG_EDX_8L].back() &&
							temIrPtr != RegValue2[REG_EDI_16H].back() && temIrPtr != RegValue2[REG_EDI_8H].back() && temIrPtr != RegValue2[REG_EDI_8L].back() &&
							temIrPtr != RegValue2[REG_ESI_16H].back() && temIrPtr != RegValue2[REG_ESI_8H].back() && temIrPtr != RegValue2[REG_ESI_8L].back() &&
							temIrPtr != RegValue2[REG_EBP_16H].back() && temIrPtr != RegValue2[REG_EBP_8H].back() && temIrPtr != RegValue2[REG_EBP_8L].back() &&
							temIrPtr != RegValue2[REG_ESP_16H].back() && temIrPtr != RegValue2[REG_ESP_8H].back() && temIrPtr != RegValue2[REG_ESP_8L].back()))
					{
						// 제거하려는 Value를 UseList로 가지는 오퍼랜드가 존재하면 UseList에서 제거
						for (int i = 0; i < temIrPtr->Operands.size(); i++)
						{
							// 제거할 IR의 오퍼랜드가 Use 하는 Value에 대해서 해당 Value의 UseList에서 제거할 IR의 오퍼랜드를 제거	
							std::set<StaticOperand*>::iterator iter = temIrPtr->Operands[i]->valuePtr->UseList.find(temIrPtr->Operands[i]);
							if (iter != temIrPtr->Operands[i]->valuePtr->UseList.end())
							{
								temIrPtr->Operands[i]->valuePtr->UseList.erase(temIrPtr->Operands[i]);
							}
						}

						instIter = irIter->second.erase(instIter);
						continue;

					}
					//else
					{
						++instIter;
					}
				}
			}
		}

		set<duint> toRemoveInst;;


		for (auto& irIter = IRList.begin(); irIter != IRList.end(); irIter++)
		{
			//std::vector<StaticIR*>::iterator instIter = irIter.second.begin();
			//for (auto instIter = irIter->second.begin(); instIter != irIter->second.end(); instIter++)
			if (irIter->second.size() > 0)
			{

			}
			else
			{
				toRemoveInst.insert(irIter->first);
			}
		}

		for (auto& nodeIt = graph.nodes.begin(); nodeIt != graph.nodes.end(); nodeIt++)
		{
			for (auto& instIter = nodeIt->second.instrs.begin(); instIter != nodeIt->second.instrs.end();)
			{
				if (toRemoveInst.find(instIter->addr) != toRemoveInst.end() && IRList.find(instIter->addr)!=IRList.end())
				{
					//_plugin_logprintf("Remove Inst %p\n", instIter->addr);
					if (instIter->addr != nodeIt->first)
					{
						//_plugin_logprintf("Remove Inst %p\n", instIter->addr);
						//instIter = nodeIt->second.instrs.erase(instIter);
						BASIC_INSTRUCTION_INFO tes;
						DbgDisasmFastAt(instIter->addr, &tes);
						DbgMemWrite(instIter->addr, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", tes.size);
						instIter->data[0] = 0x90;
						//++instIter;
						//continue;
					}
					else
					{
						BASIC_INSTRUCTION_INFO tes;
						DbgDisasmFastAt(instIter->addr, &tes);
						DbgMemWrite(instIter->addr, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", tes.size);
						instIter->data[0] = 0x90;
					}
				}
				++instIter;
			}
		}

		for (auto& nodeIt = graph.nodes.begin(); nodeIt != graph.nodes.end(); nodeIt++)
		{
			for (auto& instIter = nodeIt->second.instrs.begin(); instIter != nodeIt->second.instrs.end(); instIter++)
			{
				afterCnt++;
			}
		}

		//_plugin_logprintf("Before :%d, After %d\n", beforeCnt, afterCnt);
	}


	info->graph = graph.ToGraphList();
}

bool isGenerateCFG = false;
PLUG_EXPORT void CBTRACEEXECUTE(CBTYPE cbType, PLUG_CB_TRACEEXECUTE* info)
{
	REGDUMP regdump;
	DbgGetRegDumpEx(&regdump, sizeof(REGDUMP));
	ZydisDecodedInstruction DecodedInst;
	ZydisDecodedOperand DecodedOperand[5];
	BYTE buf[64];
	DWORD stackData;
	DWORD memOperandAddr;

	//_plugin_logprintf("[%p] test\n", info->cip);
	/*
	if (info->cip < StartAddress || info->cip > EndAddress)
	{
		REGDUMP regdump;
		DbgGetRegDumpEx(&regdump, sizeof(REGDUMP));
		//info->stop = true;
		duint ret = Script::Memory::ReadDword(regdump.regcontext.csp);

		if (ret < StartAddress || ret > EndAddress)
		{
			_plugin_logprintf("External Return Address :%p\n", ret);
			ret = Script::Memory::ReadDword(regdump.regcontext.cbp + 4);
			_plugin_logprintf("External Return Address2 :%p\n", ret);
			DbgCmdExec("StopRunTrace");
			Script::Debug::SetBreakpoint(ret);
			DbgCmdExec("erun");
			return;
		}

		_plugin_logprintf("External Module Return Address :%p\n", ret);
		Script::Debug::SetBreakpoint(ret);
		DbgCmdExec("erun");
		return;
	}
	*/

	if (isGenerateCFG == true)
	{
		BridgeCFGraphList bcl;
		DbgAnalyzeFunction(info->cip, &bcl);
	}

	DbgMemRead(info->cip, buf, 15);

	ZydisDecoderDecodeFull(&Decoder, buf, 15, &DecodedInst, DecodedOperand,
		ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);

	cntd = regdump.regcontext.cip;
	CreateIR(&DecodedInst, DecodedOperand, regdump, info->cip);

	if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_RET ||
		((DecodedInst.mnemonic == ZYDIS_MNEMONIC_JMP) && (DecodedOperand[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)))
	{
		isGenerateCFG = true;
	}
	else
		isGenerateCFG = false;

	//_plugin_logprintf("Create IR :%p\n", info->cip);
	//cntd++;
}

PLUG_EXPORT void CBBREAKPOINT(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
	_plugin_logprintf("CBBREAKPOINT\n");
	if (IRList.size() == 0)
	{
		_plugin_logprintf("IRList.size() == 0 \n");
		return;
	}

	_plugin_logprintf("IRList.size() %d \n", IRList.size());

	//DeadStoreElimination(IRList);

	_plugin_logprintf("After DSE IRList.size() %d \n", IRList.size());

	for (auto it1 : IRList)
	{
		//_plugin_logprintf("--------------------------------\n");
		for (auto it : it1.second)
		{
			if(it->isTainted)
			{
				_plugin_logprintf("--------------------------------\n");
				if (it == nullptr)
				{
					_plugin_logprintf("[%p] it is null\n", it1.first);
				}

				else
				{
					_plugin_logprintf("[%p]", it1.first);
					printIR(it);

					if (it->isTainted)
					{
						//_plugin_logprintf("[%p]Tainted\n", it1.first);
						taintList.insert(it1.first);
					}

					if (it->z3ExprPtr)
					{
						auto sim = it->z3ExprPtr->simplify();
						//_plugin_logprintf("[%p] z3: %s\n", it1.first, it->z3ExprPtr->to_string().c_str());
						_plugin_logprintf("[%p] z3 sim: %s\n", it1.first, sim.to_string().c_str());
					}
				}
				_plugin_logprintf("--------------------------------\n");
			}
		}
		//_plugin_logprintf("--------------------------------\n");
	}

	/*
	for (auto it1 : IRList)
	{
		_plugin_logprintf("--------------------------------\n");
		for (auto it : it1.second)
		{
			if(it->UseList.size()==0)
			{
				_plugin_logprintf("[%p] Dead Store\n", it1.first);
				printIR(it);
			}
		}
		_plugin_logprintf("--------------------------------\n");
	}
	*/
}

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE ctype, PLUG_CB_CREATEPROCESS* info)
{
	Script::Module::ModuleInfo modInfo;
	Script::Module::GetMainModuleInfo(&modInfo);
	StartAddress = modInfo.base;
	EndAddress = StartAddress + modInfo.size;

	ZydisDecodedInstruction DecodedInst;
	ZydisDecodedOperand DecodedOperand[5];

	/*
	BridgeCFGraphList gp;
	DbgAnalyzeFunction(StartAddress + 0x2AA9, &gp);

	auto cfg = BridgeCFGraph(&gp, true);
	for(auto it : cfg.nodes)
	{
		for (auto instIt : it.second.instrs)
		{
			ZydisDecoderDecodeFull(&Decoder, instIt.data, 15, &DecodedInst, DecodedOperand,
				ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);
			_plugin_logprintf("[%p] %s\n", instIt.addr, ZydisMnemonicGetString(DecodedInst.mnemonic));
		}
	}
	cfg.nodes.begin()->second.instrs[0].data;
	//GuiLoadGraph(&gp, gp.entryPoint);
	//GuiUpdateGraphView();

	_plugin_logprintf("CFG Start :%p\n", cfg.entryPoint);
	*/

	//initReg();
	z3Context = new z3::context;

	initStaticIR();

	Value* testValue1 = new Value("MEM1", 0);
	testValue1->ast = std::make_shared<BTreeNode>(MakeBTreeNode(testValue1->Name));
	testValue1->ast->m_NodeType = NT_SYMVAR;
	testValue1->Size = 8;
	testValue1->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM1", 8));
	MemValue[0x00C25018].push_back(testValue1);

	Value* testValue2 = new Value("MEM2", 0);
	testValue2->ast = std::make_shared<BTreeNode>(MakeBTreeNode(testValue2->Name));
	testValue2->ast->m_NodeType = NT_SYMVAR;
	testValue2->Size = 8;
	testValue2->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM2", 8));
	MemValue[0x00C25019].push_back(testValue2);

	Value* testValue3 = new Value("MEM3", 0);
	testValue3->ast = std::make_shared<BTreeNode>(MakeBTreeNode(testValue3->Name));
	testValue3->ast->m_NodeType = NT_SYMVAR;
	testValue3->Size = 8;
	testValue3->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM3", 8));
	MemValue[0x00C2501a].push_back(testValue3);

	Value* testValue4 = new Value("MEM4", 0);
	testValue4->ast = std::make_shared<BTreeNode>(MakeBTreeNode(testValue4->Name));
	testValue4->ast->m_NodeType = NT_SYMVAR;
	testValue4->Size = 8;
	testValue4->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM4", 8));
	MemValue[0x00C2501b].push_back(testValue4);

	Value* tstValue1 = new Value("MEM1", 0);
	tstValue1->ast = std::make_shared<BTreeNode>(MakeBTreeNode(tstValue1->Name));
	tstValue1->ast->m_NodeType = NT_SYMVAR;
	tstValue1->Size = 8;
	tstValue1->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM5", 8));
	MemValue[0x00C25030].push_back(tstValue1);

	Value* tstValue2 = new Value("MEM2", 0);
	tstValue2->ast = std::make_shared<BTreeNode>(MakeBTreeNode(tstValue2->Name));
	tstValue2->ast->m_NodeType = NT_SYMVAR;
	tstValue2->Size = 8;
	tstValue2->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM6", 8));
	MemValue[0x00C25031].push_back(tstValue2);

	Value* tstValue3 = new Value("MEM3", 0);
	tstValue3->ast = std::make_shared<BTreeNode>(MakeBTreeNode(tstValue3->Name));
	tstValue3->ast->m_NodeType = NT_SYMVAR;
	tstValue3->Size = 8;
	tstValue3->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM7", 8));
	MemValue[0x00C25032].push_back(tstValue3);

	Value* tstValue4 = new Value("MEM4", 0);
	tstValue4->ast = std::make_shared<BTreeNode>(MakeBTreeNode(tstValue4->Name));
	tstValue4->ast->m_NodeType = NT_SYMVAR;
	tstValue4->Size = 8;
	tstValue4->z3ExprPtr = new z3::expr(z3Context->bv_const("MEM8", 8));
	MemValue[0x00C2533].push_back(tstValue4);


	_plugin_logprintf("initReg() start\n");
	Value* eaxValue = new Value("EAX", 0);
	eaxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(eaxValue->Name));
	eaxValue->ast->m_NodeType = NT_SYMVAR;
	eaxValue->Size = 32;
	eaxValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_EAX", 32));

	IR* reg16hIR = new IR("EAX16H", RegValue[REG_EAX_16H].size(), IR::OPR::OPR_EXTRACT16H, eaxValue);
	reg16hIR->Size = 16;
	reg16hIR->isTainted = true;
	RegValue[REG_EAX_16H].push_back(reg16hIR);

	IR* regEax8hIR = new IR("EAX8H", RegValue[REG_EAX_8H].size(), IR::OPR::OPR_EXTRACT8H, eaxValue);
	regEax8hIR->Size = 8;
	regEax8hIR->isTainted = true;
	RegValue[REG_EAX_8H].push_back(regEax8hIR);

	IR* regEax8lIR = new IR("EAX8L", RegValue[REG_EAX_8L].size(), IR::OPR::OPR_EXTRACT8L, eaxValue);
	regEax8lIR->Size = 8;
	regEax8lIR->isTainted = true;
	RegValue[REG_EAX_8L].push_back(regEax8lIR);

	/*
	Z3_context ctx = Z3_mk_context(Z3_mk_config());
	auto dkrqnfrns1 = z3::expr(*z3Context, Z3_mk_fpa_fp(*z3Context,
		Z3_mk_numeral(ctx, "0", Z3_mk_bv_sort(*z3Context, 1)),
		Z3_mk_numeral(ctx, "1025", Z3_mk_bv_sort(*z3Context, 11)),
		Z3_mk_numeral(ctx, "3377699720527872", Z3_mk_bv_sort(*z3Context, 52))));


	auto dkrqnfrns2 = z3::expr(*z3Context, Z3_mk_fpa_fp(*z3Context,
		Z3_mk_numeral(ctx, "0", Z3_mk_bv_sort(*z3Context, 1)),
		Z3_mk_numeral(ctx, "120", Z3_mk_bv_sort(*z3Context, 11)),
		Z3_mk_const(ctx, Z3_mk_string_symbol(*z3Context, "FPA_TEST"), Z3_mk_bv_sort(*z3Context, 52))));

	_plugin_logprintf("BV FPA %s\n", z3::expr(*z3Context, Z3_mk_fpa_fp(*z3Context,
		Z3_mk_numeral(ctx, "0", Z3_mk_bv_sort(*z3Context, 1)),
		Z3_mk_numeral(ctx, "120", Z3_mk_bv_sort(*z3Context, 11)),
		Z3_mk_const(ctx, Z3_mk_string_symbol(*z3Context, "FPA_TEST"), Z3_mk_bv_sort(*z3Context, 52)))).to_string().c_str());

	auto dkrqnfrns3 = z3::expr(*z3Context, Z3_mk_fpa_numeral_double(*z3Context, 1.5, z3::sort(*z3Context, Z3_mk_fpa_sort_64(ctx))));

	_plugin_logprintf("_plugin_logprintf %s\n", dkrqnfrns3.to_string().c_str());

	auto ttet = Z3_mk_fpa_add(ctx, Z3_mk_const(ctx, Z3_mk_string_symbol(ctx, "rm"), Z3_mk_fpa_rounding_mode_sort(ctx)), dkrqnfrns1, dkrqnfrns2);
	Z3_mk_fpa_eq(ctx, ttet, dkrqnfrns3);
	*/

	Value* ebxValue = new Value("EBX", 0);
	ebxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ebxValue->Name));
	ebxValue->ast->m_NodeType = NT_SYMVAR;
	ebxValue->Size = 32;
	ebxValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_EBX", 32));

	IR* regEbx16hIR = new IR("EBX16H", RegValue[REG_EBX_16H].size(), IR::OPR::OPR_EXTRACT16H, ebxValue);
	regEbx16hIR->Size = 16;
	regEbx16hIR->isTainted = true;
	RegValue[REG_EBX_16H].push_back(regEbx16hIR);

	IR* regEbx8hIR = new IR("EBX8H", RegValue[REG_EBX_8H].size(), IR::OPR::OPR_EXTRACT8H, ebxValue);
	regEbx8hIR->Size = 8;
	regEbx8hIR->isTainted = true;
	RegValue[REG_EBX_8H].push_back(regEbx8hIR);

	IR* regEbx8lIR = new IR("EBX8L", RegValue[REG_EBX_8L].size(), IR::OPR::OPR_EXTRACT8L, ebxValue);
	regEbx8lIR->Size = 8;
	regEbx8lIR->isTainted = true;
	RegValue[REG_EBX_8L].push_back(regEbx8lIR);

	Value* ecxValue = new Value("ECX", 0);
	ecxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ecxValue->Name));
	ecxValue->ast->m_NodeType = NT_SYMVAR;
	ecxValue->Size = 32;
	ecxValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_ECX", 32));

	IR* regEcx16hIR = new IR("ECX16H", RegValue[REG_ECX_16H].size(), IR::OPR::OPR_EXTRACT16H, ecxValue);
	regEcx16hIR->Size = 16;
	regEcx16hIR->isTainted = true;
	RegValue[REG_ECX_16H].push_back(regEcx16hIR);

	IR* regEcx8hIR = new IR("ECX8H", RegValue[REG_ECX_8H].size(), IR::OPR::OPR_EXTRACT8H, ecxValue);
	regEcx8hIR->Size = 8;
	regEcx8hIR->isTainted = true;
	RegValue[REG_ECX_8H].push_back(regEcx8hIR);

	IR* regEcx8lIR = new IR("ECX8L", RegValue[REG_ECX_8L].size(), IR::OPR::OPR_EXTRACT8L, ecxValue);
	regEcx8lIR->Size = 8;
	regEcx8lIR->isTainted = true;
	RegValue[REG_ECX_8L].push_back(regEcx8lIR);

	Value* edxValue = new Value("EDX", 0);
	edxValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(edxValue->Name));
	edxValue->ast->m_NodeType = NT_SYMVAR;
	edxValue->Size = 32;
	edxValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_EDX", 32));

	IR* regEdx16hIR = new IR("EDX6H", RegValue[REG_EDX_16H].size(), IR::OPR::OPR_EXTRACT16H, edxValue);
	regEdx16hIR->Size = 16;
	regEdx16hIR->isTainted = true;
	RegValue[REG_EDX_16H].push_back(regEdx16hIR);

	IR* regEdx8hIR = new IR("EDX8H", RegValue[REG_EDX_8H].size(), IR::OPR::OPR_EXTRACT8H, edxValue);
	regEdx8hIR->Size = 8;
	regEdx8hIR->isTainted = true;
	RegValue[REG_EDX_8H].push_back(regEdx8hIR);

	IR* regEdx8lIR = new IR("EDX8L", RegValue[REG_EDX_8L].size(), IR::OPR::OPR_EXTRACT8L, edxValue);
	regEdx8lIR->Size = 8;
	regEdx8lIR->isTainted = true;
	RegValue[REG_EDX_8L].push_back(regEdx8lIR);

	Value* esiValue = new Value("ESI", 0);
	esiValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(esiValue->Name));
	esiValue->ast->m_NodeType = NT_SYMVAR;
	esiValue->Size = 32;
	esiValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_ESI", 32));

	IR* regEsi16hIR = new IR("ESI16H", RegValue[REG_ESI_16H].size(), IR::OPR::OPR_EXTRACT16H, esiValue);
	regEsi16hIR->Size = 16;
	regEsi16hIR->isTainted = true;
	RegValue[REG_ESI_16H].push_back(regEsi16hIR);

	IR* regEsi8hIR = new IR("ESI8H", RegValue[REG_ESI_8H].size(), IR::OPR::OPR_EXTRACT8H, esiValue);
	regEsi8hIR->Size = 8;
	regEsi8hIR->isTainted = true;
	RegValue[REG_ESI_8H].push_back(regEsi8hIR);

	IR* regEsi8lIR = new IR("ESI8L", RegValue[REG_ESI_8L].size(), IR::OPR::OPR_EXTRACT8L, esiValue);
	regEsi8lIR->Size = 8;
	regEsi8lIR->isTainted = true;
	RegValue[REG_ESI_8L].push_back(regEsi8lIR);

	Value* ediValue = new Value("EDI", 0);
	ediValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(ediValue->Name));
	ediValue->ast->m_NodeType = NT_SYMVAR;
	ediValue->Size = 32;
	ediValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_EDI", 32));

	IR* regEdi16hIR = new IR("EDI16H", RegValue[REG_EDI_16H].size(), IR::OPR::OPR_EXTRACT16H, ediValue);
	regEdi16hIR->Size = 16;
	regEdi16hIR->isTainted = true;
	RegValue[REG_EDI_16H].push_back(regEdi16hIR);

	IR* regEdi8hIR = new IR("EDI8H", RegValue[REG_EDI_8H].size(), IR::OPR::OPR_EXTRACT8H, ediValue);
	regEdi8hIR->Size = 8;
	regEdi8hIR->isTainted = true;
	RegValue[REG_EDI_8H].push_back(regEdi8hIR);

	IR* regEdi8lIR = new IR("EDI8L", RegValue[REG_EDI_8L].size(), IR::OPR::OPR_EXTRACT8L, ediValue);
	regEdi8lIR->Size = 8;
	regEdi8lIR->isTainted = true;
	RegValue[REG_EDI_8L].push_back(regEdi8lIR);

	Value* espValue = new Value("ESP", 0);
	espValue->ast = std::make_shared<BTreeNode>(MakeBTreeNode(espValue->Name));
	espValue->ast->m_NodeType = NT_SYMVAR;
	espValue->Size = 32;
	espValue->z3ExprPtr = new z3::expr(z3Context->bv_const("REG_ESP", 32));
	//z3Equation = new z3::expr(*z3esp);

	IR* regEsp16hIR = new IR("ESP16H", RegValue[REG_EBP_16H].size(), IR::OPR::OPR_EXTRACT16H, espValue);
	regEsp16hIR->Size = 16;
	RegValue[REG_EBP_16H].push_back(regEsp16hIR);

	IR* regEsp8hIR = new IR("ESP8H", RegValue[REG_EBP_8H].size(), IR::OPR::OPR_EXTRACT8H, espValue);
	regEsp8hIR->Size = 8;
	RegValue[REG_EBP_8H].push_back(regEsp8hIR);

	IR* regEsp8lIR = new IR("ESP8L", RegValue[REG_EBP_8L].size(), IR::OPR::OPR_EXTRACT8L, espValue);
	regEsp8lIR->Size = 8;
	RegValue[REG_EBP_8L].push_back(regEsp8lIR);

	StaticValue* eaxValue2 = new StaticValue("EAX", 0);
	eaxValue2->Size = 32;
	eaxValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_EAX", 32));

	StaticIR* reg16hIR2 = new StaticIR("EAX16H", RegValue2[REG_EAX_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, eaxValue2);
	reg16hIR2->Size = 16;
	reg16hIR2->isTainted = true;
	RegValue2[REG_EAX_16H].push_back(reg16hIR2);

	StaticIR* regEax8hIR2 = new StaticIR("EAX8H", RegValue2[REG_EAX_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, eaxValue2);
	regEax8hIR2->Size = 8;
	regEax8hIR2->isTainted = true;
	RegValue2[REG_EAX_8H].push_back(regEax8hIR2);

	StaticIR* regEax8lIR2 = new StaticIR("EAX8L", RegValue2[REG_EAX_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, eaxValue2);
	regEax8lIR2->Size = 8;
	regEax8lIR2->isTainted = true;
	RegValue2[REG_EAX_8L].push_back(regEax8lIR2);

	StaticValue* ebxValue2 = new StaticValue("EBX", 0);
	ebxValue2->Size = 32;
	ebxValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_EBX", 32));

	StaticIR* regEbx16hIR2 = new StaticIR("EBX16H", RegValue2[REG_EBX_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, ebxValue2);
	regEbx16hIR2->Size = 16;
	regEbx16hIR2->isTainted = true;
	RegValue2[REG_EBX_16H].push_back(regEbx16hIR2);

	StaticIR* regEbx8hIR2 = new StaticIR("EBX8H", RegValue2[REG_EBX_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, ebxValue2);
	regEbx8hIR2->Size = 8;
	regEbx8hIR2->isTainted = true;
	RegValue2[REG_EBX_8H].push_back(regEbx8hIR2);

	StaticIR* regEbx8lIR2 = new StaticIR("EBX8L", RegValue2[REG_EBX_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, ebxValue2);
	regEbx8lIR2->Size = 8;
	regEbx8lIR2->isTainted = true;
	RegValue2[REG_EBX_8L].push_back(regEbx8lIR2);

	StaticValue* ecxValue2 = new StaticValue("ECX", 0);
	ecxValue2->Size = 32;
	ecxValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_ECX", 32));

	StaticIR* regEcx16hIR2 = new StaticIR("ECX16H", RegValue2[REG_ECX_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, ecxValue2);
	regEcx16hIR2->Size = 16;
	regEcx16hIR2->isTainted = true;
	RegValue2[REG_ECX_16H].push_back(regEcx16hIR2);

	StaticIR* regEcx8hIR2 = new StaticIR("ECX8H", RegValue2[REG_ECX_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, ecxValue2);
	regEcx8hIR2->Size = 8;
	regEcx8hIR2->isTainted = true;
	RegValue2[REG_ECX_8H].push_back(regEcx8hIR2);

	StaticIR* regEcx8lIR2 = new StaticIR("ECX8L", RegValue2[REG_ECX_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, ecxValue2);
	regEcx8lIR2->Size = 8;
	regEcx8lIR2->isTainted = true;
	RegValue2[REG_ECX_8L].push_back(regEcx8lIR2);

	StaticValue* edxValue2 = new StaticValue("EDX", 0);
	edxValue2->Size = 32;
	edxValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_EDX", 32));

	StaticIR* regEdx16hIR2 = new StaticIR("EDX16H", RegValue2[REG_EDX_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, edxValue2);
	regEdx16hIR2->Size = 16;
	regEdx16hIR2->isTainted = true;
	RegValue2[REG_EDX_16H].push_back(regEdx16hIR2);

	StaticIR* regEdx8hIR2 = new StaticIR("EDX8H", RegValue2[REG_EDX_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, edxValue2);
	regEdx8hIR2->Size = 8;
	regEdx8hIR2->isTainted = true;
	RegValue2[REG_EDX_8H].push_back(regEdx8hIR2);

	StaticIR* regEdx8lIR2 = new StaticIR("EDX8L", RegValue2[REG_EDX_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, edxValue2);
	regEdx8lIR2->Size = 8;
	regEdx8lIR2->isTainted = true;
	RegValue2[REG_EDX_8L].push_back(regEdx8lIR2);

	StaticValue* esiValue2 = new StaticValue("ESI", 0);
	esiValue2->Size = 32;
	esiValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_ESI", 32));

	StaticIR* regEsi16hIR2 = new StaticIR("ESI16H", RegValue2[REG_ESI_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, esiValue2);
	regEsi16hIR2->Size = 16;
	regEsi16hIR2->isTainted = true;
	RegValue2[REG_ESI_16H].push_back(regEsi16hIR2);

	StaticIR* regEsi8hIR2 = new StaticIR("ESI8H", RegValue2[REG_ESI_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, esiValue2);
	regEsi8hIR2->Size = 8;
	regEsi8hIR2->isTainted = true;
	RegValue2[REG_ESI_8H].push_back(regEsi8hIR2);

	StaticIR* regEsi8lIR2 = new StaticIR("ESI8L", RegValue2[REG_ESI_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, esiValue2);
	regEsi8lIR2->Size = 8;
	regEsi8lIR2->isTainted = true;
	RegValue2[REG_ESI_8L].push_back(regEsi8lIR2);

	StaticValue* ediValue2 = new StaticValue("EDI", 0);
	ediValue2->Size = 32;
	ediValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_EDI", 32));

	StaticIR* regEdi16hIR2 = new StaticIR("EDI16H", RegValue2[REG_EDI_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, ediValue2);
	regEdi16hIR2->Size = 16;
	regEdi16hIR2->isTainted = true;
	RegValue2[REG_EDI_16H].push_back(regEdi16hIR2);

	StaticIR* regEdi8hIR2 = new StaticIR("EDI8H", RegValue2[REG_EDI_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, ediValue2);
	regEdi8hIR2->Size = 8;
	regEdi8hIR2->isTainted = true;
	RegValue2[REG_EDI_8H].push_back(regEdi8hIR2);

	StaticIR* regEdi8lIR2 = new StaticIR("EDI8L", RegValue2[REG_EDI_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, ediValue2);
	regEdi8lIR2->Size = 8;
	regEdi8lIR2->isTainted = true;
	RegValue2[REG_EDI_8L].push_back(regEdi8lIR2);

	StaticValue* ebpValue2 = new StaticValue("EBP", 0);
	ebpValue2->Size = 32;
	ebpValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_EBP", 32));
	//z3Equation = new z3::expr(*z3esp);

	StaticIR* regEbp16hIR2 = new StaticIR("EBP16H", RegValue2[REG_EBP_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, ebpValue2);
	regEbp16hIR2->Size = 16;
	RegValue2[REG_EBP_16H].push_back(regEbp16hIR2);

	StaticIR* regEbp8hIR2 = new StaticIR("EBP8H", RegValue2[REG_EBP_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, ebpValue2);
	regEbp8hIR2->Size = 8;
	RegValue2[REG_EBP_8H].push_back(regEbp8hIR2);

	StaticIR* regEbp8lIR2 = new StaticIR("EBP8L", RegValue2[REG_EBP_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, ebpValue2);
	regEbp8lIR2->Size = 8;
	RegValue2[REG_EBP_8L].push_back(regEbp8lIR2);

	StaticValue* espValue2 = new StaticValue("ESP", 0);
	espValue2->Size = 32;
	espValue2->z3ExprPtr = new z3::expr(z3Context2->bv_const("REG_ESP", 32));
	//z3Equation = new z3::expr(*z3esp);

	StaticIR* regEsp16hIR2 = new StaticIR("ESP16H", RegValue2[REG_ESP_16H].size(), StaticIR::OPR::OPR_EXTRACT16H, espValue2);
	regEsp16hIR2->Size = 16;
	RegValue2[REG_ESP_16H].push_back(regEsp16hIR2);

	StaticIR* regEsp8hIR2 = new StaticIR("ESP8H", RegValue2[REG_ESP_8H].size(), StaticIR::OPR::OPR_EXTRACT8H, espValue2);
	regEsp8hIR2->Size = 8;
	RegValue2[REG_ESP_8H].push_back(regEsp8hIR2);

	StaticIR* regEsp8lIR2 = new StaticIR("ESP8L", RegValue2[REG_ESP_8L].size(), StaticIR::OPR::OPR_EXTRACT8L, espValue2);
	regEsp8lIR2->Size = 8;
	RegValue2[REG_ESP_8L].push_back(regEsp8lIR2);

	_plugin_logprintf("%s StartAddress:%p, EndAddress:%p\n", modInfo.name, StartAddress, EndAddress);
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE, PLUG_CB_MENUENTRY* info)
{
	switch (info->hEntry)
	{
	case MENU_ENABLED:
	{
		regstepEnabled = !regstepEnabled;
		BridgeSettingSetUint("regstep", "Enabled", regstepEnabled);
	}
	break;
	case SET_MEM_SYMBOL:
	{
		SELECTIONDATA sel;
		GuiSelectionGet(GUI_DUMP, &sel);
		_plugin_logprintf("Memory Start %p End %p\n", sel.start, sel.end);

	}
	break;
	}
}
//Initialize your plugin data here.
bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
	duint setting = regstepEnabled;
	BridgeSettingGetUint("regstep", "Enabled", &setting);
	regstepEnabled = !!setting;

	ZydisDecoderInit(&Decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);
	return true; //Return false to cancel loading the plugin.
}

//Deinitialize your plugin data here.
void pluginStop()
{
}

//Do GUI/Menu related things here.
void pluginSetup()
{
	_plugin_menuaddentry(hMenu, MENU_ENABLED, "Enabled");
	_plugin_menuentrysetchecked(pluginHandle, MENU_ENABLED, regstepEnabled);
	_plugin_menuaddentry(hMenuDump, SET_MEM_SYMBOL, "&Set Memory Symbol");
}
