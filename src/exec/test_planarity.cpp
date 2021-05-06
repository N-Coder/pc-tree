#include "likwid_utils.h"

#include "NodePCRotation.h"
#include "utils/RegisteredElementSet.h"
#include "adapters/C1IUtils.h"
#include "adapters/OgdfPQTreeAdapter.h"

#include <hsuPC/include/PCTree.h>
#include <hsuPC/include/PCNode.h>

#include "zanettiPQR/PQRTree.h"

#include <ogdf/basic/STNumbering.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/planarity/BoothLueker.h>
#include <ogdf/planarity/BoyerMyrvold.h>
#include <ogdf/planarity/booth_lueker/PlanarPQTree.h>
#include <ogdf/planarity/booth_lueker/IndInfo.h>

#include <iostream>
#include <chrono>
#include <getopt.h>
#include <csignal>
#include <csetjmp>

using std::chrono::time_point;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

using namespace ogdf::booth_lueker;
using namespace pc_tree::hsu;

std::string name;
std::string inputPath;
std::string restrictionOutputPath;
int minRestrictionSize = 25;
int checkpointInterval = 1000;

#include "CustomPlanarPQTree.h"

jmp_buf env;
void signalHandler(int sig) {
    siglongjmp(env, 1);
}

long isPlanarPQ(Graph &G, NodeArray<int> &numbering) {
    using PQLeafKeyType = PQLeafKey<edge, void *, void *>;

    bool isPlanar = true;
    long accumTime = 0;
    NodeArray<SListPure<PQLeafKeyType *> > inLeaves(G);
    NodeArray<SListPure<PQLeafKeyType *> > outLeaves(G);
    Array<node> table(G.numberOfNodes() + 1);

    for (node v : G.nodes) {
        for (adjEntry adj : v->adjEntries) {
            edge e = adj->theEdge();
            if (numbering[e->opposite(v)] > numbering[v]) { //sideeffect: loops are ignored
                auto *L = new PQLeafKeyType(e);
                inLeaves[v].pushFront(L);
            }
        }
        table[numbering[v]] = v;
    }

    for (node v : G.nodes) {
        for (PQLeafKeyType *L : inLeaves[v]) {
            outLeaves[L->userStructKey()->opposite(v)].pushFront(L);
        }
    }


    CustomPlanarPQTree T;

    T.Initialize(inLeaves[table[1]]);
    for (int i = 2; i < G.numberOfNodes(); i++) {
        time_point<high_resolution_clock> start = high_resolution_clock::now();
        bool b = T.Reduction(outLeaves[table[i]]);
        time_point<high_resolution_clock> end = high_resolution_clock::now();
        long time = duration_cast<nanoseconds>(end - start).count();

        T.savePertinentNodeInfo();

        time_point<high_resolution_clock> startCleanup = high_resolution_clock::now();
        T.emptyAllPertinentNodes();
        time_point<high_resolution_clock> endCleanup = high_resolution_clock::now();
        time += duration_cast<nanoseconds>(endCleanup - startCleanup).count();
        accumTime += time;

        if (outLeaves[table[i]].size() >= minRestrictionSize || (i - 1) % checkpointInterval == 0 || !b) {
            json restrictionResults;
            restrictionResults["file"] = inputPath;
            restrictionResults["index"] = i - 1;
            restrictionResults["result"] = b;
            restrictionResults["size"] = outLeaves[table[i]].size();
            restrictionResults["time"] = time;
            restrictionResults["type"] = "OGDF";
            restrictionResults["name"] = name;
            if ((i - 1) % checkpointInterval == 0) {
                restrictionResults["fingerprint"] = fingerprint(OgdfPQTreeAdapter::possibleOrders(T));
            }
            cout << restrictionResults << endl;
        }

        if (b) {
            T.restorePertinentNodeInfo();
            T.ReplaceRoot(inLeaves[table[i]]);
            T.cleanUpFull();
        } else {
            isPlanar = false;
            break;
        }
    }

    // Cleanup
    for (node v : G.nodes) {
        while (!inLeaves[v].empty()) {
            PQLeafKeyType *L = inLeaves[v].popFrontRet();
            delete L;
        }
    }

    return isPlanar ? accumTime : -accumTime;
}

template<typename PCT>
void getStats(PCT &T, json &data);

template<>
void getStats(pc_tree::PCTree &T, json &data) {
    data["leaves"] = T.getLeafCount();
    data["tp_length"] = T.getTerminalPathLength();
    data["p_nodes"] = T.getPNodeCount();
    data["c_nodes"] = T.getCNodeCount();
    data["type"] = "UFPC";
}

template<>
void getStats(pc_tree::hsu::PCTree &T, json &data) {
    data["leaves"] = T.getLeafCount();
    data["tp_length"] = T.getTerminalPathLength();
    data["type"] = "HsuPC";
}

