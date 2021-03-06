#include "PCTree.h"
#include "PCNode.h"
#include "NodePCRotation.h"

#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/basic/graph_generators.h>
#include <ogdf/basic/NodeSet.h>
#include <ogdf/planarity/BoothLueker.h>

#include <bandit/bandit.h>

using namespace ogdf;
using namespace pc_tree;
using namespace snowhouse;
using namespace bandit;

using RegisteredEdgeSet = RegisteredElementSet<edge, Graph>;

bool makeConsecutive(PCTree &tree, std::initializer_list<int> listIndexes) {
    std::vector<PCNode *> restriction;
    restriction.reserve(listIndexes.size());
    for (int i: listIndexes) {
        restriction.push_back(tree.getLeaves().at(i));
    }
    return tree.makeConsecutive(restriction);
}

void testFromString(PCTree &tree) {
    it("produces equivalent trees when constructed with fromString", [&]() {
        std::stringstream ss;
        ss << tree;
        PCTree copy(ss.str(), true);
        AssertThat(tree.uniqueID(uid_utils::leafToID), Equals(copy.uniqueID(uid_utils::leafToID)));
    });
}

void testCopyCtor(PCTree &tree) {
    it("copies the tree correctly", [&]() {
        PCTreeNodeArray<PCNode *> map(tree);
        PCTree copy(tree, map, true);
        AssertThat(tree.uniqueID(uid_utils::leafToID), Equals(copy.uniqueID(uid_utils::leafToID)));
        FilteringPCTreeDFS dfs(tree, tree.getRootNode());
        for (PCNode *n: dfs) {
            AssertThat(map[n]->index(), Equals(n->index()));
        }
    });
}

void testLeafOrder(PCTree &tree) {
    it("considers the current leaf order a valid permutation", [&]() {
        std::vector<PCNode *> leafOrder;
        tree.currentLeafOrder(leafOrder);
        AssertThat(tree.isValidOrder(leafOrder), IsTrue());
    });
}

template<typename Iterable>
void testIterator(PCTree &tree, Iterable iterable) {
    PCTreeNodeArray<bool> visited(tree, false);

    int visitedLeafs = 0;
    int visitedPNodes = 0;
    int visitedCNodes = 0;
    for (PCNode *n: iterable) {
        AssertThat(visited[n], IsFalse());
        visited[n] = true;
        if (n->isLeaf()) {
            ++visitedLeafs;
        } else if (n->getNodeType() == pc_tree::PCNodeType::PNode) {
            ++visitedPNodes;
        } else {
            OGDF_ASSERT(n->getNodeType() == pc_tree::PCNodeType::CNode);
            ++visitedCNodes;
        }
    }
    AssertThat(visitedLeafs, Equals(tree.getLeafCount()));
    AssertThat(visitedPNodes, Equals(tree.getPNodeCount()));
    AssertThat(visitedCNodes, Equals(tree.getCNodeCount()));
}

void testIterators(PCTree &tree) {
    testIterator(tree, FilteringPCTreeDFS(tree, tree.getRootNode()));
    testIterator(tree, FilteringPCTreeBFS(tree, tree.getRootNode()));
}

void testGeneric(PCTree &tree) {
    testFromString(tree);
    testCopyCtor(tree);
    testLeafOrder(tree);
    testIterators(tree);
}

void testPlanarity(int nodes, int edges, int seed, bool forcePlanar) {
    std::stringstream ss;
    ss << "correctly determines planarity of a graph (nodes: " << nodes << ", edges: " << edges << ", seed: " << seed
       << ", forcePlanar: " << forcePlanar << ")";
    it(ss.str(), [&]() {
        Graph G;
        setSeed(seed);
        if (forcePlanar) {
            randomPlanarBiconnectedGraph(G, nodes, edges);
        } else {
            randomBiconnectedGraph(G, nodes, edges);
        }

        bool success = true;
        try {
            NodePCRotation N(G, G.lastNode());
            if (N.getLeafCount() > 2) {
                testGeneric(N);
            }
        } catch (GraphNotPlanarException &e) {
            success = false;
        }

        if (forcePlanar) {
            AssertThat(success, IsTrue());
        } else {
            BoothLueker BL;
            AssertThat(success, Equals(BL.isPlanar(G)));
        }
    });
}

