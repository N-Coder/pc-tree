#include "PCNode.h"
#include "PCTree.h"
#include "PCTreeIterators.h"

using namespace pc_tree;

void PCNode::appendChild(PCNode *node, bool begin) {
    OGDF_ASSERT(node != nullptr);
    OGDF_ASSERT(tree == node->tree);
    OGDF_ASSERT(this != node);
    OGDF_ASSERT(isValidNode());
    OGDF_ASSERT(node->isValidNode(tree));
    node->setParent(this);
    childCount++;
    if (child1 == nullptr) {
        // new child of node without other children
        OGDF_ASSERT(child2 == nullptr);
        child1 = child2 = node;
    } else {
        // append new child
        PCNode *&outerChild = begin ? child1 : child2;
        outerChild->replaceSibling(nullptr, node);
        node->replaceSibling(nullptr, outerChild);
        outerChild = node;
    }
    OGDF_ASSERT(isValidNode());
    OGDF_ASSERT(node->isValidNode(tree));
}

void PCNode::insertBetween(PCNode *sib1, PCNode *sib2) {
    if (sib1 == nullptr && sib2 != nullptr) {
        insertBetween(sib2, sib1);
        return;
    }
    OGDF_ASSERT(isValidNode());
    OGDF_ASSERT(sib1 != nullptr);
    OGDF_ASSERT(sib1->tree == tree);
    OGDF_ASSERT(sib1->isValidNode(tree));
    PCNode *parent = sib1->getParent();
    if (sib2 == nullptr) {
        // append at one end of the list
    } else if (sib1->isSiblingOf(sib2)) {
        // insert within one list
        OGDF_ASSERT(parent != nullptr);
        OGDF_ASSERT(parent->isParentOf(sib2));
        if (sib1->isSiblingAdjacent(sib2)) {
            //normal case, both nodes are adjacent children of the same parent
            OGDF_ASSERT(sib2->isValidNode(tree));
            OGDF_ASSERT(parent->isValidNode(tree));
            setParent(parent);
            parent->childCount++;
            sib1->replaceSibling(sib2, this);
            sib2->replaceSibling(sib1, this);
            this->replaceSibling(nullptr, sib1);
            this->replaceSibling(nullptr, sib2);
            OGDF_ASSERT(isValidNode());
            OGDF_ASSERT(parent->isValidNode(tree));
            OGDF_ASSERT(sib1->isValidNode(tree));
            OGDF_ASSERT(sib2->isValidNode(tree));
            return;
        } else {
            // wrap around if no parent is present and both nodes are outer nodes of the same parent
            OGDF_ASSERT(parent->isDetached());
            OGDF_ASSERT(parent->isChildOuter(sib1));
            OGDF_ASSERT(parent->isChildOuter(sib2));
        }
    } else if (parent != nullptr && sib2->isParentOf(parent)) {
        // sib2 is the parent of `parent`, sib1 is an outer child of `parent`
    } else {
        std::swap(sib1, sib2);
        parent = sib1->getParent();
        // now we should have the same situation as before
        OGDF_ASSERT(parent != nullptr);
        OGDF_ASSERT(sib2->isParentOf(parent));
    }

    OGDF_ASSERT(parent != nullptr);
    OGDF_ASSERT(parent->isValidNode(tree));
    OGDF_ASSERT(sib2 == nullptr || sib2->isValidNode(tree));
    setParent(parent);
    parent->childCount++;

    sib1->replaceSibling(nullptr, this);
    parent->replaceOuterChild(sib1, this);
    this->replaceSibling(nullptr, sib1);

    OGDF_ASSERT(isValidNode());
    OGDF_ASSERT(parent->isValidNode(tree));
    OGDF_ASSERT(sib1->isValidNode(tree));
    OGDF_ASSERT(sib2 == nullptr || sib2->isValidNode(tree));
}

void PCNode::detach() {
#ifdef OGDF_DEBUG
    OGDF_ASSERT(isValidNode());
    PCNode *parent = getParent();
    OGDF_ASSERT(parent == nullptr || parent->isValidNode(tree));
    forceDetach();
    OGDF_ASSERT(isValidNode());
    OGDF_ASSERT(parent == nullptr || parent->isValidNode(tree));
#else
    forceDetach();
#endif
}