template<typename PCT, typename PCN, typename PCNT, template<typename> typename PCNA>
long isPlanarPC(Graph &G, NodeArray<int> &numbering) {
    PCT T;
    long accumTime = 0;

    Array<node> order(G.numberOfNodes());
    for (node n : G.nodes) {
        order[numbering[n] - 1] = n;
    }

    EdgeArray<PCN *> leafRepresentation(G);
    PCNA<edge> edgeRepresentation(T);
    EdgeArray<int> edgeNumbering(G);
    ogdf::RegisteredElementSet<edge, Graph> freeIndexes(G);

    std::vector<edge> outEdges;
    std::vector<PCN *> consecutiveLeaves;

    std::ofstream restrictionFile;
    if (!restrictionOutputPath.empty())
        restrictionFile.open(restrictionOutputPath);
    int nextEdgeCounter = 0;
    for (node n : order) {
        if (n == order[G.numberOfNodes() - 1]) {
            break;
        }

        consecutiveLeaves.clear();
        outEdges.clear();
        json serializePlanarity;
        serializePlanarity["index"] = numbering[n] - 1;
        serializePlanarity["file"] = inputPath;
        for (adjEntry adj : n->adjEntries) {
            if (numbering[adj->theEdge()->opposite(n)] > numbering[n]) {
                outEdges.push_back(adj->theEdge());
                int index;
                if (freeIndexes.size() > 0) {
                    edge e = freeIndexes.elements().front();
                    index = edgeNumbering[e];
                    freeIndexes.remove(e);
                } else {
                    index = nextEdgeCounter++;
                }
                edgeNumbering[adj->theEdge()] = index;
                serializePlanarity["replace"].push_back(index);
            } else {
                consecutiveLeaves.push_back(leafRepresentation[adj->theEdge()]);
                serializePlanarity["consecutive"].push_back(edgeNumbering[adj->theEdge()]);
                freeIndexes.insert(adj->theEdge());
            }
        }
        if (!restrictionOutputPath.empty())
            restrictionFile << serializePlanarity << std::endl;

        PCN *mergedLeaf;
        if (n == order[0]) {
            mergedLeaf = T.newNode(PCNT::PNode);
        } else {
            likwid_before_it();
            time_point<high_resolution_clock> initStart = high_resolution_clock::now();
            bool possible = T.makeConsecutive(consecutiveLeaves);
            time_point<high_resolution_clock> initEnd = high_resolution_clock::now();
            long time = duration_cast<nanoseconds>(initEnd - initStart).count();
            accumTime += time;

            json restrictionResults;
            if (consecutiveLeaves.size() >= minRestrictionSize || (numbering[n] - 1) % checkpointInterval == 0 || !possible) {
                restrictionResults["file"] = inputPath;
                restrictionResults["index"] = numbering[n] - 1;
                restrictionResults["result"] = possible;
                restrictionResults["size"] = consecutiveLeaves.size();
                restrictionResults["time"] = time;
                restrictionResults["name"] = name;
                getStats(T, restrictionResults);
                likwid_after_it(restrictionResults);
                if ((numbering[n] - 1) % checkpointInterval == 0) {
                    restrictionResults["fingerprint"] = fingerprint(T.possibleOrders());
                    std::stringstream sb;
                    T.uniqueID(sb, [&](std::ostream &os, PCN *n, int pos) -> void {
                        if (n->getNodeType() == PCNT::Leaf)
                            os << edgeNumbering[edgeRepresentation[n]];
                    }, [&](PCN *a, PCN *b) -> bool {
                        return edgeNumbering[edgeRepresentation[a]] < edgeNumbering[edgeRepresentation[b]];
                    });
                    restrictionResults["uid"] = sb.str();
                }
                cout << restrictionResults << endl;
            } else {
                likwid_after_it(restrictionResults);
            }

            mergedLeaf = T.mergeLeaves(consecutiveLeaves, true);
        }

        if (mergedLeaf == nullptr) {
            return -accumTime;
        }

        OGDF_ASSERT(!outEdges.empty());
        if (outEdges.size() > 1) {
            std::vector<PCN *> addedLeaves;
            if (mergedLeaf->getNodeType() == PCNT::PNode) {
                T.insertLeaves(outEdges.size(), mergedLeaf, &addedLeaves);
            } else {
                T.replaceLeaf(outEdges.size(), mergedLeaf, &addedLeaves);
            }

            for (int i = 0; i < outEdges.size(); i++) {
                leafRepresentation[outEdges[i]] = addedLeaves[i];
                edgeRepresentation[addedLeaves[i]] = outEdges[i];
            }
        } else {
            leafRepresentation[outEdges.front()] = mergedLeaf;
            edgeRepresentation[mergedLeaf] = outEdges.front();
        }
    }

    return accumTime;
}