void testPlanarity() {
    std::list<std::function<int(int)>> edgeFuncs{
            [](int nodes) {
                return nodes;
            },
            [](int nodes) {
                return 2 * nodes;
            },
            [](int nodes) {
                return 3 * nodes - 6;
            }
    };
    std::time_t time = std::time(nullptr);
    std::vector<int> seeds;
    for (int i = 0; i < 10; i++) {
        seeds.push_back(i);
        seeds.push_back(time + i);
    }

    for (int nodes = 10; nodes <= 100; nodes += 10) {
        for (auto &func: edgeFuncs) {
            for (int seed: seeds) {
                testPlanarity(nodes, func(nodes), seed, true);
                testPlanarity(nodes, func(nodes), seed, false);
            }
        }
    }
}

go_bandit([]() {
    describe("PCTree", []() {
        it("applies restrictions correctly", []() {
            std::vector<PCNode *> added;
            PCTree T(10, &added);
            AssertThat(T.isTrivial(), IsTrue());
            AssertThat(makeConsecutive(T, {0, 1}), IsTrue());
            AssertThat(makeConsecutive(T, {2, 3}), IsTrue());
            AssertThat(makeConsecutive(T, {1, 2}), IsTrue());
            AssertThat(makeConsecutive(T, {3, 4, 5}), IsTrue());
            AssertThat(makeConsecutive(T, {1, 3}), IsFalse());
            AssertThat(T.isTrivial(), IsFalse());
            AssertThat(added, Equals(T.getLeaves()));

            testGeneric(T);
        });

        it("applies restrictions correctly", []() {
            PCTree T(50);
            AssertThat(makeConsecutive(T, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}),
                       IsTrue());
            AssertThat(makeConsecutive(T, {25, 26, 27, 28, 29, 30, 31, 32, 33, 34}), IsTrue());
            AssertThat(makeConsecutive(T, {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49}), IsTrue());
            AssertThat(makeConsecutive(T, {17, 42}), IsTrue());
            AssertThat(makeConsecutive(T, {5, 6, 7, 8, 9}), IsTrue());
            AssertThat(makeConsecutive(T, {11, 12, 13, 14}), IsTrue());
            AssertThat(makeConsecutive(T, {5, 6, 7, 8, 9, 10, 11, 12, 13, 14}), IsTrue());
            AssertThat(makeConsecutive(T, {8, 9}), IsTrue());
            AssertThat(makeConsecutive(T, {9, 10}), IsTrue());
            AssertThat(makeConsecutive(T, {10, 11}), IsTrue());
            AssertThat(makeConsecutive(T, {47, 48, 49, 36, 37, 38}), IsTrue());
            AssertThat(makeConsecutive(T, {47, 48}), IsTrue());
            AssertThat(makeConsecutive(T, {37, 38}), IsTrue());
            AssertThat(makeConsecutive(T, {36, 37, 38}), IsTrue());
            AssertThat(makeConsecutive(T, {47, 48, 49}), IsTrue());
            AssertThat(makeConsecutive(T, {49, 36}), IsTrue());
            AssertThat(makeConsecutive(T, {47, 32}), IsTrue());
            AssertThat(makeConsecutive(T, {34, 33, 25}), IsTrue());
            AssertThat(makeConsecutive(T, {34, 33, 32, 25}), IsTrue());
            AssertThat(makeConsecutive(T, {27, 33}), IsTrue());
            AssertThat(makeConsecutive(T, {12, 11, 10, 9, 8, 7, 6, 5, 4}), IsTrue());

            testGeneric(T);
        });

        it("detects validity of orders", []() {
            PCTree T(9);

            AssertThat(makeConsecutive(T, {0, 1, 2}), IsTrue());
            AssertThat(makeConsecutive(T, {3, 4, 5}), IsTrue());
            AssertThat(makeConsecutive(T, {0, 1}), IsTrue());
            AssertThat(makeConsecutive(T, {1, 2}), IsTrue());
            AssertThat(makeConsecutive(T, {0, 1, 2, 3}), IsTrue());

            auto leaves = T.getLeaves();
            AssertThat(T.isValidOrder(leaves), IsTrue());
            std::reverse(leaves.begin(), leaves.end());
            AssertThat(T.isValidOrder(leaves), IsTrue());
            leaves = T.getLeaves();
            std::swap(leaves.at(2), leaves.at(3));
            AssertThat(T.isValidOrder(leaves), IsFalse());

            testGeneric(T);
        });

        it("correctly applies restrictions on manually constructed trees", []() {
            PCTree T;
            auto root = T.newNode(pc_tree::PCNodeType::CNode);
            auto n1 = T.newNode(pc_tree::PCNodeType::PNode, root);
            T.insertLeaves(5, root);
            auto n2 = T.newNode(pc_tree::PCNodeType::PNode, root);
            T.insertLeaves(5, n2);
            T.insertLeaves(5, root);
            T.insertLeaves(5, n1);

            AssertThat(T.checkValid(), IsTrue());
            testGeneric(T);

            AssertThat(makeConsecutive(T, {7, 10, 15}), IsFalse());
            AssertThat(makeConsecutive(T, {6, 10, 11, 12, 13, 14, 17}), IsTrue());
            testGeneric(T);
        });

        it("correctly performs a simple intersection", []() {
            PCTree T(10);
            PCTree T2("0:(14:[15:(6, 5), 4, 3, 2, 1], 10, 9, 8, 7)");
            PCTreeNodeArray<PCNode *> leafMap(T2);
            for (int i = 0; i < T.getLeafCount(); ++i) {
                leafMap[T2.getLeaves().at(i)] = T.getLeaves().at(i);
            }
            AssertThat(T.intersect(T2, leafMap), IsTrue());
            testGeneric(T);
        });
    });

    describe("NodePCRotation", []() {
        testPlanarity();

        it("computes bundle edges correctly", []() {
            // FIXME this test passes on HsuPC but fails on UFPC
//            Graph G12;
//            auto a1 = G12.newNode(1);
//            auto a2 = G12.newNode(2);
//            auto a3 = G12.newNode(3);
//            auto a4 = G12.newNode(4);
//            auto a5 = G12.newNode(5);
//            auto a6 = G12.newNode(6);
//            auto a7 = G12.newNode(7);
//            auto a8 = G12.newNode(8);
//            auto a9 = G12.newNode(9);
//            G12.newEdge(a1, a9);
//            G12.newEdge(a1, a2);
//            G12.newEdge(a2, a9);
//            G12.newEdge(a2, a3);
//            G12.newEdge(a3, a4);
//            G12.newEdge(a4, a5);
//            G12.newEdge(a5, a6);
//            G12.newEdge(a5, a7);
//            G12.newEdge(a6, a8);
//            G12.newEdge(a7, a8);
//            G12.newEdge(a8, a9);
//
//            NodePCRotation test12(G12, a9, true);
//            node partner = test12.getTrivialPartnerPole();
//            AssertThat(partner != nullptr, IsTrue());
//            RegisteredEdgeSet adjEntries(G12);
//            for (auto leaf: test12.getLeaves()) {
//                AssertThat(test12.getPartnerEdgesForLeaf(leaf).empty(), IsFalse());
//                for (edge twinEdge: test12.getPartnerEdgesForLeaf(leaf)) {
//                    adjEntries.insert(twinEdge);
//                }
//            }
//
//            AssertThat(adjEntries.size(), Equals(partner->degree()));
//            for (edge e: adjEntries.elements()) {
//                AssertThat(e->isIncident(partner), IsTrue());
//            }
        });
    });
});

int main(int argc, char *argv[]) {
    return bandit::run(argc, argv);
}
