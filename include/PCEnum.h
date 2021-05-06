#pragma once

#include <ostream>

namespace pc_tree {
    enum class NodeLabel {
        Unknown,
        Partial,
        Full,
        Empty = Unknown
    };

    enum class PCNodeType {
        PNode,
        CNode,
        Leaf
    };

    class PCTree;

    class PCNode;
}

std::ostream &operator<<(std::ostream &, pc_tree::NodeLabel);

std::ostream &operator<<(std::ostream &, pc_tree::PCNodeType);

std::ostream &operator<<(std::ostream &, const pc_tree::PCTree *);

std::ostream &operator<<(std::ostream &, const pc_tree::PCNode *);

std::ostream &operator<<(std::ostream &, const pc_tree::PCTree &);

std::ostream &operator<<(std::ostream &, const pc_tree::PCNode &);
