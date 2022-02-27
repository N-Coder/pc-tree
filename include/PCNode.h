#pragma once

#include "PCEnum.h"

#include <ogdf/basic/basic.h>
#include <ogdf/basic/memory.h>

#include <vector>
#include <list>

namespace pc_tree {
    struct PCNodeChildrenIterable;
    struct PCNodeNeighborsIterable;

    class PCNode {
        friend class PCTree;

        friend struct PCNodeChildrenIterable;
        friend struct PCNodeNeighborsIterable;

        friend std::ostream &(::operator<<)(std::ostream &, const pc_tree::PCTree *);

        friend std::ostream &(::operator<<)(std::ostream &, const pc_tree::PCNode *);

        struct TempInfo {
            NodeLabel label = NodeLabel::Unknown;
            PCNode *predPartial = nullptr, *nextPartial = nullptr;
            PCNode *tpPred = nullptr;
            PCNode *tpPartialPred = nullptr;
            int tpPartialHeight = 0;
            PCNode *tpSucc = nullptr;
            std::vector<PCNode *> fullNeighbors;
            PCNode *ebEnd1 = nullptr, *fbEnd1 = nullptr,
                    *fbEnd2 = nullptr, *ebEnd2 = nullptr;

            void replaceNeighbor(PCNode *oldNeigh, PCNode *newNeigh) {
                if (tpPred == oldNeigh) tpPred = newNeigh;
                if (tpPartialPred == oldNeigh) tpPartialPred = newNeigh;
                if (tpSucc == oldNeigh) tpSucc = newNeigh;
                if (ebEnd1 == oldNeigh) ebEnd1 = newNeigh;
                if (ebEnd2 == oldNeigh) ebEnd2 = newNeigh;
                if (fbEnd1 == oldNeigh) fbEnd1 = newNeigh;
                if (fbEnd2 == oldNeigh) fbEnd2 = newNeigh;
            }

            void clear() {
                label = NodeLabel::Unknown;
                nextPartial = predPartial = nullptr;
                tpPred = tpPartialPred = tpSucc = nullptr;
                ebEnd1 = fbEnd1 = fbEnd2 = ebEnd2 = nullptr;
                tpPartialHeight = 0;
                fullNeighbors.clear();
            }
        };

    private:
        PCTree *tree = nullptr;

        int id;
        int nodeListIndex = -1;
        PCNodeType nodeType;
        PCNode *parentPNode = nullptr;
        mutable int parentCNodeId = -1;
        PCNode *sibling1 = nullptr;
        PCNode *sibling2 = nullptr;
        PCNode *child1 = nullptr;
        PCNode *child2 = nullptr;
        int childCount = 0;

        mutable TempInfo temp;
        mutable int timestamp = 0;

        PCNode(PCTree *tree, int id, PCNodeType nodeType)
                : tree(tree), id(id), nodeType(nodeType) {}

    public:
        void appendChild(PCNode *node, bool begin = false);

        void insertBetween(PCNode *sib1, PCNode *sib2);

        void detach();

        void replaceWith(PCNode *node);

        void mergeIntoParent();

        void flip() {
            std::swap(child1, child2);
        }

    private:
        void replaceSibling(PCNode *oldS, PCNode *newS);

        void replaceOuterChild(PCNode *oldC, PCNode *newC);

        void setParent(PCNode *parent);

        void forceDetach();

    public:
        PCNode *getNextSibling(const PCNode *pred) const;

        PCNode *getOtherOuterChild(const PCNode *child) const;

        PCNode *getNextNeighbor(const PCNode *pred, const PCNode *curr) const;

        void proceedToNextNeighbor(PCNode *&pred, PCNode *&curr) const;

        PCNode *getParent() const;

        PCNodeChildrenIterable children();

        PCNodeNeighborsIterable neighbors(PCNode *first = nullptr);

    public:
        bool isDetached() const {
            if (parentCNodeId == -1 && parentPNode == nullptr) {
                return true;
            } else {
                OGDF_ASSERT(parentCNodeId == -1 || parentPNode == nullptr);
                return false;
            }
        }

        bool isValidNode(const PCTree *ofTree = nullptr) const;

        bool isLeaf() const {
            return nodeType == PCNodeType::Leaf;
        }

        bool isParentOf(const PCNode *other) const {
            OGDF_ASSERT(other != nullptr);
            OGDF_ASSERT(tree == other->tree);
            return other->getParent() == this;
        }

        bool isSiblingOf(const PCNode *other) const {
            OGDF_ASSERT(other != nullptr);
            OGDF_ASSERT(tree == other->tree);
            return this->getParent() == other->getParent();
        }

        bool isSiblingAdjacent(const PCNode *sibling) const {
            OGDF_ASSERT(isSiblingOf(sibling));
            OGDF_ASSERT(this != sibling);
            return sibling1 == sibling || sibling2 == sibling;
        }

        bool areNeighborsAdjacent(const PCNode *neigh1, const PCNode *neigh2) const;

        bool isChildOuter(const PCNode *child) const {
            OGDF_ASSERT(isParentOf(child));
            return child1 == child || child2 == child;
        }

        bool isOuterChild() const {
            return sibling1 == nullptr || sibling2 == nullptr;
        }

    public:
        const TempInfo &constTempInfo() const {
            checkTimestamp();
            return temp;
        }

        PCNode *getFullNeighInsertionPointConst(PCNode *nonFullNeigh) {
            return getFullNeighInsertionPoint(nonFullNeigh);
        }

    private:
        void checkTimestamp() const;

        TempInfo &tempInfo() {
            checkTimestamp();
            return temp;
        }

        int addFullNeighbor(PCNode *fullNeigh) {
            checkTimestamp();
            OGDF_ASSERT(fullNeigh->tempInfo().label == NodeLabel::Full);
            temp.fullNeighbors.push_back(fullNeigh);
            return temp.fullNeighbors.size();
        }

        PCNode *&getFullNeighInsertionPoint(PCNode *nonFullNeigh) {
            checkTimestamp();
            OGDF_ASSERT(nonFullNeigh != nullptr);
            if (nonFullNeigh == temp.ebEnd1) {
                OGDF_ASSERT(areNeighborsAdjacent(temp.ebEnd1, temp.fbEnd1));
                return temp.fbEnd1;
            } else {
                OGDF_ASSERT(nonFullNeigh == temp.ebEnd2);
                OGDF_ASSERT(areNeighborsAdjacent(temp.ebEnd2, temp.fbEnd2));
                return temp.fbEnd2;
            }
        }

    public:
        const PCTree *treeOf() const { return tree; }

        int index() const { return id; }

        PCNodeType getNodeType() const { return nodeType; }

        int getChildCount() const { return childCount; }

        int getDegree() const { return isDetached() ? childCount : childCount + 1; }

        PCNode *getChild1() const { return child1; }

        PCNode *getChild2() const { return child2; }

        PCNode *getSibling1() const { return sibling1; }

        PCNode *getSibling2() const { return sibling2; }

    OGDF_NEW_DELETE
    };

    void proceedToNextSibling(PCNode *&pred, PCNode *&curr);
}