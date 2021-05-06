#pragma once

namespace pc_tree::hsu {
    class PCNode {
        friend class PCTree;

        friend class NodePCRotation;

        friend class PCArc;

        friend std::ostream &operator<<(std::ostream &, const PCTree &);

    private:

#ifdef OGDF_DEBUG
        const PCTree *tree = nullptr;
#endif

        int id = -1;
        int refCount = 0;
        PCTree::PCNodeType nodeType;
        PCArc *parentArc = nullptr;
        unsigned long degree = 0;

        std::pair<PCTree::NodeLabel, int> label;
        std::pair<PCArc *, int> informedArc;
        std::pair<bool, int> markedLabeling;
        std::pair<bool, int> markedParallelSearch;
        std::pair<PCNode *, int> terminalPathPredecessor1;
        std::pair<PCNode *, int> terminalPathPredecessor2;
        PCArc *fullBlockFirst = nullptr;
        PCArc *fullBlockLast = nullptr;
        PCArc *emptyBlockFirst = nullptr;
        PCArc *emptyBlockLast = nullptr;
        std::vector<PCArc *> fullNeighbors;
        int fullNeighborsTimestamp = 0;

        int listIndex = -1;

        // For intersections
        PCArc *temporaryBlockPointer = nullptr;
        PCArc *oldParentArc = nullptr;

    public:

        /**
         * Returns the index associated with this node.
         * @return The node's index.
         */
        [[nodiscard]] int index() const {
            return id;
        }

        /**
         * Returns the type of this node.
         * @return The node's type.
         */
        [[nodiscard]] PCTree::PCNodeType getNodeType() const {
            return nodeType;
        }

        /**
         * Returns the degree of this node.
         * @return The node's degree.
         */
        [[nodiscard]] unsigned long getDegree() const {
            return degree;
        }

        /**
         * Returns the arc entering the parent of this node, or \code nullptr \endcode, if this node is the root node.
         * @return The parent arc of this node.
         */
        [[nodiscard]] PCArc *getParentArc() const {
            return parentArc;
        }

#ifdef OGDF_DEBUG

        [[nodiscard]] const PCTree *treeOf() const {
            return tree;
        }

#endif

    private:

        /**
         * Creates a new node of a specific type.
         * @param type The new node's type.
         */
        explicit PCNode(PCTree::PCNodeType type) : nodeType(type) {}

        /**
         * Creates a copy of another node.
         * @param other The node to be copied.
         */
        PCNode(const PCNode &other) : nodeType(other.nodeType), degree(other.degree) {}

        void setLabel(PCTree::NodeLabel nodeLabel, int timestamp) {
            label.first = nodeLabel;
            label.second = timestamp;
        }

        void setInformedArc(PCArc *arc, int timestamp) {
            informedArc.first = arc;
            informedArc.second = timestamp;
        }

        void setMarkedLabeling(bool marked, int timestamp) {
            markedLabeling.first = marked;
            markedLabeling.second = timestamp;
        }

        void setMarkedParallelSearch(bool marked, int timestamp) {
            markedParallelSearch.first = marked;
            markedParallelSearch.second = timestamp;
        }

        void setTerminalPathPredecessor1(PCNode *pred, int timestamp) {
            terminalPathPredecessor1.first = pred;
            terminalPathPredecessor1.second = timestamp;
        }

        void setTerminalPathPredecessor2(PCNode *pred, int timestamp) {
            terminalPathPredecessor2.first = pred;
            terminalPathPredecessor2.second = timestamp;
        }

        unsigned long addFullNeighbor(int timestamp, PCArc *fullArc) {
            if (timestamp == fullNeighborsTimestamp) {
                fullNeighbors.push_back(fullArc);
                return fullNeighbors.size();
            } else {
                fullNeighbors.clear();
                fullNeighbors.push_back(fullArc);
                fullNeighborsTimestamp = timestamp;
                return 1;
            }
        }

        std::vector<PCArc *> &getFullNeighbors(int timestamp) {
            if (fullNeighborsTimestamp != timestamp) {
                fullNeighbors.clear();
            }
            return fullNeighbors;
        }

        [[nodiscard]] PCTree::NodeLabel getLabel(int timestamp) const {
            if (timestamp == label.second) {
                return label.first;
            } else {
                return PCTree::NodeLabel::Unknown;
            }
        }

        [[nodiscard]] bool isMarkedLabeling(int timestamp) const {
            if (timestamp == markedLabeling.second) {
                return markedLabeling.first;
            } else {
                return false;
            }
        }

        [[nodiscard]] bool isMarkedParallelSearch(int timestamp) const {
            if (timestamp == markedParallelSearch.second) {
                return markedParallelSearch.first;
            } else {
                return false;
            }
        }

        [[nodiscard]] PCNode *getTerminalPathPredecessor1(int timestamp) const {
            if (timestamp != terminalPathPredecessor1.second) {
                return nullptr;
            } else {
                return terminalPathPredecessor1.first;
            }
        }

        [[nodiscard]] PCNode *getTerminalPathPredecessor2(int timestamp) const {
            if (timestamp != terminalPathPredecessor2.second) {
                return nullptr;
            } else {
                return terminalPathPredecessor2.first;
            }
        }

        [[nodiscard]] PCArc *getInformedArc(int timestamp) const {

            if (timestamp == informedArc.second) {
                return informedArc.first;
            } else {
                return nullptr;
            }
        }

        void swapEnds() {
            PCArc *temp1 = emptyBlockLast;
            emptyBlockLast = emptyBlockFirst;
            emptyBlockFirst = temp1;

            PCArc *temp2 = fullBlockLast;
            fullBlockLast = fullBlockFirst;
            fullBlockFirst = temp2;
        }

        void resetLists() {
            emptyBlockFirst = nullptr;
            emptyBlockLast = nullptr;
            fullBlockFirst = nullptr;
            fullBlockLast = nullptr;
        }

    OGDF_NEW_DELETE
    };
}
