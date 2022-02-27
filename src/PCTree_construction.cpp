#include "PCTree.h"
#include "PCNode.h"

#include <regex>

using namespace pc_tree;

PCTree::PCTree(int leafNum, std::vector<PCNode *> *added) : PCTree() {
    OGDF_ASSERT(leafNum > 2);
    rootNode = newNode(PCNodeType::PNode);
    insertLeaves(leafNum, rootNode, added);
}

PCTree::PCTree(const std::string &str, bool keep_ids) : PCTree() {
    std::string s = std::regex_replace(str, std::regex("\\s+"), ""); //remove whitespaces

    std::stringstream ss(s);
    std::stack<PCNode *> stack;
    int nextIndex = 0;
    bool indexUsed = true;
    char previousChar = ' ';

    while (!ss.eof()) {
        char nextChar = ss.peek();

        if (isdigit(nextChar) || nextChar == '-') {
            ss >> nextIndex;
            if (keep_ids)
                nextNodeId = std::max(nextIndex + 1, nextNodeId);
            else
                nextIndex = nextNodeId++;
            indexUsed = false;
        } else {
            ss.ignore();

            PCNode *parent = stack.empty() ? nullptr : stack.top();
            PCNode *created = nullptr;

            switch (nextChar) {
                case ',':
                    if (previousChar != ']' && previousChar != ')') {
                        if (stack.empty())
                            throw std::invalid_argument("Invalid PC-Tree");

                        created = newNode(PCNodeType::Leaf, parent, nextIndex);
                    }
                    break;
                case '{':
                    if (stack.empty() && getLeafCount() > 0)
                        throw std::invalid_argument("Invalid PC-Tree");

                    OGDF_ASSERT(parent == nullptr);
                    created = newNode(PCNodeType::Leaf, parent, nextIndex);
                    stack.push(created);
                    break;
                case '[':
                    if (stack.empty() && getLeafCount() > 0)
                        throw std::invalid_argument("Invalid PC-Tree");

                    created = newNode(PCNodeType::CNode, parent, nextIndex);
                    stack.push(created);
                    break;
                case '(':
                    if (stack.empty() && getLeafCount() > 0)
                        throw std::invalid_argument("Invalid PC-Tree");

                    created = newNode(PCNodeType::PNode, parent, nextIndex);
                    stack.push(created);
                    break;
                case ']':
                    if (stack.empty() || stack.top()->nodeType != PCNodeType::CNode)
                        throw std::invalid_argument("Invalid PC-Tree");

                    if (previousChar != ']' && previousChar != ')') {
                        created = newNode(PCNodeType::Leaf, parent, nextIndex);
                    }
                    stack.pop();
                    break;
                case ')':
                    if (stack.empty() || stack.top()->nodeType != PCNodeType::PNode)
                        throw std::invalid_argument("Invalid PC-Tree");

                    if (previousChar != ']' && previousChar != ')') {
                        created = newNode(PCNodeType::Leaf, parent, nextIndex);
                    }
                    stack.pop();
                    break;
                case '}':
                    if (stack.empty() || stack.top()->nodeType != PCNodeType::Leaf)
                        throw std::invalid_argument("Invalid PC-Tree");

                    if (previousChar != ']' && previousChar != ')') {
                        created = newNode(PCNodeType::Leaf, parent, nextIndex);
                    }
                    stack.pop();
                    OGDF_ASSERT(stack.empty());
                    break;
                default:
                    break;
            }

            if (created) {
                if (indexUsed)
                    throw std::invalid_argument("Invalid PC-Tree");
                indexUsed = true;
            }

            previousChar = nextChar;
        }
    }
    if (!stack.empty())
        throw std::invalid_argument("Invalid PC-Tree");
}

PCTree::PCTree::PCTree(const PCTree &other, PCTreeNodeArray<PCNode *> &nodeMapping, bool keep_ids) : PCTree() {
    nodeMapping.init(other);
    for (PCNode *other_node: other.allNodes()) {
        PCNode *parent = other_node->getParent();
        int id = -1;
        if (keep_ids) {
            id = other_node->id;
            nextNodeId = std::max(id + 1, nextNodeId);
        }
        OGDF_ASSERT((parent == nullptr) == (other.rootNode == other_node));
        if (parent == nullptr) {
            rootNode = nodeMapping[other_node] = newNode(other_node->getNodeType(), nullptr, id);
        } else {
            nodeMapping[other_node] = newNode(other_node->getNodeType(), nodeMapping[parent], id);
        }
    }
    OGDF_ASSERT(nodeMapping[other.rootNode] == rootNode);
    OGDF_ASSERT(other.getLeafCount() == getLeafCount());
    OGDF_ASSERT(other.getPNodeCount() == getPNodeCount());
    OGDF_ASSERT(other.getCNodeCount() == getCNodeCount());
}


