#include "PCTree.h"
#include "PCNode.h"

#include <ogdf/misclayout/CircularLayout.h>
#include <ogdf/tree/TreeLayout.h>
#include <ogdf/fileformats/GraphIO.h>

#include <ogdf/basic/basic.h>
#include <bandit/bandit.h>

#include <memory>
#include <ostream>

using namespace pc_tree;
using namespace snowhouse;
using namespace bandit;
using namespace ogdf;
using namespace Dodecahedron;

struct CentralNode {
    NodeLabel parentLabel; // partial is interpreted as nullptr
    int fullNeighbors; // 0, 1 or 2+
    int partialNeighbors; // 0, 1, 2 or 3+ (invalid)
    int emptyNeighbors; // 0, 1 or 2+
    int seed;

    CentralNode(NodeLabel parentLabel, int fullNeighbors, int partialNeighbors, int emptyNeighbors, int seed)
            : parentLabel(parentLabel), fullNeighbors(fullNeighbors), partialNeighbors(partialNeighbors), emptyNeighbors(emptyNeighbors), seed(seed) {}

    friend ostream &operator<<(ostream &os, const CentralNode &aCase) {
        os << "parentLabel: " << aCase.parentLabel
           << ", full: " << aCase.fullNeighbors
           << ", partial: " << aCase.partialNeighbors
           << ", empty: " << aCase.emptyNeighbors
           << ", seed: " << aCase.seed;
        return os;
    }

    std::string toString() const {
        std::stringstream sb;
        sb << *this;
        return sb.str();
    }
};


void testUIDRegen(PCTree &T) {
    std::string uid = T.uniqueID(uid_utils::leafToID);

    std::vector<std::vector<PCNode *>> restrictions;
    int rand = randomNumber(0, T.getLeafCount());
    if (rand == T.getLeafCount()) {
        T.getRestrictions(restrictions);
    } else {
        auto it = T.getLeaves().begin();
        for (int i = 0; i < rand; i++) {
            it++;
        }
        T.getRestrictions(restrictions, *it);
    }

    PCTreeNodeArray<PCNode *> nodeMapping(T, nullptr);
    PCTree cT;
    PCNode *root = cT.newNode(PCNodeType::PNode);
    for (PCNode *leaf: T.getLeaves()) {
        OGDF_ASSERT(leaf->index() != 0);
        nodeMapping[leaf] = cT.newNode(PCNodeType::Leaf, root, leaf->index());
    }

    for (auto restriction : restrictions) {
        for (auto &n : restriction) {
            n = nodeMapping[n];
        }
        bool res = cT.makeConsecutive(restriction);
        AssertThat(res, Equals(true));
    }

    AssertThat(cT.uniqueID(uid_utils::leafToID), Equals(uid));
}

struct CreateCentralNode {
    CentralNode central;
    std::unique_ptr<PCTree> T;
    std::vector<PCNode *> fullLeaves;
    std::vector<PCNode *> emptyLeaves;
    Bigint orders = 1;
    int depth = 0;

    explicit CreateCentralNode(const CentralNode &central) : central(central) {}

    static void declareTestConsecutive(const CentralNode &central) {
        it("restriction correctly handles CentralNode(" + central.toString() + ")", [central]() {
            CreateCentralNode create(central);
            create.testConsecutive();
        });
    }

    void testConsecutive() {
        T = std::make_unique<PCTree>();
        createTree();
        bool possible = T->makeConsecutive(fullLeaves);
        AssertThat(possible, Equals(true));
        AssertThat(T->checkValid(), Equals(true));
        AssertThat(T->possibleOrders(), Equals(orders));

        AssertThat(T->makeConsecutive(emptyLeaves), Equals(true));
        AssertThat(T->checkValid(), Equals(true));
        AssertThat(T->possibleOrders(), Equals(orders));
        T.reset(); // catch exceptions from destructor
    }

    static void declareTestRegenerate(const CentralNode &central) {
        it("UID stays constant when regenerating CentralNode(" + central.toString() + ") from exported restrictions", [central]() {
            CreateCentralNode create(central);
            create.testRegenerate();
        });
    }

    void testRegenerate() {
        T = std::make_unique<PCTree>();
        createTree();
        testUIDRegen(*T);
        if (T->makeConsecutive(fullLeaves))
            testUIDRegen(*T);
        T.reset(); // catch exceptions from destructor
    }

    static void declareTestIntersection(const CentralNode &central) {
        it("intersects two trees correctly", [central]() {
            CreateCentralNode create(central);
            create.testIntersection();
        });
    }