void PCNode::forceDetach() {
    PCNode *parent = getParent();
    if (sibling1 != nullptr)
        sibling1->replaceSibling(this, sibling2);
    else if (parent != nullptr)
        parent->replaceOuterChild(this, sibling2);
    if (sibling2 != nullptr)
        sibling2->replaceSibling(this, sibling1);
    else if (parent != nullptr)
        parent->replaceOuterChild(this, sibling1);
    if (parent != nullptr) {
        OGDF_ASSERT(!parent->isChildOuter(this));
        parent->childCount--;
    }
    parentCNodeId = -1;
    parentPNode = nullptr;
    sibling1 = sibling2 = nullptr;
}

void PCNode::replaceWith(PCNode *node) {
    OGDF_ASSERT(node != nullptr);
    OGDF_ASSERT(node != this);
    OGDF_ASSERT(tree == node->tree);
    OGDF_ASSERT(node->isDetached());
    OGDF_ASSERT(node->isValidNode(tree));
    OGDF_ASSERT(this != node);
    PCNode *parent = getParent();
    OGDF_ASSERT(parent == nullptr || parent->isValidNode(tree));
    node->parentCNodeId = parentCNodeId;
    node->parentPNode = parentPNode;
    node->sibling1 = sibling1;
    node->sibling2 = sibling2;
    if (node->sibling1 != nullptr)
        node->sibling1->replaceSibling(this, node);
    if (node->sibling2 != nullptr)
        node->sibling2->replaceSibling(this, node);
    while (parent != nullptr && parent->isChildOuter(this))
        parent->replaceOuterChild(this, node);

    parentCNodeId = -1;
    parentPNode = nullptr;
    sibling1 = sibling2 = nullptr;

    OGDF_ASSERT(isValidNode());
    OGDF_ASSERT(node->isValidNode());
    OGDF_ASSERT(parent == nullptr || parent->isValidNode(tree));
}

void PCNode::mergeIntoParent() {
    OGDF_ASSERT(nodeType == PCNodeType::CNode);
    OGDF_ASSERT(isValidNode());
    PCNode *parent = getParent();
    OGDF_ASSERT(parent->nodeType == PCNodeType::CNode);
    OGDF_ASSERT(parent->isValidNode(tree));

    int pcid = tree->parents.link(nodeListIndex, parent->nodeListIndex);
    if (pcid == this->nodeListIndex) {
        std::swap(tree->cNodes[this->nodeListIndex], tree->cNodes[parent->nodeListIndex]);
        std::swap(this->nodeListIndex, parent->nodeListIndex);
    } else {
        OGDF_ASSERT(pcid == parent->nodeListIndex);
    }
    parent->childCount += childCount - 1;

    if (sibling1 != nullptr) {
        sibling1->replaceSibling(this, child1);
        child1->replaceSibling(nullptr, sibling1);
    } else {
        parent->replaceOuterChild(this, child1);
    }
    if (sibling2 != nullptr) {
        sibling2->replaceSibling(this, child2);
        child2->replaceSibling(nullptr, sibling2);
    } else {
        parent->replaceOuterChild(this, child2);
    }

    child1 = child2 = nullptr;
    sibling1 = sibling2 = nullptr;
    parentCNodeId = -1;
    parentPNode = nullptr;
    childCount = 0;

    OGDF_ASSERT(parent->isValidNode(tree));
}

void PCNode::replaceSibling(PCNode *oldS, PCNode *newS) {
    OGDF_ASSERT((newS == nullptr) || (tree == newS->tree));
    OGDF_ASSERT(newS != this);
    if (oldS == sibling1) {
        sibling1 = newS;
    } else {
        OGDF_ASSERT(oldS == sibling2);
        sibling2 = newS;
    }
}

void PCNode::replaceOuterChild(PCNode *oldC, PCNode *newC) {
    OGDF_ASSERT((newC == nullptr) || (tree == newC->tree));
    OGDF_ASSERT(newC != this);
    if (oldC == child1) {
        child1 = newC;
    } else {
        OGDF_ASSERT(oldC == child2);
        child2 = newC;
    }
}

void pc_tree::proceedToNextSibling(PCNode *&pred, PCNode *&curr) {
    pred = curr->getNextSibling(pred);
    std::swap(pred, curr);
}

PCNode *PCNode::getNextSibling(const PCNode *pred) const {
    OGDF_ASSERT(pred == nullptr || isSiblingOf(pred));
    if (pred == sibling1) {
        return sibling2;
    } else {
        OGDF_ASSERT(pred == sibling2);
        return sibling1;
    }
}

PCNode *PCNode::getOtherOuterChild(const PCNode *child) const {
    OGDF_ASSERT(isParentOf(child));
    if (child == child1) {
        return child2;
    } else {
        OGDF_ASSERT(child == child2);
        return child1;
    }
}