PCTree::~PCTree() {
    if (rootNode != nullptr && rootNode->isLeaf()) {
        // remove root from list of leaves so that it will show up last in the queue
        changeNodeType(rootNode, PCNodeType::PNode);
    }
    while (!leaves.empty()) {
        PCNode *node = leaves.back();
        PCNode *parent = node->getParent();
        PCNodeType type = node->getNodeType();
        bool is_root = node == rootNode;
        OGDF_ASSERT((parent == nullptr) == is_root);
        if (is_root) {
            OGDF_ASSERT(leaves.size() == 1);
            rootNode = nullptr;
        }
        node->detach();
        destroyNode(node);
        if (type != PCNodeType::Leaf) {
            leaves.pop_back(); // destroyNode(leaf) automatically removes it from the list
        }
        if (parent != nullptr && parent->childCount == 0) {
            leaves.push_back(parent);
        }
        if (is_root) {
            OGDF_ASSERT(leaves.empty());
            OGDF_ASSERT(rootNode == nullptr);
        } else {
            OGDF_ASSERT(!leaves.empty());
            OGDF_ASSERT(rootNode != nullptr);
        }
    }
#ifdef PCTREE_REUSE_NODES
    while (!reusableNodes.empty()) {
        delete reusableNodes.back();
        reusableNodes.pop_back();
    }
#endif
    OGDF_ASSERT(rootNode == nullptr);
    OGDF_ASSERT(pNodeCount == 0);
    OGDF_ASSERT(cNodeCount == 0);
}

void PCTree::registerNode(PCNode *node) {
    if (node->nodeType == PCNodeType::Leaf) {
        node->nodeListIndex = leaves.size();
        leaves.push_back(node);
    } else if (node->nodeType == PCNodeType::PNode) {
        pNodeCount++;
    } else {
        OGDF_ASSERT(node->nodeType == PCNodeType::CNode);
        node->nodeListIndex = parents.makeSet();
        OGDF_ASSERT(cNodes.size() == node->nodeListIndex);
        cNodes.push_back(node);
        cNodeCount++;
    }
}

void PCTree::unregisterNode(PCNode *node) {
    if (node->nodeType == PCNodeType::Leaf) {
        OGDF_ASSERT(leaves.at(node->nodeListIndex) == node);
        if (leaves.size() > 1) {
            auto last = leaves.end() - 1;
            (*last)->nodeListIndex = node->nodeListIndex;
            std::iter_swap(leaves.begin() + node->nodeListIndex, last);
            leaves.pop_back();
        } else {
            leaves.clear();
        }
        node->nodeListIndex = -1;
    } else if (node->nodeType == PCNodeType::PNode) {
        pNodeCount--;
    } else {
        OGDF_ASSERT(node->nodeType == PCNodeType::CNode);
        OGDF_ASSERT(cNodes.at(node->nodeListIndex) == node);
        cNodes[node->nodeListIndex] = nullptr;
        cNodeCount--;
        node->nodeListIndex = -1;
    }
}

PCNode *PCTree::newNode(PCNodeType type, PCNode *parent, int id) {
    int oldTableSize = nodeArrayRegistry.keyArrayTableSize();
    PCNode *node;
#ifdef PCTREE_REUSE_NODES
    if (!reusableNodes.empty()) {
        node = reusableNodes.back();
        reusableNodes.pop_back();
        if (id >= 0) {
            node->id = id;
            nextNodeId = std::max(nextNodeId, id + 1);
        } // else we also re-use the old ID
        node->nodeType = type;
    } else
#endif
    {
        if (id < 0)
            id = nextNodeId++;
        else
            nextNodeId = std::max(nextNodeId, id + 1);
        node = new PCNode(this, id, type);
    }
    registerNode(node);
    if (parent != nullptr)
        parent->appendChild(node);
    else if (rootNode == nullptr)
        rootNode = node;
    if (oldTableSize != nodeArrayRegistry.keyArrayTableSize())
        nodeArrayRegistry.enlargeArrayTables();
    return node;
}

