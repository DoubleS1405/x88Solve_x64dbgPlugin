#include "plugin.h"
#include <Zycore/LibC.h>
#include <Zydis/Zydis.h>
#include <list>
#include <set>
#include <map>
#include "Value.h"

using namespace Script;
using namespace std;

static int traceCount = 0;
bool isFirst = true;

map<string, z3::expr*> symbolExprMap;
vector<Value*> StatusFlagsValue[5];
vector<Value*> RegValue[ZYDIS_REGISTER_MAX_VALUE];
map<DWORD, vector<Value*>> MemValue;
map<DWORD, vector<IR*>> IRList;

z3::context* z3Context;
z3::expr* z3Equation;

enum
{
	MENU_ENABLED,
};

std::set<ZydisRegister> regsTainted;
vector<duint> testArr[ZYDIS_REGISTER_MAX_VALUE];
std::set<duint> addressTainted;
map<duint, vector<duint>> MemDefMap;


static bool regstepEnabled = true;
ZydisDecoder Decoder;
duint StartAddress;
duint EndAddress;

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

PLUG_EXPORT void CBTRACEEXECUTE(CBTYPE cbType, PLUG_CB_TRACEEXECUTE* info)
{
	REGDUMP regdump;
	DbgGetRegDumpEx(&regdump, sizeof(REGDUMP));
	ZydisDecodedInstruction DecodedInst;
	ZydisDecodedOperand DecodedOperand[5];
	BYTE buf[64];
	DWORD stackData;
	DWORD memOperandAddr;
	_plugin_logprintf("[%p] test\n", info->cip);

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

	DbgMemRead(info->cip, buf, 15);

	ZydisDecoderDecodeFull(&Decoder, buf, 15, &DecodedInst, DecodedOperand,
		ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);

	CreateIR(&DecodedInst, DecodedOperand, regdump, regdump.regcontext.cip);

	if (IRList.find(regdump.regcontext.cip) != IRList.end())
	{
		_plugin_logprintf("--------------------------------\n");
		for (auto it : IRList[regdump.regcontext.cip])
		{
			_plugin_logprintf("[%p]", regdump.regcontext.cip);
			printIR(it);
		}
		_plugin_logprintf("--------------------------------\n");
	}

}