void PCNode::proceedToNextNeighbor(PCNode *&pred, PCNode *&curr) const {
    pred = getNextNeighbor(pred, curr);
    std::swap(pred, curr);
}

PCNode *PCNode::getNextNeighbor(const PCNode *pred, const PCNode *curr) const {
    OGDF_ASSERT(curr != nullptr);

    PCNode *next;
    PCNode *parent = getParent();

    if (pred == nullptr) {
        if (curr == parent) {
            next = child1;
        } else {
            OGDF_ASSERT(isParentOf(curr));
            next = (curr->sibling1 != nullptr) ? curr->sibling1 : curr->sibling2;
        }
    } else {
        // find next sibling
        if (pred->isSiblingOf(curr)) {
            OGDF_ASSERT(isParentOf(curr));
            if (pred->isSiblingAdjacent(curr)) {
                next = curr->getNextSibling(pred);
            } else {
                OGDF_ASSERT(isDetached());
                OGDF_ASSERT(isChildOuter(pred));
                OGDF_ASSERT(isChildOuter(curr));
                next = curr->getNextSibling(nullptr);
            }

            // find second or second to last child
        } else if (pred == parent) {
            OGDF_ASSERT(isParentOf(curr));
            next = curr->getNextSibling(nullptr);

            // find first or last child
        } else {
            OGDF_ASSERT(curr == parent);
            OGDF_ASSERT(isParentOf(pred));
            next = getOtherOuterChild(pred);
        }
    }

    if (next == nullptr) {
        if (parent == nullptr) {
            return getOtherOuterChild(curr);
        } else {
            return parent;
        }
    } else {
        OGDF_ASSERT(isParentOf(next));
        return next;
    }
}

bool PCNode::areNeighborsAdjacent(const PCNode *neigh1, const PCNode *neigh2) const {
    OGDF_ASSERT(neigh1 != nullptr);
    OGDF_ASSERT(neigh2 != nullptr);
    OGDF_ASSERT(neigh1 != neigh2);

    if (isParentOf(neigh1) && isParentOf(neigh2)) {
        return neigh1->isSiblingAdjacent(neigh2) || (isDetached() && isChildOuter(neigh1) && isChildOuter(neigh2));

    } else {
        PCNode *parent = getParent();
        if (neigh1 == parent) {
            OGDF_ASSERT(isParentOf(neigh2));
            return isChildOuter(neigh2);

        } else {
            OGDF_ASSERT(neigh2 == parent);
            OGDF_ASSERT(isParentOf(neigh1));
            return isChildOuter(neigh1);
        }
    }
}


bool PCNode::isValidNode(const PCTree *ofTree) const {
    if (ofTree and tree != ofTree)
        return false;
    if (parentCNodeId == -1 && parentPNode == nullptr) {
        OGDF_ASSERT(sibling1 == nullptr && sibling2 == nullptr);
    } else {
        OGDF_ASSERT(parentCNodeId == -1 || parentPNode == nullptr);
        PCNode *parent = getParent();
        OGDF_ASSERT(parent->tree == tree);
        int null_sibs = 0;
        if (sibling1 == nullptr)
            null_sibs++;
        else
            OGDF_ASSERT(sibling1->isSiblingAdjacent(this));
        if (sibling2 == nullptr)
            null_sibs++;
        else
            OGDF_ASSERT(sibling2->isSiblingAdjacent(this));
        int outsides = 0;
        if (parent->child1 == this) outsides++;
        if (parent->child2 == this) outsides++;
        OGDF_ASSERT(null_sibs == outsides);
        OGDF_ASSERT((null_sibs > 0) == isOuterChild());
        if (isOuterChild()) OGDF_ASSERT(parent->isChildOuter(this));
    }

    if (childCount == 0) {
        OGDF_ASSERT(child1 == nullptr);
        OGDF_ASSERT(child2 == nullptr);
    } else if (childCount == 1) {
        OGDF_ASSERT(child1 != nullptr);
        OGDF_ASSERT(child2 != nullptr);
        OGDF_ASSERT(child1 == child2);
    } else {
        OGDF_ASSERT(childCount >= 2);
        OGDF_ASSERT(child1 != nullptr);
        OGDF_ASSERT(child2 != nullptr);
        OGDF_ASSERT(child1 != child2);
    }

    if (nodeType == PCNodeType::CNode) {
        OGDF_ASSERT(tree->cNodes.at(nodeListIndex) == this);
        return tree->parents.find(nodeListIndex) == nodeListIndex;
    } else if (nodeType == PCNodeType::Leaf) {
        OGDF_ASSERT(getDegree() <= 1);
        OGDF_ASSERT(tree->leaves.at(nodeListIndex) == this);
        return true;
    } else {
        OGDF_ASSERT(nodeType == PCNodeType::PNode);
        return true;
    }
}

