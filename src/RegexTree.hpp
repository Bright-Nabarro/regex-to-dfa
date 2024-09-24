#pragma once

#include <memory>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <format>
#include <cassert>
#include <iostream>


#define PRINT_POS(tree, node) (tree)->Display3Pos( (node), std::cout)

namespace
{
// this is used because the function RegexTree::FollowPos needs to return
// a const reference to a followpos set, so it can't return an empty set
// created locally in that function, since it will go out of scope once the
// function returns.
static const std::unordered_set<std::size_t> empty_set;
} // namespace

class RegexTree
{
public:
    explicit RegexTree(std::string_view regex)
        : root(ConcatEndNode(BuildTree(regex))), alphabet(Alphabet(root.get()))
    {
        CalcFollowPos(root.get());
    }
    /// Return an unordered_set of unique characters that exist in the regex.
    const std::unordered_set<char>& Alphabet() const
    {
        return alphabet;
    }

    /// Return FirstPos set for the root of the regex tree.
    const std::unordered_set<std::size_t>& FirstPosRoot() const
    {
        return root->firstpos;
    }

    /// Return the FollowPos set for a leaf in the regex tree given its
    /// position.
    const std::unordered_set<std::size_t>& FollowPos(std::size_t pos) const
    {
        return pos < leaves.size() ? leaves[pos]->followpos : empty_set;
    }

    /// Return true if the label of the leaf at the given position equals the
    /// given character, and return false otherwise.
    bool CharAtPos(char character, std::size_t pos) const
    {
        return pos < leaves.size() ? leaves[pos]->label == character : false;
    }

    /// Return the position of the end of the regex, which equals the number of
    /// leaves in the regex tree.
    std::size_t EndPos() const
    {
        return leaves.size();
    }

	//newly added
public:
	void DisplayState(std::string_view filepath) const;
	
	void DisplayFollowPos(std::ostream& os) const;

private:
    class Node
    {
    public:
        virtual ~Node() = 0;
		virtual void Display(std::ofstream& out, const RegexTree* tree) const = 0;
		///first为原始字符，second为特定标识符
		virtual std::pair<std::string, std::string> ToString() const = 0;

		///leaves索引，firstpos && lastpos 只会包含leaves
        std::unordered_set<std::size_t> firstpos;
        std::unordered_set<std::size_t> lastpos;
        bool nullable;
    };

    class ConcatNode : public Node
    {
    public:
        explicit ConcatNode(std::unique_ptr<Node> l, std::unique_ptr<Node> r)
            : left(std::move(l)), right(std::move(r))
        {
            firstpos.insert(left->firstpos.cbegin(), left->firstpos.cend());
            if (left->nullable)
            {
                firstpos.insert(right->firstpos.cbegin(),
                                right->firstpos.cend());
            }

            lastpos.insert(right->lastpos.cbegin(), right->lastpos.cend());
            if (right->nullable)
            {
                lastpos.insert(left->lastpos.cbegin(), left->lastpos.cend());
            }

            nullable = left->nullable && right->nullable;
        }

		void Display(std::ofstream& out, const RegexTree* tree) const override
		{
			auto this_id = ToString();
			assert(left != nullptr);
			assert(right != nullptr);

			out << std::format(R"("{}" [label = "{}"])", this_id.second, this_id.first)
				<< '\n';

			auto left_id = left->ToString();
			out << std::format(R"("{}" -> "{}")", this_id.second, left_id.second)
				<< '\n';

			auto right_id = right->ToString();
			out << std::format(R"("{}" -> "{}")", this_id.second, right_id.second)
				<< '\n';

			PRINT_POS(tree, *this);
			left->Display(out, tree);
			right->Display(out, tree);
		}