//PLUG_EXPORT void CBTRACEEXECUTE(CBTYPE cbType, PLUG_CB_TRACEEXECUTE* info)
//{
//	REGDUMP regdump;
//	DbgGetRegDump(&regdump);
//	ZydisDecodedInstruction DecodedInst;
//	ZydisDecodedOperand DecodedOperand[5];
//	BYTE buf[64];
//	DWORD stackData;
//
//	
//	if (info->cip < StartAddress || info->cip > EndAddress)
//	{
//		REGDUMP regdump;
//		DbgGetRegDump(&regdump);
//		//info->stop = true;
//		duint ret = Script::Memory::ReadDword(regdump.regcontext.csp);
//		if (ret < StartAddress || ret > EndAddress)
//		{
//			_plugin_logprintf("External Return Address :%p\n", ret);
//			ret= Script::Memory::ReadDword(regdump.regcontext.cbp + 4);
//			_plugin_logprintf("External Return Address2 :%p\n", ret);
//			DbgCmdExec("StopRunTrace");
//			Script::Debug::SetBreakpoint(ret);
//			DbgCmdExec("erun");
//			return;
//		}
//		_plugin_logprintf("External Module Return Address :%p\n", ret);
//		Script::Debug::SetBreakpoint(ret);
//		DbgCmdExec("erun");
//		return;
//	}
//	
//
//	DbgMemRead(info->cip, buf, 15);
//	ZydisDecoderDecodeFull(&Decoder, buf, 15, &DecodedInst, DecodedOperand,
//		ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY);
//
//	if ((isFirst == true) && (info->cip == 0x0047CDEC/*0x48B19E*/ /*0x46EA5F*//*0x44202F*/))
//	{
//		duint target = regdump.regcontext.csi;
//		//_plugin_logprintf("%p Start Taint EAX:%p\n", info->cip, target + 0x1c);
//		_plugin_logprintf("%p Start Taint EAX:%p\n", info->cip, target + 0x4);
//		//_plugin_logprintf("%p Start Taint EDX:%p\n", info->cip, target + 0x20);
//		addressTainted.insert(target + 0x4); // EAX
//		addressTainted.insert(target + 0x14); // EBP
//		addressTainted.insert(target + 0x20); // EDX
//		/*
//		addressTainted.insert(target + 0xc); // ESI
//		addressTainted.insert(target + 0x20); // EDX
//		addressTainted.insert(target + 0x10); // EBX
//		addressTainted.insert(target + 0x14); // ECX
//		addressTainted.insert(target + 8); // E
//		addressTainted.insert(target + 0x24); // EBP
//		addressTainted.insert(target + 0x1c); // EAX
//		*/
//
//		isFirst = false;
//	}
//
//	if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_CALL)
//	{
//		DISASM_INSTR dis;
//		DbgDisasmAt(info->cip, &dis);
//		duint tgt = dis.arg[0].value;
//
//		if (DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_MEMORY)
//		{
//			_plugin_logprintf("Dispather suspicious %p\n", dis.arg[0].memvalue);
//		}
//	}
//
//
//	if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_JMP)
//	{
//		DISASM_INSTR dis;
//		DbgDisasmAt(info->cip, &dis);
//		duint tgt = dis.arg[0].value;
//
//		if (DecodedOperand[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
//		{
//			//_plugin_logprintf("-----------------------------------\n");
//			if (DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
//			{
//				if (info->cip == 0x0047CDEC)
//				{
//
//				}
//				else
//				{
//					//_plugin_logprintf("%p->%p\n", info->cip, tgt);
//				}
//			}
//			else if (DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_MEMORY)
//			{
//				_plugin_logprintf("Dispather suspicious %p -> %p\n", info->cip, dis.arg[0].memvalue);
//			}
//		}
//		//_plugin_logprintf("Dispather suspicious %p\n", info->cip);
//		//_plugin_logprintf("%p\n", info->cip);
//		return;
//	}
//
//	else if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_RET)
//	{
//		DISASM_INSTR dis;
//		DbgDisasmAt(info->cip, &dis);
//		duint tgt = dis.arg[0].value;
//
//		//_plugin_logprintf("-----------------------------------\n");
//		//_plugin_logprintf("%p\n", Script::Memory::ReadDword(regdump.regcontext.csp));
//		return;
//	}
//
//	else if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_PUSH)
//	{
//		stackData = Script::Memory::ReadDword(regdump.regcontext.csp);
//		duint memOp = regdump.regcontext.csp - 4;
//
//		// REG가 Taint된 경우 스택을 무조건 Taint 시킨다.
//		if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER))
//		{
//			ZydisRegister reg_r = DecodedOperand[0].reg.value;
//			if (regsTainted.find(reg_r) != regsTainted.end())
//			{
//				_plugin_logprintf("%p:[%p](%d)=%s(%d)\n", info->cip, memOp, MemDefMap[memOp].size() + 1, ZydisRegisterGetString(reg_r), testArr[reg_r].size());
//				addressTainted.insert(memOp);
//				
//				MemDefMap[memOp].push_back(info->cip);
//				if (DecodedOperand[0].size == 32)
//				{
//					addressTainted.insert(memOp + 1);
//					MemDefMap[memOp + 1].push_back(info->cip);
//					addressTainted.insert(memOp + 2);
//					MemDefMap[memOp + 2].push_back(info->cip);
//					addressTainted.insert(memOp + 3);
//					MemDefMap[memOp + 3].push_back(info->cip);
//				}
//
//				else if (DecodedOperand[0].size == 16)
//				{
//					addressTainted.insert(memOp + 1);
//					MemDefMap[memOp + 1].push_back(info->cip);
//				}
//				return;
//			}
//
//			// 스택이 Taint 되고 REG가 Taint되지 않는 경우
//			else if (regsTainted.find(reg_r) == regsTainted.end())
//			{
//				//if (addressTainted.find(memOp) != addressTainted.end())
//					//addressTainted.erase(memOp);
//				// << hexstr(insAddr) << ":" << "[" << hexstr(memOp) << "]" << "(" << MemDefMap[memOp].size() + 1 << ")" << "=" << REG_StringShort(reg_r) << "(" << testArr[reg_r].size() << ")" << endl;
//				//MemDefMap[memOp].push_back(insAddr);
//			}
//		}
//
//		else if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_MEMORY))
//		{
//			DISASM_INSTR dis;
//			DbgDisasmAt(info->cip, &dis);
//			duint memOp0 = dis.arg[0].value;
//
//			// MEM가 Taint된 경우 스택을 무조건 Taint 시킨다.
//			if (addressTainted.find(memOp0) != addressTainted.end())
//			{
//				//logstr = hexstr(insAddr) + "PUSH " + hexstr(memOp0) + " Stack : " + hexstr(_stackMemOp);
//				_plugin_logprintf("%p:[%p](%d)=[%p](%d)\n", info->cip, memOp, MemDefMap[memOp].size() + 1, memOp0, MemDefMap[memOp0].size());
//				addressTainted.insert(memOp);
//				MemDefMap[memOp].push_back(info->cip);
//				if (DecodedOperand[0].size == 32)
//				{
//					addressTainted.insert(memOp + 1);
//					MemDefMap[memOp + 1].push_back(info->cip);
//					addressTainted.insert(memOp + 2);
//					MemDefMap[memOp + 2].push_back(info->cip);
//					addressTainted.insert(memOp + 3);
//					MemDefMap[memOp + 3].push_back(info->cip);
//				}
//
//				else if (DecodedOperand[0].size == 16)
//				{
//					addressTainted.insert(memOp + 1);
//					MemDefMap[memOp + 1].push_back(info->cip);
//				}
//				return;
//			}
//		}
//	}
//
//	else if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_POP)
//	{
//		stackData = Script::Memory::ReadDword(regdump.regcontext.csp);
//		duint memOp = regdump.regcontext.csp;
//
//		// Stack이 Taint 된 경우 Reg를 Taint 시킨다.
//		if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER))
//		{
//			ZydisRegister reg_r = DecodedOperand[0].reg.value;
//			if (addressTainted.find(memOp) != addressTainted.end())
//			{
//				//logstr = hexstr(insAddr) + "PUSH " + hexstr(memOp0) + " Stack : " + hexstr(_stackMemOp);
//				_plugin_logprintf("%p:%s(%d)=[%p](%d)\n", info->cip, ZydisRegisterGetString(reg_r), testArr[reg_r].size() + 1, memOp, MemDefMap[memOp].size());
//				TainteRegdEx(reg_r, info->cip);
//				return;
//			}
//
//			// 스택이 Taint 되고 REG가 Taint되지 않는 경우
//			else if (regsTainted.find(reg_r) == regsTainted.end())
//			{
//				//if (addressTainted.find(memOp) != addressTainted.end())
//					//addressTainted.erase(memOp);
//				// << hexstr(insAddr) << ":" << "[" << hexstr(memOp) << "]" << "(" << MemDefMap[memOp].size() + 1 << ")" << "=" << REG_StringShort(reg_r) << "(" << testArr[reg_r].size() << ")" << endl;
//				//MemDefMap[memOp].push_back(insAddr);
//			}
//		}
//
//		else if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_MEMORY))
//		{
//			DISASM_INSTR dis;
//			DbgDisasmAt(info->cip, &dis);
//			duint memOp0 = dis.arg[0].value;
//			if (addressTainted.find(memOp) != addressTainted.end())
//			{
//				//logstr = hexstr(insAddr) + "PUSH " + hexstr(memOp0) + " Stack : " + hexstr(_stackMemOp);
//				_plugin_logprintf("%p:[%p](%d)=[%p](%d)\n", info->cip, memOp0, MemDefMap[memOp0].size() + 1, memOp, MemDefMap[memOp].size());
//				addressTainted.insert(memOp0);
//				MemDefMap[memOp].push_back(info->cip);
//				if (DecodedOperand[0].size == 32)
//				{
//					addressTainted.insert(memOp + 1);
//					MemDefMap[memOp + 1].push_back(info->cip);
//					addressTainted.insert(memOp + 2);
//					MemDefMap[memOp + 2].push_back(info->cip);
//					addressTainted.insert(memOp + 3);
//					MemDefMap[memOp + 3].push_back(info->cip);
//				}
//
//				else if (DecodedOperand[0].size == 16)
//				{
//					addressTainted.insert(memOp + 1);
//					MemDefMap[memOp + 1].push_back(info->cip);
//				}
//				return;
//			}
//
//			// 스택이 Taint 되고 REG가 Taint되지 않는 경우
//			else if (addressTainted.find(memOp) == addressTainted.end())
//			{
//				//if (addressTainted.find(memOp) != addressTainted.end())
//					//addressTainted.erase(memOp);
//				// << hexstr(insAddr) << ":" << "[" << hexstr(memOp) << "]" << "(" << MemDefMap[memOp].size() + 1 << ")" << "=" << REG_StringShort(reg_r) << "(" << testArr[reg_r].size() << ")" << endl;
//				//MemDefMap[memOp].push_back(insAddr);
//			}
//		}
//	}
//
//	// LEA는 두 번째 오퍼랜드의 주소값을 계산하여 첫 번째 오퍼랜드에 저장하는 것이다.
//	// 일단 두 번쨰 오퍼랜드를 상수 취급하고 첫 번째 오퍼랜드를 Untaint 시킨다.
//	else if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_LEA)
//	{
//		if (regsTainted.find(DecodedOperand[0].reg.value) != regsTainted.end())
//		{
//			regsTainted.erase(DecodedOperand[0].reg.value);
//			UnTainteRegdEx(DecodedOperand[0].reg.value);
//		}
//		return;
//	}
//
//	else if (DecodedInst.mnemonic == ZYDIS_MNEMONIC_XCHG)
//	{
//		if (DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
//		{
//			if (regsTainted.find(DecodedOperand[0].reg.value) != regsTainted.end())
//			{
//				if (regsTainted.find(DecodedOperand[1].reg.value) != regsTainted.end())
//				{
//					return;
//				}
//
//				else
//				{
//					_plugin_logprintf("xchg1 %p:%s(%d)=%s(%d) %s %s(%d)\n", info->cip, ZydisRegisterGetString(DecodedOperand[1].reg.value), testArr[DecodedOperand[1].reg.value].size() + 1,
//						ZydisRegisterGetString(DecodedOperand[1].reg.value), testArr[DecodedOperand[1].reg.value].size(),
//						ZydisMnemonicGetString(DecodedInst.mnemonic),
//						ZydisRegisterGetString(DecodedOperand[0].reg.value), testArr[DecodedOperand[0].reg.value].size());
//					regsTainted.erase(DecodedOperand[0].reg.value);
//					TainteRegdEx(DecodedOperand[1].reg.value, info->cip);
//					return;
//				}
//			}
//
//			if (regsTainted.find(DecodedOperand[1].reg.value) != regsTainted.end())
//			{
//				if (regsTainted.find(DecodedOperand[0].reg.value) != regsTainted.end())
//				{
//					return;
//				}
//
//				else
//				{
//					_plugin_logprintf("xchg2 %p:%s(%d)=%s(%d) %s %s(%d)\n", info->cip, ZydisRegisterGetString(DecodedOperand[0].reg.value), testArr[DecodedOperand[0].reg.value].size() + 1,
//						ZydisRegisterGetString(DecodedOperand[0].reg.value), testArr[DecodedOperand[0].reg.value].size(),
//						ZydisMnemonicGetString(DecodedInst.mnemonic),
//						ZydisRegisterGetString(DecodedOperand[1].reg.value), testArr[DecodedOperand[1].reg.value].size());					
//					regsTainted.erase(DecodedOperand[1].reg.value);
//					TainteRegdEx(DecodedOperand[0].reg.value, info->cip);
//					return;
//				}
//			}
//		}
//
//		else
//		{
//			MessageBoxA(0, "Test", "XCHG MEM, REG", 0);
//		}
//		return;
//	}
//
//	else
//	{
//		if (DecodedInst.operand_count_visible == 2)
//		{
//			// REG, REG
//			if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER) && (DecodedOperand[1].type == ZYDIS_OPERAND_TYPE_REGISTER))
//			{
//				//  TYPE 1
//				ZydisRegister reg0 = DecodedOperand[0].reg.value;
//				ZydisRegister reg1 = DecodedOperand[1].reg.value;
//
//				if ((DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) && ((DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ) == 0))
//				{
//					if (DecodedOperand[1].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
//					{
//						if (regsTainted.find(reg1) != regsTainted.end())
//						{
//							//regsTainted.insert(reg0);
//							//logstr = hexstr(insAddr) + ":" +  REG_StringShort(reg0) + "_" + hexstr(testArr[reg0].size()+1)  + "=" + REG_StringShort(reg1) + "_" + hexstr(testArr[reg1].size());
//
//							_plugin_logprintf("%p:%s(%d)=%s(%d)\n", info->cip, ZydisRegisterGetString(reg0), testArr[reg0].size() + 1, ZydisRegisterGetString(reg1), testArr[reg1].size());
//							TainteRegdEx(reg0, info->cip);
//							//testArr[reg0].push_back(insAddr);
//							return;
//						}
//
//						// reg0이 Taint 되고, reg1이 Taint되지 않은 경우
//						else if (regsTainted.find(reg1) == regsTainted.end())
//						{
//							//if (regsTainted.find(reg0) != regsTainted.end())
//							regsTainted.erase(reg0);
//							UnTainteRegdEx(reg0);
//						}
//					}
//				}
//
//				//  TYPE 2
//				else if ((DecodedOperand[0].actions & (ZYDIS_OPERAND_ACTION_MASK_WRITE)) && (DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ))
//				{
//					if (DecodedOperand[1].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
//					{
//						//_plugin_logprintf("[Type 2] %p\n", info->cip);
//						if (regsTainted.find(reg1) != regsTainted.end())
//						{
//							//if (regsTainted.find(reg0) != regsTainted.end())
//							{
//								_plugin_logprintf("%p:%s(%d)=%s(%d) %s %s(%d)\n", info->cip, ZydisRegisterGetString(reg0), testArr[reg0].size() + 1,
//									ZydisRegisterGetString(reg0), testArr[reg0].size(),
//									ZydisMnemonicGetString(DecodedInst.mnemonic),
//									ZydisRegisterGetString(reg1), testArr[reg1].size());
//								TainteRegdEx(reg0, info->cip);
//								//testArr[reg0].push_back(insAddr);
//								return;
//							}
//						}
//					}
//				}
//			}
//
//			// REG, MEM
//			else if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER) && (DecodedOperand[1].type == ZYDIS_OPERAND_TYPE_MEMORY))
//			{
//				//  TYPE 1
//				ZydisRegister reg0 = DecodedOperand[0].reg.value;
//				DISASM_INSTR dis;
//				DbgDisasmAt(info->cip, &dis);
//				duint memOp1 = dis.arg[1].value;
//
//				if ((DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) && ((DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ) == 0))
//				{
//					if (DecodedOperand[1].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
//					{
//						//_plugin_logprintf("[Type 1 Reg, Mem] %p\n", info->cip);
//						if (addressTainted.find(memOp1) != addressTainted.end())
//						{
//							//logstr = hexstr(insAddr) + "PUSH " + hexstr(memOp0) + " Stack : " + hexstr(_stackMemOp);
//							//_plugin_logprintf("%p:%s(%d)=[%p](%d)\n", info->cip, ZydisRegisterGetString(reg0), testArr[reg0].size() + 1, memOp1, MemDefMap[memOp1].size());
//							TainteRegdEx(reg0, info->cip);
//
//							if (regsTainted.find(DecodedOperand[1].mem.base) != regsTainted.end())
//							{
//								_plugin_logprintf("%p:[VMREG] Base Reg Tainted 1 (%s)(%d)\n", info->cip, ZydisRegisterGetString(DecodedOperand[1].mem.base), testArr[DecodedOperand[1].mem.base].size());
//								return;
//							}
//
//							return;
//						}
//
//						if (regsTainted.find(DecodedOperand[1].mem.base) != regsTainted.end())
//						{
//							TainteRegdEx(reg0, info->cip);
//							_plugin_logprintf("%p:[VMREG] Base Reg Tainted 2 (%s)(%d)\n", info->cip, ZydisRegisterGetString(DecodedOperand[1].mem.base), testArr[DecodedOperand[1].mem.base].size());
//							return;
//						}
//
//						else
//						{							
//							regsTainted.erase(reg0);
//							UnTainteRegdEx(reg0);
//						}
//					}
//				}
//
//				//  TYPE 2
//				else if ((DecodedOperand[0].actions & (ZYDIS_OPERAND_ACTION_MASK_WRITE)) && (DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ))
//				{
//					if (DecodedOperand[1].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
//					{
//						if (addressTainted.find(memOp1) != addressTainted.end())
//						{
//							if (regsTainted.find(reg0) != regsTainted.end())
//							{
//								_plugin_logprintf("%p:%s(%d)= %s(%d)%s[%p](%d)\n", info->cip, ZydisRegisterGetString(reg0), testArr[reg0].size() + 1,
//									ZydisRegisterGetString(reg0), testArr[reg0].size() + 1, ZydisMnemonicGetString(DecodedInst.mnemonic),
//									memOp1, MemDefMap[memOp1].size());
//								TainteRegdEx(reg0, info->cip);
//							}
//						}
//						//_plugin_logprintf("[Type 2 Reg, Mem] %p\n", info->cip);
//					}
//				}
//			}
//
//			// MEM, REG
//			else if ((DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_MEMORY) && (DecodedOperand[1].type == ZYDIS_OPERAND_TYPE_REGISTER))
//			{
//				//  TYPE 1
//				DISASM_INSTR dis;
//				DbgDisasmAt(info->cip, &dis);		
//				duint memOp = /*GetEA(DecodedOperand[0].mem.base, DecodedOperand[0].mem.index, DecodedOperand[0].mem.scale, DecodedOperand[0].mem.disp.value);*/ dis.arg[0].value;
//				ZydisRegister reg1 = DecodedOperand[1].reg.value;
//				if ((DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) && ((DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ) == 0))
//				{
//					//_plugin_logprintf("[Type 2 Mem, Reg] %p memOp %x\n", info->cip, memOp);
//
//					if (regsTainted.find(reg1) != regsTainted.end())
//					{
//						//_plugin_logprintf("%p:[%p](%d)=%s(%d)\n", info->cip, memOp, MemDefMap[memOp].size() + 1, ZydisRegisterGetString(reg1), testArr[reg1].size());
//
//						// Mem0가 이미 Taint 된 경우
//						//if (addressTainted.find(memOp) != addressTainted.end())
//						{
//							addressTainted.erase(memOp);
//							addressTainted.insert(memOp);
//							MemDefMap[memOp].push_back(info->cip);
//							if (DecodedOperand[0].size == 32)
//							{
//								addressTainted.insert(memOp + 1);
//								MemDefMap[memOp + 1].push_back(info->cip);
//								addressTainted.insert(memOp + 2);
//								MemDefMap[memOp + 2].push_back(info->cip);
//								addressTainted.insert(memOp + 3);
//								MemDefMap[memOp + 3].push_back(info->cip);
//							}
//
//							else if (DecodedOperand[0].size == 16)
//							{
//								addressTainted.insert(memOp + 1);
//								MemDefMap[memOp + 1].push_back(info->cip);
//							}
//							return;
//						}
//					}
//
//					// mem0이 Taint 되고, reg1이 Taint되지 않은 경우
//					else if (regsTainted.find(reg1) == regsTainted.end())
//					{
//						//if (addressTainted.find(memOp) != addressTainted.end())
//						addressTainted.erase(memOp);
//						if (info->cip == 0x0048CDE6)
//						{
//							if (addressTainted.find(memOp) == addressTainted.end())
//							{
//								_plugin_logprintf("%p deleted\n",memOp);
//							}
//							MessageBoxA(0,"Test","0048CDE6",0);
//						}
//						if (DecodedOperand[0].size == 32)
//						{
//							addressTainted.erase(memOp + 1);
//							addressTainted.erase(memOp + 2);
//							addressTainted.erase(memOp + 3);
//						}
//
//						else if (DecodedOperand[0].size == 16)
//						{
//							addressTainted.erase(memOp + 1);
//						}
//
//					}
//
//				}
//
//				//  TYPE 2
//				else if ((DecodedOperand[0].actions & (ZYDIS_OPERAND_ACTION_MASK_WRITE)) && (DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ))
//				{
//					if (DecodedOperand[1].actions & ZYDIS_OPERAND_ACTION_MASK_READ)
//					{
//						//_plugin_logprintf("[Type 2 Mem, Reg] %p\n", info->cip);
//					}
//				}
//			}
//		}
//
//		else if (DecodedInst.operand_count_visible == 1)
//		{
//			if (DecodedOperand[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
//			{
//				ZydisRegister reg0 = DecodedOperand[0].reg.value;
//				if ((DecodedOperand[0].actions & (ZYDIS_OPERAND_ACTION_MASK_WRITE)) && (DecodedOperand[0].actions & ZYDIS_OPERAND_ACTION_MASK_READ))
//				{
//					if (regsTainted.find(reg0) != regsTainted.end())
//					{
//						//if (regsTainted.find(reg0) != regsTainted.end())
//						{
//							_plugin_logprintf("%p:%s(%d)=%s(%d) %s %s(%d)\n", info->cip, ZydisRegisterGetString(reg0), testArr[reg0].size() + 1,
//								ZydisRegisterGetString(reg0), testArr[reg0].size(),
//								ZydisMnemonicGetString(DecodedInst.mnemonic),
//								ZydisRegisterGetString(reg0), testArr[reg0].size());
//							TainteRegdEx(reg0, info->cip);
//							//testArr[reg0].push_back(insAddr);
//							return;
//						}
//					}
//				}
//			}
//		}
//	}
//}


