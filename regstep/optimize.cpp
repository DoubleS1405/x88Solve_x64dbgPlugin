#include "Value.h"
#include <map>

extern vector<Value*> RegValue[ZYDIS_REGISTER_MAX_VALUE];
extern map<DWORD, vector<Value*>> MemValue;

void RemoveIR(IR* _irPtr)
{
	for (int i = 0; i < _irPtr->Operands.size(); i++)
	{
		// 제거할 IR의 오퍼랜드에 대한 Value 인스턴스에서 UseList 제거		
		std::set<OPERAND*>::iterator iter = _irPtr->Operands[i]->valuePtr->UseList.find(_irPtr->Operands[i]);
		if (iter != _irPtr->Operands[i]->valuePtr->UseList.end())
		{
			_irPtr->Operands[i]->valuePtr->UseList.erase(_irPtr->Operands[i]);
		}
	}

	if (_irPtr != nullptr)
	{
		//delete _irPtr;
	}
}

void ConstantFoldingForUnary(IR * _irPtr)
{
	if (dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->opr == IR::OPR::OPR_BVV)
	{
		switch (_irPtr->opr)
		{
		case IR::OPR::OPR_EXTRACT16H:
			_plugin_logprintf("OPR_EXTRACT16H Constant Folding\n");
			break;
		case IR::OPR::OPR_EXTRACT8HH:
			_plugin_logprintf("OPR_EXTRACT8HH Constant Folding\n");
			break;
		case IR::OPR::OPR_EXTRACT8HL:
			_plugin_logprintf("OPR_EXTRACT8HL Constant Folding\n");
			break;
		case IR::OPR::OPR_EXTRACT8H:
			_plugin_logprintf("OPR_EXTRACT8H Constant Folding\n");
			break;
		case IR::OPR::OPR_EXTRACT8L:
			_plugin_logprintf("OPR_EXTRACT8L Constant Folding\n");
			break;
		case IR::OPR::OPR_NOT:
			_plugin_logprintf("OPR_NOT Constant Folding\n");
			break;
		}
	}
}

void ConstantFoldingForBinary(DWORD _offset, IR* _irPtr)
{
	if (dynamic_cast<IR*>(_irPtr->Operands[0]->valuePtr)->opr == IR::OPR::OPR_BVV
		&& dynamic_cast<IR*>(_irPtr->Operands[1]->valuePtr)->opr == IR::OPR::OPR_BVV)
	{
		switch (_irPtr->opr)
		{
		case IR::OPR::OPR_CONCAT:
			_plugin_logprintf("[%p] OPR_CONCAT 2 Constant Folding\n", _offset);
			break;
		case IR::OPR::OPR_ADD:
			_plugin_logprintf("[%p] OPR_ADD Constant Folding\n", _offset);
			break;
		case IR::OPR::OPR_SUB:
			_plugin_logprintf("[%p] OPR_SUB Constant Folding\n", _offset);
			break;
		case IR::OPR::OPR_EXTRACT8H:
			_plugin_logprintf("[%p] OPR_EXTRACT8H Constant Folding\n", _offset);
			break;
		case IR::OPR::OPR_EXTRACT8L:
			_plugin_logprintf("[%p] OPR_EXTRACT8L Constant Folding\n", _offset);
			break;
		case IR::OPR::OPR_NOT:
			_plugin_logprintf("OPR_NOT Constant Folding\n");
			break;
		}
	}
}

