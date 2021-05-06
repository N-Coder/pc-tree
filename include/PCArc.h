#pragma once

namespace pc_tree::hsu {
    class ArcIterator;

    /**
     * Represents one direction of an edge in the PC-tree. Every edge is implemented with two oppositely directed twin arcs.
     */
    class PCArc {
        friend class PCTree;

    private:
#ifdef OGDF_DEBUG
        const PCTree *tree = nullptr;
#endif

        int id = -1;
        PCArc *neighbor2 = nullptr;
        PCArc *neighbor1 = nullptr;
        PCArc *twin = nullptr;
        bool yParent = true;

        std::pair<PCArc *, int> blockPointer;
        std::pair<PCNode *, int> yNode;

        bool preferNeighbor1 = false;

        // For intersections:
        PCArc *blockStart = nullptr;
        PCArc *blockEnd = nullptr;

    public:
        ArcIterator begin();

        ArcIterator end();

        /**
         * Returns the index associated with this arc.
         * @return The arcs's index.
         */
        [[nodiscard]] int index() const {
            return id;
        }

        /**
         * For arc (x, y), returns whether y is the parent of x.
         * @return Whether y is the parent node.
         */
        [[nodiscard]] bool isYParent() const {
            return yParent;
        }

        /**
         * Returns the node this arc points at, or \code nullptr \endcode, if this arc points at a C-node.
         * @return The node this arc points at.
         */
        [[nodiscard]] PCNode *getYNode() const {
            return getyNode(INT_MIN);
        }

        [[nodiscard]] PCTree::PCNodeType getYNodeType() const {
            if (yNode.first == nullptr) {
                return PCTree::PCNodeType::CNode;
            } else {
                return yNode.first->nodeType;
            }
        }

        /**
         * Returns neighbor one of this arc. Note that consistency of neighbors one and two pointers in the cyclic list is
         * not guaranteed. To iterate the cyclic list, use \see getNextArc
         * @return Neighbor one of this arc.
         */
        [[nodiscard]] PCArc *getNeighbor1() const {
            return neighbor1;
        }

        /**
         * Returns neighbor two of this arc. Note that consistency of neighbors one and two pointers in the cyclic list is
         * not guaranteed. To iterate the cyclic list, use \see getNextArc
         * @return Neighbor two of this arc.
         */
        [[nodiscard]] PCArc *getNeighbor2() const {
            return neighbor2;
        }

        /**
         * Returns the next neighbor of this arc in the cyclic order, given the previous arc.
         * @param previous The previous arc in the cyclic order.
         * @return The next arc in the cyclic order.
         */
        [[nodiscard]] PCArc *getNextArc(PCArc *previous) const {
            OGDF_ASSERT(previous && previous->tree == tree);
            OGDF_ASSERT(neighbor1 == previous || neighbor2 == previous);

            if (neighbor1 == previous) {
                return neighbor2;
            } else {
                return neighbor1;
            }
        }

        /**
         * Returns the twin of this arc, i.e., the arc that faces the opposite direction.
         * @return The twin of this arc.
         */
        [[nodiscard]] PCArc *getTwinArc() const {
            return twin;
        }

        [[nodiscard]] bool isAdjacent(PCArc *neighbor) {
            return neighbor1 == neighbor || neighbor2 == neighbor;
        }

#ifdef OGDF_DEBUG

        [[nodiscard]] const PCTree *treeOf() const {
            return tree;
        }

#endif

    private:
        /**
         * Creates a copy of another arc.
         * @param other The arc to be copied.
         */
        PCArc(const PCArc &other) : yParent(other.yParent), id(other.id) {}

        //! Creates a new arc.
        PCArc() = default;

        ~PCArc() {
            PCTree::deleteNodePointer(yNode.first);
        }

        void setBlockPointer(PCArc *arc, int timestamp) {
            blockPointer.first = arc;
            blockPointer.second = timestamp;
        }

        [[nodiscard]] PCNode *getyNode(int timestamp) const {
            if (yNode.second != timestamp &&
                (yNode.first == nullptr || yNode.first->nodeType == PCTree::PCNodeType::CNode)) {
                return nullptr;
            } else {
                return yNode.first;
            }

        }

        void setyNode(PCNode *node, int timestamp) {
            yNode.first = PCTree::assignNodePointer(node, yNode.first);
            yNode.second = timestamp;
        }

        [[nodiscard]] PCArc *getBlockPointer(int timestamp) const {
            if (blockPointer.second == timestamp) {
                return blockPointer.first;
            } else {
                return nullptr;
            }
        }

        void extract() {
            // Extract arc from the circular list, the remaining arcs form the new circular list.
            if (neighbor1 != nullptr) {
                if (neighbor1->neighbor1 == this) {
                    neighbor1->neighbor1 = neighbor2;
                } else if (neighbor1->neighbor2 == this) {
                    neighbor1->neighbor2 = neighbor2;

                }
            }

            if (neighbor2 != nullptr) {
                if (neighbor2->neighbor1 == this) {
                    neighbor2->neighbor1 = neighbor1;
                } else if (neighbor2->neighbor2 == this) {
                    neighbor2->neighbor2 = neighbor1;
                }
            }
        }

    OGDF_NEW_DELETE
    };

    class ArcIterator {
    private :
        PCArc *first;
        PCArc *current;
        PCArc *previous;
        PCArc *next;
        bool start;
        bool end;

    public:
        ArcIterator(PCArc *first, PCArc *current, PCArc *previous, PCArc *next, bool start, bool end) :
                first(first), next(next), start(start), end(end), current(current), previous(previous) {};

        [[nodiscard]] bool valid() const {
            return !start && !end;
        }

        bool operator==(const ArcIterator other) const {
            return first == other.first && current == other.current && start == other.start && end == other.end;
        }

        bool operator!=(const ArcIterator other) const {
            return first != other.first || current != other.current || start != other.start && end != other.end;
        }

        ArcIterator &operator++() {
            previous = current;
            current = next;
            next = current->getNextArc(previous);
            start = false;

            if (current == first) {
                end = true;
            }

            return *this;
        }

        ArcIterator operator++(int) {
            ArcIterator it = *this;
            ++(*this);
            return it;
        }

        ArcIterator &operator--() {
            next = current;
            current = previous;
            previous = current->getNextArc(next);
            end = false;
            if (current == first) {
                start = true;
            }

            return *this;
        }

        ArcIterator operator--(int) {
            ArcIterator it = *this;
            --(*this);
            return it;
        }

        [[nodiscard]] ArcIterator succ() const {
            ArcIterator it = *this;
            ++it;
            return it;
        }

        [[nodiscard]] ArcIterator pred() const {
            ArcIterator it = *this;
            --it;
            return it;
        }

        PCArc *operator*() {
            return current;
        }
    };
}
