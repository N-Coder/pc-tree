#pragma once

#include "utils/RegisteredArray.h"
#include "utils/RegisteredElementSet.h"

#include <ogdf/basic/Graph.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/decomposition/SPQRTree.h>
#include <ogdf/planarity/PlanarityModule.h>

#include <bigInt/bigint.h>

#include <list>
#include <stack>
#include <queue>
#include <memory>
#include <climits>

namespace pc_tree::hsu {
    class PCNode;

    class PCArc;

    class PCTree;
}

std::ostream &operator<<(std::ostream &, const pc_tree::hsu::PCTree &);

namespace pc_tree::hsu {
    using namespace ogdf;
    using namespace Dodecahedron;


    template<class Key>
    class PCTreeRegistry : public RegistryBase<Key, PCTreeRegistry<Key>> {
        PCTree *m_pTree;
        int *m_nextKeyIndex;

    public:
        PCTreeRegistry(PCTree *pcTree, int *nextKeyIndex) : m_pTree(pcTree), m_nextKeyIndex(nextKeyIndex) {}

        bool isKeyAssociated(Key key) const override;

        int keyToIndex(Key key) const override;

        int keyArrayTableSize() const override {
            return RegistryBase<Key, PCTreeRegistry<Key>>::calculateTableSize(*m_nextKeyIndex);
        }

        int maxKeyIndex() const override {
            return (*m_nextKeyIndex) - 1;
        }

        operator PCTree &() const {
            return *m_pTree;
        }

        operator PCTree *() const {
            return m_pTree;
        }

        PCTree &getTree() const {
            return *m_pTree;
        }
    };

    template<typename Value>
    using PCTreeNodeArray = RegisteredArray<PCTreeRegistry<PCNode *>, PCNode *, Value>;

    template<typename Value>
    using PCTreeArcArray = RegisteredArray<PCTreeRegistry<PCArc *>, PCArc *, Value>;

    using PCTreeNodeSet = RegisteredElementSet<PCNode *, PCTreeRegistry<PCNode *>>;

    using PCTreeArcSet = RegisteredElementSet<PCArc *, PCTreeRegistry<PCArc *>>;

    class PCTree {
        friend std::ostream &(::operator<<)(std::ostream &, const PCTree &);

        friend class PCArc;

        friend class PCNode;

    private:
        PCTreeRegistry<PCNode *> nodeArrayRegistry; //! base for the PCTreeNodeArray
        PCTreeRegistry<PCArc *> arcArrayRegistry; //! base for the PCTreeArcArray
        int nextNodeId = 0; //! index for the next created node
        int nextArcId = 0; //! index for the next created node

        std::vector<PCNode *> leaves;

        int timestamp = 0; //! updated after every restriction
        PCArc *tempRootNodeEntryArc = nullptr; //! stores the first arc of the root node. Only valid at timestamp 0
        PCNode *tempRootNode = nullptr; //! stores the root node until arcs are added
        std::queue<PCArc *> nextArcQueue; //! used for BFS in labeling algorithm.
        std::deque<PCNode *> partialNodes;
        std::deque<PCNode *> terminalPath;
        bool invalid = false; //! indicates whether current restriction is possible
        PCNode *apexCandidate = nullptr;
        PCNode *centralCNode = nullptr; //! new central C-node created during the update step


    public:

        operator const PCTreeRegistry<PCNode *> &() const {
            return nodeArrayRegistry;
        }

        operator const PCTreeRegistry<PCArc *> &() const {
            return arcArrayRegistry;
        }

        enum class NodeLabel {
            Unknown,
            Partial,
            Full
        };

        enum class PCNodeType {
            PNode,
            CNode,
            Leaf
        };

        /**
         * Creates a new, trivial PC-tree with \p leafNum leaves.
         * @param leafNum The number of leaves to be created.
         * @param added A list where the leaves will be added.
         */
        explicit PCTree(int leafNum, std::vector<PCNode *> *added = nullptr);

        /**
         * Constructs a copy of \p other.
         * @param other The tree to be copied.
         * @param leafMapping Maps the leaves of \p other to the leaves of the new tree.
         */
        PCTree(const PCTree &other, PCTreeNodeArray<PCNode *> *leafMapping = nullptr);

        /**
        * Constructs a PC-tree from a string.
        * @param str The string.
        */
        explicit PCTree(const std::string &str);

        /**
         * Constructs a PC-tree with no nodes. Nodes can be added with newNode and insertLeaves.
         */
        PCTree() : nodeArrayRegistry(this, &nextNodeId), arcArrayRegistry(this, &nextArcId) {}

        //! Destructor.
        ~PCTree();

        /**
         * Inserts a specific number of leaves as children of node \p parent.
         * @param count The number of leaves to be inserted.
         * @param parent The parent node of the leaves.
         * @param added A list where the new leaves will be added.
         */
        void insertLeaves(int count, PCNode *parent, std::vector<PCNode *> *added = nullptr);

