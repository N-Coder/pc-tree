#include "likwid_utils.h"

#include "PCTree.h"

#include "hsuPC/include/PCTree.h"
#include "hsuPC/include/PCNode.h"

#ifdef ENABLE_CPPZANETTI
#include "zanettiPQR/PQRTree.h"
#endif

#include <ogdf/basic/STNumbering.h>
#include <ogdf/basic/graph_generators.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/planarity/BoothLueker.h>
#include <ogdf/planarity/BoyerMyrvold.h>

#include <ogdf/planarity/booth_lueker/IndInfo.h>
#include <ogdf/planarity/booth_lueker/PlanarPQTree.h>

#include <iostream>
#include <chrono>
#include <getopt.h>

using namespace ogdf;

using std::chrono::time_point;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

// TODO for non-planar return number of nodes processed instead of false

bool isPlanarPQ(const Graph &G, const NodeArray<int> &numbering) {
    using namespace booth_lueker;

    bool planar = true;

    NodeArray<SListPure<PlanarLeafKey<IndInfo *> *> > inLeaves(G);
    NodeArray<SListPure<PlanarLeafKey<IndInfo *> *> > outLeaves(G);
    Array<node> table(G.numberOfNodes() + 1);

    for (node v : G.nodes) {
        for (adjEntry adj : v->adjEntries) {
            edge e = adj->theEdge();
            if (numbering[e->opposite(v)] > numbering[v])
                //sideeffect: loops are ignored
            {
                PlanarLeafKey<IndInfo *> *L = new PlanarLeafKey<IndInfo *>(e);
                inLeaves[v].pushFront(L);
            }
        }
        table[numbering[v]] = v;
    }

    for (node v : G.nodes) {
        for (PlanarLeafKey<IndInfo *> *L : inLeaves[v]) {
            outLeaves[L->userStructKey()->opposite(v)].pushFront(L);
        }
    }

    PlanarPQTree T;

    T.Initialize(inLeaves[table[1]]);
    for (int i = 2; i < G.numberOfNodes(); i++) {
        if (T.Reduction(outLeaves[table[i]])) {
            T.ReplaceRoot(inLeaves[table[i]]);
            T.emptyAllPertinentNodes();

        } else {
            planar = false;
            break;
        }
    }
    if (planar)
        T.emptyAllPertinentNodes();


    // Cleanup
    for (node v : G.nodes) {
        while (!inLeaves[v].empty()) {
            PlanarLeafKey<IndInfo *> *L = inLeaves[v].popFrontRet();
            delete L;
        }
    }

    return planar;
}

template<typename PCT, typename PCN, typename PCNT>
bool isPlanarPC(const Graph &G, const NodeArray<int> &numbering) {
    PCT T;

    std::vector<node> order(G.numberOfNodes(), nullptr);
    for (node n : G.nodes) {
        order.at(numbering[n] - 1) = n;
    }

    std::vector<PCN *> leafRepresentation(G.maxEdgeIndex() + 1, nullptr);
    std::vector<edge> outEdges;
    std::vector<PCN *> consecutiveLeaves;

    for (node n : order) {
        if (n == order[G.numberOfNodes() - 1]) {
            break;
        }

        consecutiveLeaves.clear();
        outEdges.clear();
        for (adjEntry adj : n->adjEntries) {
            if (numbering[adj->theEdge()->opposite(n)] > numbering[n]) {
                outEdges.push_back(adj->theEdge());
            } else {
                consecutiveLeaves.push_back(leafRepresentation[adj->theEdge()->index()]);
            }
        }

        PCN *mergedLeaf;
        if (n == order[0]) {
            mergedLeaf = T.newNode(PCNT::PNode);
        } else {
            if (!T.makeConsecutive(consecutiveLeaves)){
                return false;
            }
            mergedLeaf = T.mergeLeaves(consecutiveLeaves, true);
        }

        OGDF_ASSERT(!outEdges.empty());
        if (outEdges.size() > 1) {
            consecutiveLeaves.clear(); // re-use consecutiveLeaves
            std::vector<PCN *> &addedLeaves = consecutiveLeaves;
            if (mergedLeaf->getNodeType() == PCNT::PNode) {
                T.insertLeaves(outEdges.size(), mergedLeaf, &addedLeaves);
            } else {
                OGDF_ASSERT(mergedLeaf->getNodeType() == PCNT::Leaf);
                T.replaceLeaf(outEdges.size(), mergedLeaf, &addedLeaves);
            }

            for (int i = 0; i < outEdges.size(); i++) {
                leafRepresentation[outEdges[i]->index()] = addedLeaves[i];
            }
        } else {
            leafRepresentation[outEdges.front()->index()] = mergedLeaf;
        }
    }

    return true;
}

