#pragma once

#include "PCEnum.h"
#include "PCRegistry.h"
#include "PCNode.h"
#include "PCTreeIterators.h"

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/basic/DisjointSets.h>

#include <bigInt/bigint.h>

#include <vector>
#include <list>
#include <sstream>

// #define PCTREE_REUSE_NODES

namespace pc_tree {
    bool isTrivialRestriction(int restSize, int leafCount);

    namespace uid_utils {
        void nodeToID(std::ostream &os, PCNode *n, int pos);

        void nodeToPosition(std::ostream &os, PCNode *n, int pos);

        void leafToID(std::ostream &os, PCNode *n, int pos);

        void leafToPosition(std::ostream &os, PCNode *n, int pos);

        bool compareNodesByID(PCNode *a, PCNode *b);
    }

    class PCTree {
        using DisjointSets = ogdf::DisjointSets<>;

        friend std::ostream &(::operator<<)(std::ostream &, const pc_tree::PCTree *);

        friend std::ostream &(::operator<<)(std::ostream &, const pc_tree::PCNode *);

        friend class PCNode;

    private:
        PCTreeRegistry<PCNode *> nodeArrayRegistry;
        int nextNodeId = 0;
        int pNodeCount = 0;
        int cNodeCount = 0;
        std::vector<PCNode *> cNodes;
        std::vector<PCNode *> leaves;
        PCNode *rootNode = nullptr;
        mutable DisjointSets parents;
        int timestamp = 0;

        PCNode *firstPartial = nullptr;
        PCNode *lastPartial = nullptr;
        int partialCount = 0;
        PCNode *apexCandidate = nullptr;
        bool apexCandidateIsFix = false;
        int terminalPathLength = 0;
        PCNode *apexTPPred2 = nullptr;
        std::vector<PCNode *> c_neighbors;

#ifdef PCTREE_REUSE_NODES
        std::vector<PCNode *> reusableNodes;
#endif

    public:

        explicit PCTree() : nodeArrayRegistry(this, &nextNodeId) {};

        explicit PCTree(int leafNum, std::vector<PCNode *> *added = nullptr);

        explicit PCTree(const PCTree &other, PCTreeNodeArray<PCNode *> &nodeMapping, bool keep_ids = false);

        explicit PCTree(const std::string &str, bool keep_ids = false);

        virtual ~PCTree();

        PCNode *newNode(PCNodeType type, PCNode *parent = nullptr, int id = -1);

        void destroyNode(PCNode *&node) {
            destroyNode((PCNode *const &) node);
            node = nullptr;
        }

        void destroyNode(PCNode *const &node);

        void insertLeaves(int count, PCNode *parent, std::vector<PCNode *> *added = nullptr);

        void replaceLeaf(int leafCount, PCNode *leaf, std::vector<PCNode *> *added = nullptr);

        PCNode *mergeLeaves(std::vector<PCNode *> &consecutiveLeaves, bool assumeConsecutive = false);

        bool makeConsecutive(std::initializer_list<PCNode *> consecutiveLeaves) {
            std::vector<PCNode *> list(consecutiveLeaves);
            return makeConsecutive(list);
        }

        bool isTrivialRestriction(std::vector<PCNode *> &consecutiveLeaves) const;

        bool isTrivialRestriction(int size) const;

        bool makeConsecutive(std::vector<PCNode *> &consecutiveLeaves);

        bool intersect(PCTree &other, PCTreeNodeArray<PCNode *> &mapping);

        PCNode *setRoot(PCNode *newRoot);

        PCNode *changeRoot(PCNode *newRoot);

        PCNodeType changeNodeType(PCNode *node, PCNodeType newType);

    public:

        operator const PCTreeRegistry<PCNode *> &() const { return nodeArrayRegistry; }

        bool isTrivial() const;

        const std::vector<PCNode *> &getLeaves() const { return leaves; }

        int getLeafCount() const { return leaves.size(); };

        int getPNodeCount() const { return pNodeCount; }

        int getCNodeCount() const { return cNodeCount; }

        PCNode *getRootNode() const { return rootNode; }

        int getTerminalPathLength() const { return terminalPathLength; }

        FilteringPCTreeDFS allNodes() const {
            return FilteringPCTreeDFS(*this, rootNode);
        }

        FilteringPCTreeDFS innerNodes() const {
            return FilteringPCTreeDFS(*this, rootNode, [](PCNode *node) {
                return !node->isLeaf();
            });
        }

        template<typename Container>
        void currentLeafOrder(Container &container) const {
            for (PCNode *leaf: FilteringPCTreeDFS(*this, rootNode))
                if (leaf->isLeaf())
                    container.push_back(leaf);
            OGDF_ASSERT(container.size() == leaves.size());
        }

        std::vector<PCNode *> currentLeafOrder() const {
            std::vector<PCNode *> container;
            currentLeafOrder(container);
            return container;
        }

        bool checkValid() const;

        bool isValidOrder(std::vector<PCNode *> &order) const;

        void getTree(ogdf::Graph &tree, ogdf::GraphAttributes *g_a, PCTreeNodeArray<ogdf::node> &pc_repr,
                     ogdf::NodeArray<PCNode *> *g_repr = nullptr, bool mark_full = false) const;