    void testIntersection() {
        T = std::make_unique<PCTree>();
        createTree();

        PCTree t1(T->getLeafCount());
        PCTreeNodeArray<PCNode *> leafMap1(*T);
        PCTree t2(T->getLeafCount());
        PCTreeNodeArray<PCNode *> leafMap2(*T);
        PCTreeNodeArray<PCNode *> leafMap21(t2);

        for (int i = 0; i < T->getLeafCount(); ++i) {
            PCNode *original = T->getLeaves().at(i);
            leafMap1[original] = t1.getLeaves().at(i);
            leafMap2[original] = t2.getLeaves().at(i);
            leafMap21[t2.getLeaves().at(i)] = t1.getLeaves().at(i);
        }

        std::vector<std::vector<PCNode *>> restrictions;
        T->getRestrictions(restrictions);
        auto rnd = std::random_device{};
        auto rng = std::default_random_engine{rnd()};
        std::shuffle(std::begin(restrictions), std::end(restrictions), rng);

        for (int i = 0; i < restrictions.size(); ++i) {
            if (i % 2 == 0) {
                copyRestriction(restrictions.at(i), leafMap1, t1);
            } else {
                copyRestriction(restrictions.at(i), leafMap2, t2);
            }
        }
        OGDF_ASSERT(t1.checkValid());
        OGDF_ASSERT(t2.checkValid());

        AssertThat(t1.intersect(t2, leafMap21), IsTrue());
        AssertThat(t1.checkValid(), IsTrue());
        std::string uidOriginal = T->uniqueID([&](std::ostream &os, PCNode *n, int pos) {
            if (n->isLeaf()) os << leafMap1[n]->index();
        });
        std::string uidIntersection = t1.uniqueID(uid_utils::leafToID);

        AssertThat(uidIntersection, Equals(uidOriginal));

        T.reset();
    }

    static void copyRestriction(const std::vector<PCNode *> &originalRestriction,
                                const PCTreeNodeArray<PCNode *> &leafMap, PCTree &tree) {
        std::vector<PCNode *> copyRestriction;
        copyRestriction.reserve(originalRestriction.size());
        for (PCNode *n : originalRestriction) {
            copyRestriction.push_back(leafMap[n]);
        }
        AssertThat(tree.makeConsecutive(copyRestriction), IsTrue());
    }

    void dump(const std::string &name) {
        Graph G;
        GraphAttributes GA(G, GraphAttributes::all);

        PCTreeNodeArray<ogdf::node> pc_repr(*T, nullptr);
        T->getTree(G, &GA, pc_repr, nullptr, fullLeaves);

        CircularLayout cl;
        cl.call(GA);
//        GraphIO::write(GA, name + "-cl.svg");

        TreeLayout tl;
        G.reverseAllEdges();
        tl.call(GA);
//        GraphIO::write(GA, name + "-tl.svg");
    }

    void createTree() {
        setSeed(central.seed);
        PCNode *emptyNode = nullptr, *fullNode = nullptr, *partialNode = nullptr;
        PCNode *apex = createNode(
                PCNodeType::PNode,
                central.fullNeighbors, central.partialNeighbors, central.emptyNeighbors,
                fullNode, partialNode, emptyNode);

        if (central.parentLabel == NodeLabel::Full) {
            if (fullNode != nullptr) {
                fullNode->detach();
                fullNode->appendChild(apex);
                T->setRoot(moveUpRoot(fullNode));
            } else {
                T->setRoot(apex);
            }
        } else if (central.parentLabel == NodeLabel::Empty) {
            if (emptyNode != nullptr) {
                emptyNode->detach();
                emptyNode->appendChild(apex);
                T->setRoot(moveUpRoot(emptyNode));
            } else {
                T->setRoot(apex);
            }
        } else {
            T->setRoot(apex);
        }
        if (central.partialNeighbors != 0) {
            // we'll create a new central C-Node
            orders *= 2;
        }
        OGDF_ASSERT(T->checkValid());
    }

    static PCNode *moveUpRoot(PCNode *node) {
        while (randomNumber(0, 99) < 75 && node->getChildCount() > 0) {
            PCNode *child = node->getChild1();
            //if (child->getNodeType() == PCNodeType::Leaf) break;
            child->detach();
            child->appendChild(node);
            node = child;
        }
        return node;
    }