PCNode *PCNode::getParent() const {
    if (parentPNode != nullptr) {
        OGDF_ASSERT(parentCNodeId < 0);
        OGDF_ASSERT(parentPNode->nodeType == PCNodeType::PNode || parentPNode->nodeType == PCNodeType::Leaf);
        return parentPNode;
    } else if (parentCNodeId >= 0) {
        parentCNodeId = tree->parents.find(parentCNodeId);
        OGDF_ASSERT(tree->cNodes.at(parentCNodeId) != nullptr);
        PCNode *parent = tree->cNodes[parentCNodeId];
        OGDF_ASSERT(parent != this);
        OGDF_ASSERT(parent->nodeType == PCNodeType::CNode);
        OGDF_ASSERT(parent->nodeListIndex == parentCNodeId);
        return parent;
    } else {
        return nullptr;
    }
}

void PCNode::setParent(PCNode *parent) {
    OGDF_ASSERT(isDetached());
    OGDF_ASSERT(parent != nullptr);
    if (parent->nodeType == PCNodeType::CNode) {
        parentCNodeId = parent->nodeListIndex;
    } else {
        parentPNode = parent;
    }
}

void PCNode::checkTimestamp() const {
    OGDF_ASSERT(isValidNode());
    if (tree->timestamp != timestamp) {
        OGDF_ASSERT(tree->timestamp > timestamp);
        temp.clear();
        timestamp = tree->timestamp;
    }
}

std::ostream &operator<<(std::ostream &os, const PCNode &node) {
    return os << &node;
}

std::ostream &operator<<(std::ostream &os, const PCNode *node) {
    if (node == nullptr) return os << "null-Node";
//    if (node->timestamp == node->tree->timestamp)
//        os << node->temp.label << " ";
    os << node->nodeType << " " << node->index();
    os << " with " << node->childCount << " children";
//    if (node->childCount == 1)
//        os << " [" << node->child1->index() << "]";
//    else if (node->childCount == 2)
//        os << " [" << node->child1->index() << ", " << node->child2->index() << "]";
//    else if (node->childCount > 2)
//        os << " [" << node->child1->index() << ", ..., " << node->child2->index() << "]";
    os << " [";
    int c = 0;
    for (PCNode *pred = nullptr, *curr = node->child1; curr != nullptr; proceedToNextSibling(pred, curr)) {
        if (c > 0)
            os << ", ";
        os << curr->index();
        c++;
    }
    os << "]";
    PCNode *parent = node->getParent();
    if (parent != nullptr) {
        os << " and parent ";
//        if (parent->timestamp == parent->tree->timestamp)
//            os << parent->temp.label << " ";
        os << parent->nodeType << " " << parent->index();
    } else {
        os << " and no parent";
    }
    return os;
}

PCNodeChildrenIterable PCNode::children() {
    return PCNodeChildrenIterable(this);
}

PCNodeNeighborsIterable PCNode::neighbors(PCNode *startWith) {
    return PCNodeNeighborsIterable(this, startWith);
}

PCNodeIterator &PCNodeIterator::operator++() {
    node->proceedToNextNeighbor(pred, curr);
    return *this;
}

PCNodeIterator PCNodeIterator::operator++(int) {
    PCNodeIterator before = *this;
    node->proceedToNextNeighbor(pred, curr);
    return before;
}

bool PCNodeIterator::isParent() {
    return node != nullptr && curr != nullptr && curr->isParentOf(node);
}

PCNodeIterator PCNodeChildrenIterable::begin() const noexcept {
    return PCNodeIterator(node, nullptr, node->child1);
}

PCNodeIterator PCNodeChildrenIterable::end() const noexcept {
    if (node->child1 != nullptr && !node->isDetached()) {
        return PCNodeIterator(node, node->child2, node->getParent());
    } else {
        return PCNodeIterator(node, node->child2, node->child1);
    }
}

unsigned long PCNodeChildrenIterable::count() const {
    return node->childCount;
}

PCNodeIterator PCNodeNeighborsIterable::begin() const noexcept {
    return PCNodeIterator(node, nullptr, first);
}

PCNodeIterator PCNodeNeighborsIterable::end() const noexcept {
    PCNode *second = node->getNextNeighbor(nullptr, first);
    PCNode *last = node->getNextNeighbor(second, first);
    return PCNodeIterator(node, last, first);
}

unsigned long PCNodeNeighborsIterable::count() const {
    return node->getDegree();
}

