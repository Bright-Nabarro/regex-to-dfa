#include "DFA.h"
#include "RegexTree.h"

int main()
{
    auto tree = RegexTree("(a|b)cd*");
	tree.CreateDotFile("./output/tree.gv");
    auto dfa = DFA(tree);
    dfa.CreateDotFile("./output/dfa.gv");
}