        void getTree(ogdf::Graph &tree, ogdf::GraphAttributes *g_a, PCTreeNodeArray<ogdf::node> &pc_repr,
                     ogdf::NodeArray<PCNode *> *g_repr, std::vector<PCNode *> full_leaves);

        void getRestrictions(std::vector<std::vector<PCNode *>> &restrictions, PCNode *fixedLeaf = nullptr) const;

        Dodecahedron::Bigint possibleOrders() const;

        std::ostream &uniqueID(std::ostream &,
                               const std::function<void(std::ostream &os, PCNode *, int)> &printNode = uid_utils::nodeToID,
                               const std::function<bool(PCNode *, PCNode *)> &compareNodes = uid_utils::compareNodesByID);

        std::string uniqueID(const std::function<void(std::ostream &os, PCNode *, int)> &printNode = uid_utils::nodeToID,
                             const std::function<bool(PCNode *, PCNode *)> &compareNodes = uid_utils::compareNodesByID) {
            std::stringstream sb;
            uniqueID(sb, printNode);
            return sb.str();
        }

    private:

        void unregisterNode(PCNode *node);

        void registerNode(PCNode *node);

        void assignLabels(std::vector<PCNode *> &fullLeaves, std::vector<PCNode *> *fullNodeOrder = nullptr);

        bool findTerminalPath();

        PCNode *createCentralNode();

        int updateTerminalPath(PCNode *central, PCNode *tpNeigh);

        void addPartialNode(PCNode *partial);

        void removePartialNode(PCNode *partial);

        void replaceTPNeigh(PCNode *central, PCNode *oldTPNeigh, PCNode *newTPNeigh, PCNode *newFullNeigh, PCNode *otherEndOfFullBlock);

        bool checkTPPartialCNode(PCNode *node);

        int findEndOfFullBlock(PCNode *node, PCNode *pred, PCNode *curr, PCNode *&fullEnd, PCNode *&emptyEnd) const;

        bool setApexCandidate(PCNode *ac, bool fix = false);

        void updateSingletonTerminalPath();

        void appendNeighbor(PCNode *central, std::vector<PCNode *> &neighbors, PCNode *append, bool is_parent = false);

        PCNode *splitOffFullPNode(PCNode *node, bool skip_parent);

        bool findNodeRestrictions(PCTree &applyTo, PCTreeNodeArray<PCNode *> &mapping,
                                  PCTreeNodeArray<std::vector<PCNode *>> &blockNodes,
                                  PCTreeNodeArray<std::vector<PCNode *>> &subtreeNodes,
                                  PCTreeNodeArray<PCNode *> &leafPartner,
                                  PCTreeNodeArray<bool> &isFront);

        void restoreSubtrees(PCTreeNodeArray<std::vector<PCNode *>> &blockNodes,
                             PCTreeNodeArray<std::vector<PCNode *>> &subtreeNodes,
                             PCTreeNodeArray<PCNode *> &leafPartner,
                             PCTreeNodeArray<bool> &isFront);

    public:
        struct Observer {
            enum class Stage {
                Trivial, NoPartials, InvalidTP, SingletonTP, Done
            };

            virtual void makeConsecutiveCalled(PCTree &tree, const std::vector<PCNode *> &consecutiveLeaves) {};

            virtual void labelsAssigned(PCTree &tree, PCNode *firstPartial, PCNode *lastPartial, int partialCount) {};

            virtual void terminalPathFound(PCTree &tree, PCNode *apex, PCNode *apexTPPred2, int terminalPathLength) {};

            virtual void centralCreated(PCTree &tree, PCNode *central) {};

            virtual void beforeMerge(PCTree &tree, int count, PCNode *tpNeigh) {};

            virtual void afterMerge(PCTree &tree, PCNode *successor) {};

            virtual void fullNodeSplit(PCTree &tree, PCNode *fullNode) {};

            virtual void makeConsecutiveDone(PCTree &tree, Stage stage, bool success) {};
        };

        struct LoggingObserver : public Observer {
            void makeConsecutiveCalled(PCTree &tree, const std::vector<PCNode *> &consecutiveLeaves) override;

            void labelsAssigned(PCTree &tree, PCNode *firstPartial, PCNode *lastPartial, int partialCount) override;

            void terminalPathFound(PCTree &tree, PCNode *apex, PCNode *apexTPPred2, int terminalPathLength) override;

            void centralCreated(PCTree &tree, PCNode *central) override;

            void beforeMerge(PCTree &tree, int count, PCNode *tpNeigh) override;

            void makeConsecutiveDone(PCTree &tree, Stage stage, bool success) override;
        };

    private:
        std::list<Observer *> observers;

    public:
        std::list<Observer *>::const_iterator addObserver(Observer *observer) {
            observers.push_back(observer);
            return --observers.end();
        }

        void removeObserver(std::list<Observer *>::const_iterator it) {
            observers.erase(it);
        }

        void removeObserver(Observer *observer) {
            observers.remove(observer);
        }
    };
}
