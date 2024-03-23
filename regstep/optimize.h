#pragma once
#include "Value.h"

void RemoveIR(IR* _irPtr);
void DeadStoreElimination(multimap<DWORD, vector<IR*>>& IRList);