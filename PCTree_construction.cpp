#include "PCTree.h"
#include "PCNode.h"
#include "PCArc.h"

#include <regex>

using namespace pc_tree::hsu;

PCTree::PCTree(int leafNum, std::vector<PCNode *> *added) : PCTree() {
    OGDF_ASSERT(leafNum >= 2);
    PCNode *rootNode = newNode(PCNodeType::PNode, nullptr);
    insertLeaves(leafNum, rootNode, added);
}

PCTree::PCTree(const std::string &str) : PCTree() {
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
            indexUsed = false;
        } else {
            ss.ignore();

            PCNode *parent = stack.empty() ? nullptr : stack.top();
            PCNode *newNodePtr = nullptr;

            switch (nextChar) {
                case ',':
                    if (previousChar != ']' && previousChar != ')') {
                        if (stack.empty())
                            throw std::invalid_argument("Invalid PC-Tree");

                        newNodePtr = newNode(PCTree::PCNodeType::Leaf, parent);
                    }
                    break;
                case '[':
                    if (stack.empty() && getLeafCount() > 0)
                        throw std::invalid_argument("Invalid PC-Tree");

                    newNodePtr = newNode(PCTree::PCNodeType::CNode, parent);
                    stack.push(newNodePtr);
                    break;
                case '(':
                    if (stack.empty() && getLeafCount() > 0)
                        throw std::invalid_argument("Invalid PC-Tree");

                    newNodePtr = newNode(PCTree::PCNodeType::PNode, parent);
                    stack.push(newNodePtr);
                    break;
                case ']':
                    if (stack.empty() || stack.top()->nodeType != PCNodeType::CNode)
                        throw std::invalid_argument("Invalid PC-Tree");

                    if (previousChar != ']' && previousChar != ')') {
                        newNodePtr = newNode(PCTree::PCNodeType::Leaf, parent);
                    }
                    stack.pop();
                    break;
                case ')':
                    if (stack.empty() || stack.top()->nodeType != PCNodeType::PNode)
                        throw std::invalid_argument("Invalid PC-Tree");

                    if (previousChar != ']' && previousChar != ')') {
                        newNodePtr = newNode(PCTree::PCNodeType::Leaf, parent);
                    }
                    stack.pop();
                    break;
                default:
                    break;
            }

            if (newNodePtr) {
                if (indexUsed)
                    throw std::invalid_argument("Invalid PC-Tree");

                setIndex(newNodePtr, nextIndex);
                indexUsed = true;
            }

            previousChar = nextChar;
        }
    }
    if (!stack.empty())
        throw std::invalid_argument("Invalid PC-Tree");
}

PCTree::PCTree(const PCTree &other, PCTreeNodeArray<PCNode *> *leafMapping) : PCTree() {
    PCArc *startArc = other.findRootEntryArc();
    if (startArc == nullptr) {
        if (other.tempRootNode != nullptr) {
            tempRootNode = newNode(other.tempRootNode->getNodeType(), nullptr);
        }
        return;
    }
    OGDF_ASSERT(leafMapping == nullptr || leafMapping->registeredAt() == &other.nodeArrayRegistry);

    PCNode *newRoot = newNode(startArc->getYNodeType(), nullptr);

    // Hold the next arc in the original tree and the parent node in the copy.
    std::stack<std::pair<PCArc *, PCNode *>> stack;
    stack.emplace(startArc, newRoot);

    while (!stack.empty()) {
        PCArc *nextArc = stack.top().first;
        PCNode *parent = stack.top().second;
        PCArc *previousArc = nextArc;
        PCArc *currentArc = nextArc->neighbor1;
        stack.pop();

        while (currentArc != nextArc) {
            PCNode *newNodePtr = newNode(currentArc->twin->getYNodeType(), parent);

            if (leafMapping && currentArc->twin->getYNodeType() == PCNodeType::Leaf) {
                PCNode *originalLeaf = currentArc->twin->getYNode();
                (*leafMapping)[originalLeaf] = newNodePtr;
            }
            stack.emplace(currentArc->twin, newNodePtr);
            PCArc *temp = currentArc;
            currentArc = currentArc->getNextArc(previousArc);
            previousArc = temp;
        }

        if (parent == newRoot) {
            PCNode *newNodePtr = newNode(currentArc->twin->getYNodeType(), parent);

            if (leafMapping && currentArc->twin->getYNodeType() == PCNodeType::Leaf) {
                PCNode *originalLeaf = currentArc->twin->getYNode();
                (*leafMapping)[originalLeaf] = newNodePtr;
            }
            stack.emplace(currentArc->twin, newNodePtr);
        }
    }

    OGDF_ASSERT(other.getLeaves().size() == leaves.size());
}