long isPlanarPQR(const Graph &G, NodeArray<int> &numbering) {
    std::unique_ptr<cpp_zanetti::PQRTree> T;
    long accumTime = 0;

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

        if (n == order[0]) {
            T = std::make_unique<cpp_zanetti::PQRTree>(outEdges.size(), addedLeaves);
        } else {
            bool possible = true;
            std::string exception;
            time_point<high_resolution_clock> start = high_resolution_clock::now();
            try {
                possible = T->reduce(consecutiveLeaves);
            } catch (const std::exception &e) {
                possible = false;
                exception = e.what();
            }
            long time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
            accumTime += time;

            if (consecutiveLeaves.size() >= minRestrictionSize || (numbering[n] - 1) % checkpointInterval == 0 || !possible) {
                json restrictionResults;
                restrictionResults["file"] = inputPath;
                restrictionResults["index"] = numbering[n] - 1;
                restrictionResults["result"] = possible;
                restrictionResults["size"] = consecutiveLeaves.size();
                restrictionResults["time"] = time;
                restrictionResults["type"] = "CppZanetti";
                restrictionResults["name"] = name;
                if (!exception.empty()) {
                    restrictionResults["exception"] = exception;
                }
                if ((numbering[n] - 1) % checkpointInterval == 0) {
                    restrictionResults["fingerprint"] = fingerprint(T->getPossibleOrders());
                    restrictionResults["tree"] = T->toString();
                }
                cout << restrictionResults << endl;
            }
            if (!possible) {
                return -accumTime;
            }
            try {
                T->mergeAndReplaceLeaves(consecutiveLeaves, outEdges, addedLeaves);
            } catch (std::exception &e) {
                json restrictionResults;
                restrictionResults["file"] = inputPath;
                restrictionResults["index"] = numbering[n] - 1;
                restrictionResults["type"] = "CppZanetti";
                restrictionResults["name"] = name;
                restrictionResults["exception"] = e.what();
                std::cout << restrictionResults << std::endl;
                return -accumTime;
            }
        }

        OGDF_ASSERT(outEdges.size() == addedLeaves.size());
        for (int i = 0; i < outEdges.size(); i++) {
            leafRepresentation.at(outEdges.at(i)) = addedLeaves.at(i);
        }
    }

    return accumTime;
}

int main(int argc, char **argv) {
    std::string type = "UFPC";
    bool accumulate = false;
    int c;
    while ((c = getopt(argc, argv, "as:c:t:n:")) != -1) {
        switch (c) {
            case 'a':
                accumulate = true;
                break;
            case 's':
                minRestrictionSize = std::stoi(optarg);
                break;
            case 'c':
                checkpointInterval = std::stoi(optarg);
                break;
            case 't':
                type = optarg;
                break;
            case 'n':
                name = optarg;
                break;
            default:
                cerr << "Invalid arguments!" << std::endl;
                return 1;
        }
    }
    if (name.empty()) {
        name = type;
    }

    if (optind >= argc) {
        cerr << "Expected infile!" << std::endl;
        return 1;
    }
    inputPath = argv[optind++];
    if (optind < argc) {
        restrictionOutputPath = argv[optind++];
    }
    if (optind < argc) {
        cerr << "Too many positional arguments!" << std::endl;
        return 1;
    }

    Graph G;
    if (!GraphIO::read(G, inputPath)) {
        cerr << "Could not read file " << inputPath << "!" << endl;
        return 1;
    }

    signal(SIGSEGV, signalHandler);
    NodeArray<int> numbering(G);
    int num = computeSTNumbering(G, numbering);
    OGDF_ASSERT(num > 0);

    if (sigsetjmp(env, 1) == 0) {
        if (type == "UFPC") {
            json likwidData;
            likwid_prepare(likwidData, accumulate);
            isPlanarPC<pc_tree::PCTree, pc_tree::PCNode, pc_tree::PCNodeType, pc_tree::PCTreeNodeArray>(G, numbering);
            likwid_finalize(likwidData);
            if (!likwidData.empty())
                cerr << likwidData << std::endl;
        } else if (type == "HsuPC") {
            isPlanarPC<pc_tree::hsu::PCTree, pc_tree::hsu::PCNode, pc_tree::hsu::PCTree::PCNodeType, pc_tree::hsu::PCTreeNodeArray>(G, numbering);
        } else if (type == "OGDF") {
            isPlanarPQ(G, numbering);
        } else if (type == "CppZanetti") {
            isPlanarPQR(G, numbering);
        } else {
            cerr << "Invalid type argument " << type << "!" << std::endl;
            return 1;
        }
    } else {
        json errorOutput;
        errorOutput["file"] = inputPath;
        errorOutput["type"] = type;
        errorOutput["index"] = -1;
        errorOutput["name"] = name;
        errorOutput["exception"] = SIGSEGV;
        std::cout << errorOutput << std::endl;
    }

    return 0;
}