void DeadStoreElimination(multimap<DWORD, vector<IR*>>& IRList)
{
	set<DWORD> forTest;
	DWORD cnt = IRList.size();
	{
		for (auto it1 = IRList.begin(); it1 != IRList.end();it1++)
		{
			//auto& irVector = it1->second;  // Reference to the current vector
			DWORD cnt2 = it1->second.size();

			for (DWORD d = 0; d < cnt2; d++)
			{
				for (auto it2 = it1->second.rbegin(); it2 != it1->second.rend();)
				{
					IR* testIR = *it2;

					if (testIR->UseList.size() == 0)
					{
						//RemoveIR(testIR);

						if(testIR != RegValue[REG_EAX_16H].back() && testIR != RegValue[REG_EAX_8H].back() && testIR != RegValue[REG_EAX_8L].back() &&
							testIR != RegValue[REG_EBX_16H].back() && testIR != RegValue[REG_EBX_8H].back() && testIR != RegValue[REG_EBX_8L].back()&&
							testIR != RegValue[REG_ECX_16H].back() && testIR != RegValue[REG_ECX_8H].back() && testIR != RegValue[REG_ECX_8L].back()&&
							testIR != RegValue[REG_EDX_16H].back() && testIR != RegValue[REG_EDX_8H].back() && testIR != RegValue[REG_EDX_8L].back()&&
							testIR != RegValue[REG_EDI_16H].back() && testIR != RegValue[REG_EDI_8H].back() && testIR != RegValue[REG_EDI_8L].back()&&
							testIR != RegValue[REG_ESI_16H].back() && testIR != RegValue[REG_ESI_8H].back() && testIR != RegValue[REG_ESI_8L].back()&&
							testIR != RegValue[REG_EBP_16H].back() && testIR != RegValue[REG_EBP_8H].back() && testIR != RegValue[REG_EBP_8L].back()&&
							testIR != RegValue[REG_ESP_16H].back() && testIR != RegValue[REG_ESP_8H].back() && testIR != RegValue[REG_ESP_8L].back())
						{
							for (int i = 0; i < testIR->Operands.size(); i++)
							{
								// 제거할 IR의 오퍼랜드에 대한 Value 인스턴스에서 UseList 제거		
								std::set<OPERAND*>::iterator iter = testIR->Operands[i]->valuePtr->UseList.find(testIR->Operands[i]);
								if (iter != testIR->Operands[i]->valuePtr->UseList.end())
								{
									testIR->Operands[i]->valuePtr->UseList.erase(testIR->Operands[i]);
									_plugin_logprintf("Dead Store %p\n", it1->first);
								}
							}
						}

						it2 = decltype(it2)(it1->second.erase(std::next(it2).base()));
						//it2 = it1->second.erase(it2);
					}
					else
					{
						++it2;
					}
				}
			}
		}
	}

	for (auto it1 = IRList.rbegin(); it1 != IRList.rend();)
	{
		if (it1->second.size() == 0)
		{
			//_plugin_logprintf("IR Vector is null %p\n", it1->first);
			it1 =  decltype(it1)(IRList.erase(std::next(it1).base()));
			//it1 = IRList.erase(it1);
			forTest.insert(it1->first);
		}
		else
		{
			++it1;
		}
	}
}

//void DeadStoreElimination(multimap<DWORD, vector<IR*>>& IRList)
//{
//	DWORD cnt = IRList.size();
//
//	for (int d = 0; d < cnt; d++)
//	{
//		for (auto it1 = IRList.begin(); it1 != IRList.end();)
//		{
//			auto& irVector = it1->second;  // Reference to the current vector
//			DWORD cnt2 = it1->second.size();
//			for (int i = 0; i < cnt2; i++)
//			{
//				for (auto it2 = it1->second.begin(); it2 != it1->second.end();)
//				{
//						IR* testIR = *it2;
//
//						if (testIR->UseList.size() == 0)
//						{
//							RemoveIR(*it2);
//							it2 = irVector.erase(it2);
//							//it2 = it1->second.erase(it2);
//						}
//
//						else
//						{
//							++it2;
//						}
//					
//				}
//			}
//
//			if (it1->second.size() == 0)
//			{
//				//_plugin_logprintf("IR Vector is null %p\n",it1->first);
//				it1 = IRList.erase(it1);
//			}
//
//			else
//			{
//				++it1;
//			}
//		}
//	}
//
//}