#ifdef ENABLE_CPPZANETTI
bool isPlanarPQR(const Graph &G, const NodeArray<int> &numbering) {
    std::unique_ptr<cpp_zanetti::PQRTree> T;

    std::vector<node> order(G.numberOfNodes(), nullptr);
    for (node n : G.nodes) {
        order[numbering[n] - 1] = n;
    }

    std::vector<cpp_zanetti::Leaf *> leafRepresentation(G.maxEdgeIndex() + 1, nullptr);
    std::vector<int> outEdges;
    std::vector<cpp_zanetti::Leaf *> consecutiveLeaves, addedLeaves;

    for (node n : order) {
        if (n == order[G.numberOfNodes() - 1]) {
            break;
        }
//        std::cout << "Iteration " << numbering[n] - 1 << std::endl;
        consecutiveLeaves.clear();
        addedLeaves.clear();
        outEdges.clear();
        for (adjEntry adj : n->adjEntries) {
            if (numbering[adj->theEdge()->opposite(n)] > numbering[n]) {
                outEdges.push_back(adj->theEdge()->index());
            } else {
                consecutiveLeaves.push_back(leafRepresentation.at(adj->theEdge()->index()));
            }
        }
//        std::cout << "Merge: ";
//        for (auto *l : consecutiveLeaves) std::cout << l->value << ", ";
//        std::cout << std::endl;
//        std::cout << "Insert: ";
//        for (auto l : outEdges) std::cout << l << ", ";
//        std::cout << std::endl;

        if (n == order[0]) {
            T = std::make_unique<cpp_zanetti::PQRTree>(outEdges.size(), addedLeaves);
        } else {
            if (!T->reduce(consecutiveLeaves)) {
                return false;
            }
//            std::cout << T->toString() << std::endl;
            T->mergeAndReplaceLeaves(consecutiveLeaves, outEdges, addedLeaves);
        }

        OGDF_ASSERT(outEdges.size() == addedLeaves.size());
        for (int i = 0; i < outEdges.size(); i++) {
            leafRepresentation.at(outEdges.at(i)) = addedLeaves.at(i);
        }
//        std::cout << T->toString() << std::endl;
//        std::cout << std::endl;
    }

    return true;
}
#endif


template<typename Ret=bool, typename Func>
Ret test(json &results, const std::string &name, const Func &func) {
    time_point<high_resolution_clock> start, end;
    Ret ret;
    {
        start = high_resolution_clock::now();
        ret = func();
        end = high_resolution_clock::now();
    }
    results["times"][name] = duration_cast<nanoseconds>(end - start).count();
    results["results"][name] = ret;
    return ret;

}

int main(int argc, char **argv) {
    int nodeCount = 0;
    int edgeCount = 0;
    int seed = 0;
    bool planar = false;
    std::string inputFile;
    int repetitions = 1;

    int c;
    while ((c = getopt(argc, argv, "n:m:ps:i:r:")) != -1) {
        switch (c) {
            case 'n':
                nodeCount = std::stoi(optarg);
                break;
            case 'm':
                edgeCount = std::stoi(optarg);
                break;
            case 'r':
                repetitions = std::stoi(optarg);
                break;
            case 'p':
                planar = true;
                break;
            case 's':
                seed = std::stoi(optarg);
                break;
            case 'i':
                inputFile = optarg;
                break;
            default:
                std::cerr << "Invalid arguments" << std::endl;
                return 1;
        }
    }

    json results;
    Graph G;
    if (inputFile.empty()) {
        std::stringstream sb;
        sb << "G-n" << nodeCount << "-m" << edgeCount << "-s" << seed << "-p" << planar;
        results["id"] = sb.str();

        setSeed(seed);
        if (planar)
            randomPlanarBiconnectedGraph(G, nodeCount, edgeCount);
        else
            randomBiconnectedGraph(G, nodeCount, edgeCount);
    } else {
        results["id"] = inputFile;
        if (!GraphIO::read(G, inputFile)) {
            cerr << "Could not read file " << inputFile << "!" << endl;
            return 1;
        }
    }

    results["nodes"] = G.numberOfNodes();
    results["edges"] = G.numberOfEdges();

    likwid_prepare(results, false);
    while (repetitions > 0) {
        repetitions--;
        results["repetition"] = repetitions;

        NodeArray<int> numbering(G);
        int num = test<int>(results, "stNumbering", [&] { return computeSTNumbering(G, numbering); });
        OGDF_ASSERT(num == G.numberOfNodes());

        likwid_before_it();
        test(results, "UFPC::isPlanar", [&] { return isPlanarPC<pc_tree::PCTree, pc_tree::PCNode, pc_tree::PCNodeType>(G, numbering); });
        likwid_after_it(results);

        test(results, "HsuPC::isPlanar", [&] { return isPlanarPC<pc_tree::hsu::PCTree, pc_tree::hsu::PCNode, pc_tree::hsu::PCTree::PCNodeType>(G, numbering); });
        test(results, "BoothLueker::doTest", [&] { return isPlanarPQ(G, numbering); });
#ifdef ENABLE_CPPZANETTI
        test(results, "CppZanetti::isPlanar", [&] { return isPlanarPQR(G, numbering); });
#endif
        Graph G1(G);
        test(results, "BoothLueker::isPlanarDestructive", [&] { return BoothLueker().isPlanarDestructive(G1); });
        Graph G2(G);
        test(results, "BoyerMyrvold::isPlanarDestructive", [&] { return BoyerMyrvold().isPlanarDestructive(G2); });

        cout << results << std::endl;
    }
    likwid_finalize(results);
    return 0;
}