    PCNode *createNode(NodeLabel label) {
        if (randomNumber(0, 2) <= depth && label != NodeLabel::Partial) {
            PCNode *node = T->newNode(PCNodeType::Leaf);
            if (label == NodeLabel::Full) {
                fullLeaves.push_back(node);
            } else {
                OGDF_ASSERT(label == NodeLabel::Empty);
                emptyLeaves.push_back(node);
            }
            return node;
        } else {
            return createNode((randomNumber(0, 1) == 1 ? PCNodeType::PNode : PCNodeType::CNode), label);
        }
    }

    PCNode *createNode(PCNodeType type, NodeLabel label) {
        int full = 0, empty = 0, partial = 0;
        if (label == NodeLabel::Partial) {
            partial = randomNumber(0, 99) < 75 - (depth * 10) ? 1 : 0;
            full = randomNumber(1 - partial, 5);
            if (full == 0)
                empty = randomNumber(1, 5);
            else
                empty = randomNumber(1 - partial, 5);
        } else if (label == NodeLabel::Full) {
            full = randomNumber(2, 5);
        } else {
            empty = randomNumber(2, 5);
        }
        return createNode(type, full, partial, empty);
    }

    PCNode *createNode(PCNodeType type, int full, int partial, int empty) {
        PCNode *fullChild, *emptyChild, *partialChild;
        return createNode(type, full, partial, empty, fullChild, partialChild, emptyChild);
    }

    PCNode *createNode(PCNodeType type, int full, int partial, int empty, PCNode *&fullChild, PCNode *&partialChild, PCNode *&emptyChild) {
        OGDF_ASSERT(type != PCNodeType::Leaf);
        PCNode *node = T->newNode(type);
        if (type == PCNodeType::PNode) {
            orders *= factorial(full);
            orders *= factorial(empty);
        }
        if (type == PCNodeType::CNode && partial == 0 && (full == 0 || empty == 0)) {
            orders *= 2;
        }
        int sum = full + partial + empty;
        OGDF_ASSERT(sum >= 2);
        depth++;
        while (full + partial + empty > 0) {
            int rand = type == PCNodeType::CNode ? 0 : randomNumber(0, full + partial + empty - 1);
            if (rand < full) {
                PCNode *child = createNode(NodeLabel::Full);
                node->appendChild(child);
                if (child->getNodeType() != PCNodeType::Leaf)
                    fullChild = child;
                full--;
            } else if (rand < full + partial) {
                PCNode *child = createNode(NodeLabel::Partial);
                node->appendChild(child);
                if (child->getNodeType() != PCNodeType::Leaf)
                    partialChild = child;
                partial--;
            } else {
                OGDF_ASSERT(rand < full + partial + empty);
                PCNode *child = createNode(NodeLabel::Empty);
                node->appendChild(child);
                if (child->getNodeType() != PCNodeType::Leaf)
                    emptyChild = child;
                empty--;
            }
        }
        depth--;
        OGDF_ASSERT(full == 0);
        OGDF_ASSERT(partial == 0);
        OGDF_ASSERT(empty == 0);
        OGDF_ASSERT(node->getDegree() == sum);
        if (randomNumber(0, 1) == 1)
            node->flip();
        return node;
    }
};

go_bandit([]() {
    describe("PCTree", []() {
        CreateCentralNode::declareTestRegenerate(CentralNode(
                NodeLabel::Empty, 4, 1, 2, 5
        ));

        std::time_t time = std::time(nullptr);
        std::vector<int> seeds;
        for (int i = 0; i < 10; i++) {
            seeds.push_back(i);
            seeds.push_back(time + i);
        }
        std::list<CentralNode> centrals;
        for (NodeLabel parentLabel: std::vector<NodeLabel>{NodeLabel::Full, NodeLabel::Partial, NodeLabel::Empty})
            for (int fullNeighbors = 0; fullNeighbors <= 4; fullNeighbors++)
                for (int partialNeighbors = 0; partialNeighbors <= 2; partialNeighbors++)
                    for (int emptyNeighbors = 0; emptyNeighbors <= 4; emptyNeighbors++) {
                        if (fullNeighbors + partialNeighbors + emptyNeighbors <= 2) continue;
                        if (partialNeighbors <= 1 && (fullNeighbors == 0 || emptyNeighbors == 0)) continue;
                        for (int seed : seeds)
                            centrals.emplace_back(parentLabel, fullNeighbors, partialNeighbors, emptyNeighbors, seed);
                    }
        for (CentralNode &central : centrals) {
            CreateCentralNode::declareTestConsecutive(central);
            CreateCentralNode::declareTestRegenerate(central);
            CreateCentralNode::declareTestIntersection(central);
        }
    });
});

int main(int argc, char *argv[]) {
    return bandit::run(argc, argv);
}