        /**
         * Inserts a new node of the given type. If the parent is nullptr, creates a new root node.
         * @param type The node type of the new node.
         * @param parent The parent node of the new node.
         * @return A pointer to the created node.
         */
        PCNode *newNode(PCNodeType type, PCNode *parent = nullptr);


        /**
         * Removes a leaf from the tree and updates the remaining tree accordingly.
         * @param leaf The leaf to be removed.
         */
        void removeLeaf(PCNode *leaf);

        /**
         * Replaces a leaf with a P-PCNode and \p leafCount leaves.
         * @param leaf The node to be changed.
         * @param leafCount The number of new leaves.
         * @param added Returns the new leaves.
         */
        void replaceLeaf(int leafCount, PCNode *leaf, std::vector<PCNode *> *added = nullptr);

        /**
         * Makes a list of leaves consecutive and merges them to a single leaf.
         * @param consecutiveLeaves The leaves to be merged.
         * @return A pointer to the merged leaf, or nullptr, if the leaves cannot be made consecutive.
         */
        PCNode *mergeLeaves(std::vector<PCNode *> &consecutiveLeaves, bool assumeConsecutive = false);

        /**
         * Attempts to make leaves consecutive. If the restriction is not possible, the tree will not be changed.
         * @param consecutiveLeaves The leaves to be made consecutive.
         * @return Whether the restriction is possible.
         */
        [[nodiscard]] bool makeConsecutive(std::vector<PCNode *> &consecutiveLeaves);

        bool makeConsecutive(std::initializer_list<PCNode *> consecutiveLeaves) {
            std::vector<PCNode *> list(consecutiveLeaves);
            return makeConsecutive(list);
        }


        /**
         * Intersects the tree with another tree. The other tree will be destroyed.
         * @param other The PC-tree whose restrictions will be applied to this.
         * @param mapping Maps the leaves of \p other to the leaves of this.
         */
        [[nodiscard]] bool intersect(PCTree &other, PCTreeNodeArray<PCNode *> &mapping);

        /**
         * Checks if the given order of leaves is valid in this tree.
         * @param order The order of leaves.
         * @return Whether the given order is valid.
         */
        [[nodiscard]] bool isValidOrder(std::vector<PCNode *> &order);

        /**
         * Returns whether this tree is valid, i. e., whether every inner node has degree at least three.
         * @return Whether this tree is valid.
         */
        [[nodiscard]] bool isValid() const;


        /**
         * Creates an OGDF-representation of this tree.
         * @param tree The output Graph.
         * @param g_a GraphAttributes, where information about the nodes (e. g., node type) will be stored.
         * @param representation Maps the node of the PC-tree to the nodes of the OGDF-tree.
         */
        void getTree(Graph &tree, GraphAttributes *g_a, NodeArray<PCNode *> &representation);

        /**
         * Returns a list of all leaves of the tree.
         * @return All leaves of the tree.
         */
        [[nodiscard]] const std::vector<PCNode *> &getLeaves() const {
            return leaves;
        }

        /**
         * Returns the current number of leaves in the tree.
         * @return The number of leaves in the tree.
         */
        [[nodiscard]] int getLeafCount() {
            return leaves.size();
        }

        /**
         * Returns true, if the tree represents all permutations of the leaves. Returns false otherwise.
         * @return Whether the PC-tree is trivial.
         */
        [[nodiscard]] bool isTrivial() const;


        /**
         * Returns the parent of a node, if the parent is known.
         * @param node The node
         * @return The parent node
         */
        [[nodiscard]] PCNode *getParent(const PCNode *node) const;

        /**
         * Computes all restrictions of leaves represented by this tree.
         * @param restrictions The resulting restrictions.
         */
        void getRestrictions(std::vector<std::vector<PCNode *>> &restrictions, PCNode *startLeaf);

        int getTerminalPathLength() {
            return terminalPath.size();
        }

        Bigint possibleOrders();

        int getInnerNodeCount();

        void getNodeStats(int &p_nodes, int &c_nodes, int &sum_p_deg, int &sum_c_deg) const;

        std::ostream &uniqueID(std::ostream &,
                               const std::function<void(std::ostream &os, PCNode *, int)> &printNode,
                               const std::function<bool(PCNode *, PCNode *)> &compareNodes);

        std::vector<PCNode *> currentLeafOrder() const;

    private:

        //! Creates a new PCNode with a given type for this tree.
        PCNode *createNode(PCTree::PCNodeType type);

        //! Creates a new PCNode for this tree as a copy of another PCNode.
        PCNode *createNode(PCNode &other);

        //! Creates a new PCArc for this tree.
        PCArc *createArc();

        //! Creates a new PCArc for this tree as a copy of another PCArc.
        PCArc *createArc(PCArc &other);

        //! Inserts a new node between two adjacent arcs of a node.
        PCNode *insertNode(PCNodeType type, PCArc *adjacent1, PCArc *adjacent2);

        //! Applies a restriction to the tree.
        bool makeConsecutive(std::vector<PCNode *> &consecutiveLeaves, PCArc **fullBlockFirst, PCArc **fullBlockLast,
                             PCArc **emptyBlockFirst, PCArc **emptyBlockLast);

