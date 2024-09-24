#include "RegexTree.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <unordered_set>

RegexTree::Node::~Node() {}

std::unique_ptr<RegexTree::Node> RegexTree::BuildTree(std::string_view regex,
                                                      bool star)
{
    if (regex.length() == 1 || (regex.front() == '\\' && regex.length() == 2))
    {
        auto leaf = std::make_unique<LeafNode>(
            leaves.size(), regex.length() == 1 ? regex[0] : regex[1]);
        leaves.emplace_back(leaf.get());
        return leaf;
    }

    // ...|...
    int open_parens = 0;
    for (std::size_t i = 0; i < regex.length(); ++i)
    {
        // don't consider "\(" or "\)" as regex parens
        if (regex[i] == '\\')
        {
            ++i;
            continue;
        }

        open_parens += regex[i] == '(';
        open_parens -= regex[i] == ')';
        if (regex[i] == '|' && open_parens == 0)
        {
            return std::make_unique<UnionNode>(BuildTree(regex.substr(0, i)),
                                               BuildTree(regex.substr(i + 1)));
        }
    }

    // TODO: support escaping `*`, `(`, `)`, and `|` with `\`
    switch (regex.back())
    {
    case '*':
    {
        return BuildTree(regex.substr(0, regex.length() - 1), true);
    }
    case ')':
    {
        const auto open_paren_idx = [&]
        {
            int close_parens = 1;
            for (std::size_t i = regex.length() - 2; i >= 0; --i)
            {
                close_parens += regex[i] == ')';
                close_parens -= regex[i] == '(';
                if (close_parens == 0)
                    return i;
            }
            // unbalanced parens
            throw std::exception();
        }();

        // (...)
        if (open_paren_idx == 0)
        {
            if (star)
            {
                return std::make_unique<StarNode>(
                    BuildTree(regex.substr(1, regex.length() - 2)));
            }
            else
            {
                return BuildTree(regex.substr(1, regex.length() - 2));
            }
        }
        // ...(...)
        else
        {
            auto left = BuildTree(regex.substr(0, open_paren_idx));
            auto right = [&]() -> std::unique_ptr<Node>
            {
                if (star)
                {
                    return std::make_unique<StarNode>(
                        BuildTree(regex.substr(open_paren_idx)));
                }
                else
                {
                    return BuildTree(regex.substr(open_paren_idx));
                }
            }();
            return std::make_unique<ConcatNode>(std::move(left),
                                                std::move(right));
        }
    }
    default:
    {
        // ...
        auto left = BuildTree(regex.substr(0, regex.length() - 1));
        auto right = [&]() -> std::unique_ptr<Node>
        {
            if (star)
            {
                return std::make_unique<StarNode>(
                    BuildTree(regex.substr(regex.length() - 1)));
            }
            else
            {
                return BuildTree(regex.substr(regex.length() - 1));
            }
        }();
        return std::make_unique<ConcatNode>(std::move(left), std::move(right));
    }
    }
}

std::unordered_set<char> RegexTree::Alphabet(RegexTree::Node* n)
{
    if (auto node = dynamic_cast<RegexTree::ConcatNode*>(n))
    {
        auto alphabet1 = Alphabet(node->left.get());
        auto alphabet2 = Alphabet(node->right.get());
        alphabet1.insert(alphabet2.begin(), alphabet2.end());
        return alphabet1;
    }
    else if (auto node = dynamic_cast<RegexTree::UnionNode*>(n))
    {
        auto alphabet1 = Alphabet(node->left.get());
        auto alphabet2 = Alphabet(node->right.get());
        alphabet1.insert(alphabet2.begin(), alphabet2.end());
        return alphabet1;
    }
    else if (auto node = dynamic_cast<RegexTree::StarNode*>(n))
    {
        return Alphabet(node->child.get());
    }
    else if (auto node = dynamic_cast<RegexTree::LeafNode*>(n))
    {
        return std::unordered_set<char>({node->label});
    }
    else if (auto node = dynamic_cast<RegexTree::EndNode*>(n))
    {
        return std::unordered_set<char>();
    }
    else
    {
        // there is no 6th type of RegexTree nodes
        throw std::exception();
    }
}

std::unique_ptr<RegexTree::Node>
RegexTree::ConcatEndNode(std::unique_ptr<RegexTree::Node> root)
{
    auto end_node = std::make_unique<EndNode>(EndPos());
    return std::make_unique<ConcatNode>(std::move(root), std::move(end_node));
}

void RegexTree::CalcFollowPos(RegexTree::Node* n)
{
    if (auto node = dynamic_cast<RegexTree::UnionNode*>(n))
    {
        CalcFollowPos(node->left.get());
        CalcFollowPos(node->right.get());
    }
    else if (auto node = dynamic_cast<RegexTree::ConcatNode*>(n))
    {
        for (auto i : node->left->lastpos)
        {
            leaves[i]->followpos.insert(node->right->firstpos.cbegin(),
                                        node->right->firstpos.cend());
        }
        CalcFollowPos(node->left.get());
        CalcFollowPos(node->right.get());
    }
    else if (auto node = dynamic_cast<RegexTree::StarNode*>(n))
    {
        for (auto i : node->lastpos)
        {
            leaves[i]->followpos.insert(node->firstpos.cbegin(),
                                        node->firstpos.cend());
        }
        CalcFollowPos(node->child.get());
    }
}


void RegexTree::DisplayState(std::string_view filepath) const
{
	std::ofstream outfile { filepath.data() };
	outfile << "``` dot\n"
			<< "digraph regex_tree {\n"
			<< "node [shape=plaintext]\n";
	if (root != nullptr)
		root->Display(outfile, this);
	outfile << "}\n"
			<< "```\n";
}


void RegexTree::DisplayFollowPos(std::ostream& os) const
{
	for(auto leave : leaves)
	{
		os << std::format("{}: ", leave->label);
		for (auto leave_idx : leave->followpos)
		{
			if (leave_idx == leaves.size())
				os << std::format("{}, ", '#');
			else
				os << std::format("{}, ", leaves[leave_idx]->label);
		}
		os << std::endl;
	}
}

void RegexTree::Display3Pos(const Node& node, std::ostream& os) const
{
	os << std::format("{}[{}]: \n", node.ToString().first, node.ToString().second);
	os << std::format("\tnullable: {}\n", node.nullable);
	os << std::format("\tfirstpos: \n\t\t");

	auto print_leave = [&](size_t idx){
		if (idx == leaves.size())
			os << "#, ";
		else
			os << std::format("{}, ", leaves[idx]->label);
	};
	for (auto leave_idx : node.firstpos)
	{
		print_leave(leave_idx);
	}

	os << std::format("\n\tlastpos: \n\t\t");
	for (auto leave_idx : node.lastpos)
	{
		print_leave(leave_idx);
	}
	os << std::endl;
}

