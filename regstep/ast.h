#pragma once
#include <string>
#include <memory>
#include "z3++.h"

using namespace std;
class Value;

enum NodeType
{
	NT_NONE,
	NT_OPERATOR,
	NT_OPERAND,
	NT_SYMVAR
};

class BTreeNode
{
public:
	NodeType m_NodeType;
	std::string nodeName;
	unsigned int childrenCount =0;
	Value* valPtr;
	std::shared_ptr<BTreeNode> left;
	std::shared_ptr<BTreeNode> right;
	std::shared_ptr<BTreeNode> third;
	std::shared_ptr<BTreeNode> fourth;	
};


BTreeNode MakeBTreeNode(string nodeName);
string GetData(std::shared_ptr<BTreeNode> bt);
void SetData(BTreeNode* bt, string data);

std::shared_ptr<BTreeNode> GetLeftSubTree(std::shared_ptr<BTreeNode> bt);
std::shared_ptr<BTreeNode> GetRightSubTree(std::shared_ptr<BTreeNode> bt);

void MakeLeftSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub);
void MakeRightSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub);
void MakeThirdSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub);
void MakeFourthSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub);

typedef void Extract(BTreeNode* bt);

void PreorderTraverse(std::shared_ptr<BTreeNode> bt);
void InorderTraverse(std::shared_ptr<BTreeNode> bt);
void PostorderTraverse(std::shared_ptr<BTreeNode> bt);

string EvaluateExpTree(std::shared_ptr<BTreeNode> bt);
z3::expr GetZ3ExprFromTree(std::shared_ptr<BTreeNode> bt);