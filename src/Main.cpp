#define REGEX_TREE_DEBUG

#include <iostream>
#include "DFA.hpp"
#include "RegexTree.hpp"

void TestRegexTree()
{
	auto tree = RegexTree("a");
	tree.DisplayFollowPos(std::cout);
	tree.DisplayState("./output/tree.md");
    auto dfa = DFA(tree);
    dfa.DisplayState("./output/dfa.md");
	
}

int main()
{
	TestRegexTree();
} 