        //! Assigns labels to the nodes of the tree, starting at the given leaves. Can also store the informedArc
        //! of the inner nodes in the order they become full (Helpful for Intersection).
        void assignLabels(std::vector<PCNode *> &fullLeaves, std::vector<PCArc *> *informedArcOrder = nullptr);

        //! Combines a given arc with the blocks of full arcs of the C-node it points to.
        void expandBlocks(PCArc *arc);

        //! Appends an arc to a full block. The other neighbor has to be empty.
        void appendArc(PCArc *arc, PCArc *blockArc, PCArc *otherNeighbor);

        //! Returns a pointer to the common neighbor of two arcs, or nullptr, if no such neighbor exists. Ignores ignoredArc
        //! as a common neighbor.
        static PCArc *getCommonNeighbor(PCArc *arc1, PCArc *arc2, PCArc *ignoredArc);

        //! Finds all nodes on the terminal path and adds them to the list.
        void findTerminalPath();

        //! Finds the first partial predecessor of a given node.
        [[nodiscard]] PCNode *findApex(PCNode *highestNode) const;

        //! Adds all predecessors of a node to the front or the back of the terminal path list.
        unsigned long addToTerminalPath(PCNode *start, bool front);

        //! Declares a node as apex candidate. If multiple apex candidates are found, the restriction is impossible.
        void setApexCandidate(PCNode *node);

        //! Sets a node's predecessor on the terminal path. If a node has two predecessors, it has to be the apex node.
        //! If a node has more than two predecessors, the restriction is impossible.
        void setPredecessor(PCNode *node, PCNode *pred);

        //! Updates the tree to represent the new restriction.
        void updateTerminalPath();

        //! Deletes the edge between two nodes on the terminal path.
        void deleteEdge(PCNode *previous, PCNode *current);

        //! Determines the ends of blocks at a C-node, given an arc on the terminal path.
        void findBlocks(PCNode *node, PCArc *entryArc);

        //! Creates the new edge between the given P-node and the central C-PCNode, returns the arc to be added to the C-PCNode.
        PCArc *preparePNode(PCNode *pNode, bool isParent, unsigned long degree);

        //! Deletes a redundant node of degree two.
        void deleteDegreeTwoNode(PCArc *incoming1);

        //! Returns whether the given arc entering the given node is on the terminal path. Only works for non-terminal
        //! nodes on the terminal path.
        bool isTerminalPathArc(PCNode *node, PCArc *arc);

        //! Splits a P-PCNode on the terminal path and adds both new nodes to the central C-node.
        void splitPNode(PCNode *node);

        //! Splits a C-PCNode on the terminal path and directly adds its blocks to the central C-node.
        void splitCNode(PCNode *node);

        //! Appends the Start of one list of arcs to the end of another. The ends of a list are given by nullptr neighbors.
        static void combineLists(PCArc *listBegin, PCArc *listEnd);

        //! Adds a list of arcs to the full or empty side of the central C-node.
        void addToCentralCNode(PCArc *firstArc, PCArc *lastArc, bool full);

        //! Returns the parent of the given node on the terminal path, or nullptr, if the parent cannot be on the terminal path.
        PCNode *getTerminalPathParent(PCNode *node);

        //! Returns whether the given arc enters from a full node.
        bool isFullArc(PCArc *arc) const;

        //! Sets the neighbor of the given arc to nullptr. Useful for extracting and combining lists of arcs.
        static void setNeighborNull(PCArc *arc, PCArc *nullNeighbor);

        //! Updates the two given pointers to the next two arcs in the list.
        static void getNextArc(PCArc *&previous, PCArc *&current);

        //! Returns a pointer to the following arc in the list.
        static PCArc *getNextArc2(PCArc *previous, PCArc *current);

        //! Finds all restrictions in this tree and applies them to another tree.
        bool findNodeRestrictions(PCTree &applyTo, PCTreeNodeArray<PCNode *> &mapping);

        //! Prints the tree.
        std::ostream &printNodes(std::ostream &os) const;

        //! Restores the tree by replacing all merged leaves with the subtree they represent.
        void restoreSubtrees(std::vector<PCNode *> &oldLeaves);

        //! Creates a new owning pointer to a C-node, i. e., increases the reference count of the node.
        static PCNode *assignNodePointer(PCNode *newPointer, PCNode *oldPointer = nullptr);

        //! Deletes an owning pointer of a node. If the pointer is the last pointer, the node is deleted.
        static void deleteNodePointer(PCNode *pointer);

        //! Removes a leaf from the list of the tree's leaves.
        void removeLeafFromList(PCNode *leaf);

        //! Inserts a leaf into the list of the tree's leaves.
        void insertLeafIntoList(PCNode *leaf);

        //! Assigns the next available index to a node.
        void setIndex(PCNode *node);

        //! Assigns a specific index to a node.
        void setIndex(PCNode *node, int index);

        //! Assigns the next available index to an arc.
        void setIndex(PCArc *arc);

        PCArc *findRootEntryArc() const;

    };
}