		std::pair<std::string, std::string> ToString() const override
		{
			return { "•", std::format("{}", reinterpret_cast<const void*>(this)) };
		}


        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    class UnionNode : public Node
    {
    public:
        explicit UnionNode(std::unique_ptr<Node> l, std::unique_ptr<Node> r)
            : left(std::move(l)), right(std::move(r))
        {
            firstpos.insert(left->firstpos.cbegin(), left->firstpos.cend());
            firstpos.insert(right->firstpos.cbegin(), right->firstpos.cend());

            lastpos.insert(left->lastpos.cbegin(), left->lastpos.cend());
            lastpos.insert(right->lastpos.cbegin(), right->lastpos.cend());

            nullable = left->nullable || right->nullable;
        }

		void Display(std::ofstream& out, const RegexTree* tree) const override
		{
			auto this_id = ToString();
			assert(left != nullptr);
			assert(right != nullptr);

			out << std::format(R"("{}" [label = "{}"])", this_id.second, this_id.first)
				<< '\n';

			auto left_id = left->ToString();
			out << std::format(R"("{}" -> "{}")", this_id.second, left_id.second)
				<< '\n';

			auto right_id = right->ToString();
			out << std::format(R"("{}" -> "{}")", this_id.second, right_id.second)
				<< '\n';
			PRINT_POS(tree, *this);
			left->Display(out, tree);
			right->Display(out, tree);
		}

		std::pair<std::string, std::string> ToString() const override
		{
			return { "|", std::format("{}", reinterpret_cast<const void*>(this)) };
		}

        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

    };

    class StarNode : public Node
    {
    public:
        explicit StarNode(std::unique_ptr<Node> c) : child(std::move(c))
        {
            firstpos = child->firstpos;
            lastpos = child->lastpos;
            nullable = true;
        }

		void Display(std::ofstream& out, const RegexTree* tree) const override
		{
			assert(child != nullptr);
			auto this_id = ToString();
			out << std::format(R"("{}" [label = "{}"])", this_id.second, this_id.first)
				<< '\n';

			auto child_id = child->ToString();
			out << std::format(R"("{}" -> "{}")", this_id.second, child_id.second)
				<< '\n';

			PRINT_POS(tree, *this);
			child->Display(out, tree);
		}

		std::pair<std::string, std::string> ToString() const override
		{
			return { "*", std::format("{}", reinterpret_cast<const void*>(this)) };
		}

        std::unique_ptr<Node> child;
    };

    class LeafNode : public Node
    {
    public:
        explicit LeafNode(std::size_t pos, char l) : label(l)
        {
            firstpos.emplace(pos);
            lastpos.emplace(pos);
            nullable = false;
        }

		void Display(std::ofstream& out, const RegexTree* tree) const override
		{
			auto this_id = ToString();
			out << std::format(R"("{}" [label = "{}"])", this_id.second, this_id.first)
				<< '\n';
			PRINT_POS(tree, *this);
		}

		std::pair<std::string, std::string> ToString() const override
		{
			return { std::format("{}", label) ,
				 	 std::format("{}", reinterpret_cast<const void*>(this)) };
		}

        std::unordered_set<std::size_t> followpos;
        char label;
    };

    class EndNode : public Node
    {
    public:
        explicit EndNode(std::size_t end_pos)
        {
            firstpos.insert(end_pos);
        }

		void Display(std::ofstream& out, const RegexTree* tree) const override
		{
			auto this_id = ToString();
			out << std::format(R"("{}" [label = "{}"])", this_id.second, this_id.first)
				<< '\n';

			PRINT_POS(tree, *this);
		}

		std::pair<std::string, std::string> ToString() const override
		{
			return { "#" , std::format("{}", reinterpret_cast<const void*>(this)) };
		}
    };

	std::unique_ptr<Node> BuildTree(std::string_view regex, bool star = false);

    /// Create an EndNode and concatenate it with the root of the regex tree.
    std::unique_ptr<Node> ConcatEndNode(std::unique_ptr<RegexTree::Node> root);

    std::unordered_set<char> Alphabet(Node* node);
    void CalcFollowPos(Node* node);

	friend void TestRegexTree();
	friend class DisplayPos;
	void Display3Pos(const Node& node, std::ostream& os) const;

    std::vector<LeafNode*> leaves;
    std::unique_ptr<Node> root;
    std::unordered_set<char> alphabet;
};