void PCTree::destroyNode(PCNode *const &node) {
    OGDF_ASSERT(node->tree == this);
    OGDF_ASSERT(node->isDetached());
    OGDF_ASSERT(node->childCount == 0);
    OGDF_ASSERT(node->child1 == nullptr);
    OGDF_ASSERT(node->child2 == nullptr);
    OGDF_ASSERT(node != rootNode);
    unregisterNode(node);
#ifdef PCTREE_REUSE_NODES
    reusableNodes.push_back(node);
#else
    delete node;
#endif
}

PCNodeType PCTree::changeNodeType(PCNode *node, PCNodeType newType) {
    PCNodeType oldType = node->nodeType;
    if (oldType == newType)
        return oldType;

    unregisterNode(node);
    node->nodeType = newType;
    registerNode(node);

    if (oldType == PCNodeType::CNode || newType == PCNodeType::CNode) {
        PCNode *pred = nullptr;
        PCNode *curr = node->child1;
        int children = 0;
        while (curr != nullptr) {
            if (oldType == PCNodeType::CNode) {
                OGDF_ASSERT(curr->parentPNode == nullptr);
            } else {
                OGDF_ASSERT(curr->parentPNode == node);
                OGDF_ASSERT(curr->parentCNodeId == -1);
            }
            if (newType == PCNodeType::CNode) {
                curr->parentPNode = nullptr;
                curr->parentCNodeId = node->nodeListIndex;
            } else {
                curr->parentPNode = node;
                curr->parentCNodeId = -1;
            }
            children++;
            proceedToNextSibling(pred, curr);
        }
        OGDF_ASSERT(children == node->childCount);
        OGDF_ASSERT(pred == node->child2);
    }

    return oldType;
}

void PCTree::insertLeaves(int count, PCNode *parent, std::vector<PCNode *> *added) {
    OGDF_ASSERT(parent != nullptr);
    OGDF_ASSERT(parent->tree == this);
    if (added) added->reserve(added->size() + count);
    for (int i = 0; i < count; i++) {
        PCNode *leaf = newNode(PCNodeType::Leaf);
        parent->appendChild(leaf);
        if (added) added->push_back(leaf);
    }
}

void PCTree::replaceLeaf(int leafCount, PCNode *leaf, std::vector<PCNode *> *added) {
    OGDF_ASSERT(leaf && leaf->tree == this);
    OGDF_ASSERT(leaf->isLeaf());
    OGDF_ASSERT(leafCount > 1);
    if (getLeafCount() <= 2) {
        changeNodeType(leaf->getParent(), PCNodeType::PNode);
        insertLeaves(leafCount, leaf->getParent(), added);
        leaf->detach();
        destroyNode(leaf);
    } else {
        changeNodeType(leaf, PCNodeType::PNode);
        insertLeaves(leafCount, leaf, added);
    }
}

PCNode *PCTree::mergeLeaves(std::vector<PCNode *> &consecutiveLeaves, bool assumeConsecutive) {
    OGDF_ASSERT(!consecutiveLeaves.empty());
    if (!assumeConsecutive && !makeConsecutive(consecutiveLeaves)) {
        return nullptr;
    }

    // Remove all consecutive leaves except the first one.
    for (auto it = ++consecutiveLeaves.begin(); it != consecutiveLeaves.end(); ++it) {
        PCNode *leaf = *it;
        PCNode *parent = leaf->getParent();
        leaf->detach();
        destroyNode(leaf);
        if (parent->childCount == 1) {
            PCNode *child = parent->child1;
            child->detach();
            parent->replaceWith(child);
            destroyNode(parent);
        } else if (parent->childCount == 2 && parent == rootNode && leaves.size() > 2) {
            PCNode *leafChild = parent->child1->isLeaf() ? parent->child1 : parent->child2;
            PCNode *nonLeafChild = parent->getOtherOuterChild(leafChild);
            OGDF_ASSERT(leafChild->isLeaf());
            OGDF_ASSERT(!nonLeafChild->isLeaf());
            nonLeafChild->detach();
            leafChild->detach();
            rootNode = nonLeafChild;
            destroyNode(parent);
            nonLeafChild->appendChild(leafChild);
        } else {
            OGDF_ASSERT(parent->getChildCount() > 1);
        }
    }
    // OGDF_HEAVY_ASSERT(checkValid()); // this breaks if the replacement reduces the leaves to two or less

    // Return the remaining leaf.
    return consecutiveLeaves.front();
}