PCTree::~PCTree() {
    if ((tempRootNodeEntryArc == nullptr || timestamp != 0) && leaves.empty()) {
        delete tempRootNode;
    } else {
        PCArc *startArc = (tempRootNodeEntryArc != nullptr && timestamp == 0) ? tempRootNodeEntryArc : leaves.front()->parentArc;
        std::stack<PCArc *> stack;
        stack.push(startArc);

        while (!stack.empty()) {
            PCArc *nextArc = stack.top();
            stack.pop();

            // Delete all arcs in the circular list and their respective subtree.
            for (PCArc *current : *nextArc) {
                if (nextArc != startArc && current == nextArc) {
                    continue;
                }
                stack.push(current->twin);
            }

            if (nextArc != startArc) {
                delete nextArc->twin;
                delete nextArc;
            }
        }
    }

    for (PCNode *node : terminalPath) {
        deleteNodePointer(node);
    }

    deleteNodePointer(apexCandidate);

}

void PCTree::insertLeafIntoList(PCNode *leaf) {
    OGDF_ASSERT(leaf->tree == this);
    leaf->listIndex = leaves.size();
    leaves.push_back(leaf);
    OGDF_ASSERT(leaves.at(leaf->listIndex) == leaf);
}

void PCTree::removeLeafFromList(PCNode *leaf) {
    OGDF_ASSERT(leaf->tree == this);
    if (leaves.size() > 1) {
        auto pos = leaves.begin() + leaf->listIndex;
        OGDF_ASSERT(*pos == leaf);
        leaves.back()->listIndex = leaf->listIndex;
        std::iter_swap(pos, leaves.end() - 1);
        leaves.pop_back();
    } else {
        leaves.clear();
    }
    leaf->listIndex = -1;
}

void PCTree::insertLeaves(int count, PCNode *parent, std::vector<PCNode *> *added) {
    OGDF_ASSERT(parent != nullptr);
    OGDF_ASSERT(parent->tree == this);
    for (int i = 0; i < count; i++) {
        PCNode *leaf = newNode(PCNodeType::Leaf, parent);
        if (added) added->push_back(leaf);
    }
}

PCNode *PCTree::newNode(PCNodeType type, PCNode *parent) {
    OGDF_ASSERT(parent != nullptr || tempRootNode == nullptr);
    OGDF_ASSERT(parent == nullptr || parent->tree == this);
    PCArc *entryArc = parent ? (parent->parentArc ? parent->parentArc->twin : tempRootNodeEntryArc) : nullptr;
    PCArc *entryArcNeighbor = entryArc ? entryArc->neighbor1 : nullptr;

    return insertNode(type, entryArc, entryArcNeighbor);
}

PCNode *PCTree::insertNode(PCNodeType type, PCArc *adjacent1, PCArc *adjacent2) {
    PCNode *node = createNode(type);

    PCNode *parent;
    if (adjacent1 == nullptr && adjacent2 == nullptr) {
        OGDF_ASSERT(tempRootNodeEntryArc == nullptr && timestamp == 0);
        if (tempRootNode == nullptr) {
            OGDF_ASSERT(type != PCNodeType::Leaf);
            tempRootNode = node;
            return node;
        }

        parent = tempRootNode;
        tempRootNode = nullptr;
    } else {
        OGDF_ASSERT(adjacent1->tree == this && adjacent2->tree == this);
        OGDF_ASSERT(adjacent1->isAdjacent(adjacent2)
                    || (adjacent1->isAdjacent(nullptr)) && adjacent2->isAdjacent(nullptr));
        OGDF_ASSERT(adjacent1->getYNodeType() != PCNodeType::Leaf);
        parent = adjacent1->getyNode(timestamp);
    }

    PCArc *parentArc = createArc();
    PCArc *parentArcTwin = createArc();
    parentArc->yParent = true;
    parentArc->twin = parentArcTwin;
    parentArcTwin->twin = parentArc;
    parentArcTwin->yParent = false;
    parentArc->setyNode(parent, timestamp);
    parentArcTwin->setyNode(node, timestamp);
    parentArcTwin->neighbor1 = parentArcTwin;
    parentArcTwin->neighbor2 = parentArcTwin;
    parentArc->neighbor1 = adjacent1;
    parentArc->neighbor2 = adjacent2;

    if (adjacent1 != nullptr && adjacent2 != nullptr) {
        if (adjacent1->neighbor1 == nullptr || (adjacent1->neighbor2 != nullptr && adjacent1->neighbor1 == adjacent2)) {
            adjacent1->neighbor1 = parentArc;
        } else {
            adjacent1->neighbor2 = parentArc;
        }

        if (adjacent2->neighbor1 == nullptr || (adjacent2->neighbor2 != nullptr && adjacent2->neighbor1 == adjacent1)) {
            adjacent2->neighbor1 = parentArc;
        } else {
            adjacent2->neighbor2 = parentArc;
        }
    } else {
        parentArc->neighbor1 = parentArc;
        parentArc->neighbor2 = parentArc;
        tempRootNodeEntryArc = parentArc;
    }

    if (type == PCNodeType::PNode) {
        node->degree = 1;
    } else if (type == PCNodeType::Leaf) {
        node->degree = 1;
        insertLeafIntoList(node);
    }


    node->parentArc = parentArc;
    if (parent != nullptr && parent->nodeType == PCNodeType::PNode) {
        parent->degree++;
    }

    return node;
}
