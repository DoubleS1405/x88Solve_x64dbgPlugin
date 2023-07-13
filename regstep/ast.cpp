#include <stdio.h>
#include <stdlib.h>
#include "ast.h"


BTreeNode MakeBTreeNode(string nodeName)
{
	BTreeNode nd;// = (BTreeNode*)malloc(sizeof(BTreeNode));

	nd.nodeName = nodeName;
	return nd;
}

string GetData(std::shared_ptr<BTreeNode> bt)
{
	return bt->nodeName;
}

void SetData(BTreeNode* bt, string data)
{
	bt->nodeName = data;
}

std::shared_ptr<BTreeNode> GetLeftSubTree(std::shared_ptr<BTreeNode> bt)
{
	return bt->left;
}

std::shared_ptr<BTreeNode> GetRightSubTree(std::shared_ptr<BTreeNode> bt)
{
	return bt->right;
}

void MakeLeftSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub)
{
	main->left = sub;
}

void MakeRightSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub)
{
	main->right = sub;
}

void MakeThirdSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub)
{
	main->third = sub;
}

void MakeFourthSubTree(std::shared_ptr<BTreeNode> main, std::shared_ptr<BTreeNode> sub)
{
	main->fourth = sub;
}

void PostorderTraverse(std::shared_ptr<BTreeNode> bt)
{
	if (bt == NULL)
		return;

	printf("(%s ", bt->nodeName.c_str());
	PostorderTraverse(bt->left);
	PostorderTraverse(bt->right);
	PostorderTraverse(bt->third);
	PostorderTraverse(bt->fourth);
	printf(")", bt->nodeName.c_str());
}

void PreorderTraverse(std::shared_ptr<BTreeNode> bt)
{
	if (bt == NULL)
		return;

	//if (bt->left && bt->right && bt->third && bt->fourth)
	//{
	//	printf("(");
	//}
	printf("(%s", bt->nodeName.c_str());
	PreorderTraverse(bt->left);
	PreorderTraverse(bt->right);
	PreorderTraverse(bt->third);
	PreorderTraverse(bt->fourth);
	printf(")", bt->nodeName.c_str());
}

void InorderTraverse(std::shared_ptr<BTreeNode> bt)
{
	if (bt == NULL)
		return;

	if (bt->left == nullptr)
	{
		printf("%s ", bt->nodeName.c_str());
	}
	else if (bt->left != nullptr && (bt->right == nullptr))
	{
		printf("(%s ", bt->nodeName.c_str());
		InorderTraverse(bt->left);
		printf(")", bt->nodeName.c_str());
	}

	else if (bt->left != nullptr && (bt->right != nullptr) && (bt->third == nullptr))
	{
		InorderTraverse(bt->left);
		printf("%s ", bt->nodeName.c_str());
		InorderTraverse(bt->right);
	}

	else if (bt->left != nullptr && (bt->right != nullptr) && (bt->third != nullptr && (bt->fourth == nullptr)))
	{
		printf("(%s ", bt->nodeName.c_str());
		InorderTraverse(bt->left);
		InorderTraverse(bt->right);
		InorderTraverse(bt->third);
		printf(")", bt->nodeName.c_str());
	}

	else if (bt->left != nullptr && (bt->right != nullptr) && (bt->third != nullptr && (bt->fourth != nullptr)))
	{
		printf("(%s ", bt->nodeName.c_str());
		InorderTraverse(bt->left);
		InorderTraverse(bt->right);
		InorderTraverse(bt->third);
		InorderTraverse(bt->fourth);
		printf(")", bt->nodeName.c_str());
	}
}