PLUG_EXPORT void CBBREAKPOINT(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
	if (IRList.size() == 0)
		return;

	for (auto it1 : IRList)
	{
		printf("--------------------------------\n");
		for (auto it : it1.second)
		{
			_plugin_logprintf("[%p]", it1.first);
			//InorderTraverse(it->ast);
			_plugin_logprintf("%s", EvaluateExpTree(it->ast).c_str());
			z3::expr z3Expression = GetZ3ExprFromTree(it->ast);

			// 생성된 Z3 AST 출력
			_plugin_logprintf("Z3 AST: %s\n", z3Expression.simplify().to_string().c_str());
			MessageBoxA(0, "test", "test", 0);
			if (it1.first == StartAddress + 0xB1E9)
			{
				z3::solver solver(*z3Context);
				z3Expression = z3Expression == 0x41;
				solver.add(z3Expression);
				z3::check_result result = solver.check();

				if (result == z3::sat) {

					// Get the model
					z3::model model = solver.get_model();

					_plugin_logprintf("Solve %s\n", model.to_string().c_str());
				}
				else if (result == z3::unsat) {
					_plugin_logprintf("unsat\n");
				}
			}
		}
		printf("--------------------------------\n");
	}
	//Script::Debug::DeleteBreakpoint(info->breakpoint->addr);
	//DbgCmdExec("StopRunTrace");
	//DbgCmdExec("TraceIntoIntoTraceRecord 5000000");
}


/*
PLUG_EXPORT void CBSTEPPED(CBTYPE cbType, PLUG_CB_STEPPED* info)
{
	REGDUMP regdump;
	DbgGetRegDump(&regdump);

	auto& r = regdump.regcontext;

	_plugin_logprintf(R"(eax=%p ebx=%p ecx=%p
edx=%p esi=%p edi=%p
eip=%p esp=%p ebp=%p
)",
r.cax, r.cbx, r.ccx,
r.cdx, r.csi, r.cdi,
r.cip, r.csp, r.cbp);
}
*/

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE ctype, PLUG_CB_CREATEPROCESS* info)
{
	Script::Module::ModuleInfo modInfo;
	Script::Module::GetMainModuleInfo(&modInfo);
	StartAddress = modInfo.base;
	EndAddress = StartAddress + modInfo.size;
	//initReg();
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
}
