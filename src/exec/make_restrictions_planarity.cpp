#include "PCTree.h"
#include "utils.h"

#include <ogdf/basic/basic.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/STNumbering.h>
#include <ogdf/basic/extended_graph_alg.h>
#include <ogdf/basic/graph_generators.h>

#include <iostream>
#include <memory>
#include <getopt.h>
#include <filesystem>

bool isPlanar(Graph &G, std::string &id, std::vector<std::string> &matrices, ConsecutiveOnesInterface *dumpT,
              std::filesystem::path &matrixDir, bool planar) {
    NodeArray<int> numbering(G);
    edge st = G.chooseEdge();
    int nodeCount = computeSTNumbering(G, numbering, st->source(), st->target());
    OGDF_ASSERT(nodeCount == G.numberOfNodes());
    std::vector<node> order(nodeCount);
    for (node n : G.nodes)
        order[numbering[n] - 1] = n;

    PCTree T;
    EdgeArray<PCNode *> leafRepresentation(G);
    std::vector<edge> outEdges;
    std::vector<PCNode *> consecutiveLeaves;

    for (node n : order) {
        if (n == order.back()) {
            return true;
        }

        consecutiveLeaves.clear();
        outEdges.clear();
        for (adjEntry adj : n->adjEntries) {
            if (numbering[adj->theEdge()->opposite(n)] > numbering[n]) {
                outEdges.push_back(adj->theEdge());
            } else {
                consecutiveLeaves.push_back(leafRepresentation[adj->theEdge()]);
            }
        }

        PCNode *mergedLeaf;
        if (n == order.front()) {
            mergedLeaf = T.newNode(PCNodeType::PNode, nullptr);
        } else {
            if (!T.isTrivialRestriction(consecutiveLeaves)) {
                std::vector<std::vector<int>> restrictions;
                getRestrictions(T, restrictions, leafRepresentation[st], &consecutiveLeaves);
                json matrixJson;
                matrixJson["parent_id"] = id;
                matrixJson["node_idx"] = numbering[n] - 2; // s ^= 0, root ^= 1
                int idx = matrixJson["idx"] = matrices.size();
                std::stringstream sb;
                sb << id << "_" << idx;
                std::filesystem::path matrixFile(matrixDir);
                matrixFile /= sb.str();
                int cols;
                int rows;
                matrixJson["id"] = sb.str();
                matrixJson["cols"] = cols = T.getLeafCount();
                matrixJson["rows"] = rows = restrictions.size();

                int numOnes = 0;
                int maxSize = 0;
                for (auto &res : restrictions) {
                    numOnes += res.size();
                    maxSize = std::max(maxSize, (int) res.size());
                }
                matrixJson["num_ones"] = numOnes;
                matrixJson["fraction"] = numOnes / (((double) cols) * rows);
                matrixJson["max_size"] = maxSize;
                matrixJson["max_fraction"] = ((double) maxSize) / cols;
                json &lastRes = matrixJson["last_restriction"] = {};
                lastRes["c_nodes"] = T.getCNodeCount();
                lastRes["p_nodes"] = T.getPNodeCount();

                bool possible;
                matrixJson["exp_possible"] = possible = T.makeConsecutive(consecutiveLeaves);
                lastRes["size"] = consecutiveLeaves.size();
                lastRes["exp_possible"] = possible;
                lastRes["tpLength"] = T.getTerminalPathLength();
                lastRes["exp_uid"] = T.uniqueID(uid_utils::nodeToPosition);
                lastRes["exp_fingerprint"] = fingerprint(T.possibleOrders());
                matrixJson["restrictions"] = std::move(restrictions);
                if (planar || !possible) {
                    std::ofstream outFile(matrixFile, std::ofstream::out | std::ofstream::trunc);
                    outFile << matrixJson;
                    matrices.push_back(matrixFile.string());
                }
                if (!possible) {
                    return false;
                }
            }

            mergedLeaf = T.mergeLeaves(consecutiveLeaves, true);
        }

        OGDF_ASSERT(!outEdges.empty());
        if (outEdges.size() > 1) {
            std::vector<PCNode *> addedLeaves;
            if (mergedLeaf->getNodeType() == PCNodeType::PNode) {
                T.insertLeaves(outEdges.size(), mergedLeaf, &addedLeaves);
            } else {
                OGDF_ASSERT(mergedLeaf->getNodeType() == PCNodeType::Leaf);
                if (T.getLeafCount() <= 2) {
                    T.insertLeaves(outEdges.size(), mergedLeaf->getParent(), &addedLeaves);
                    mergedLeaf->detach();
                    T.destroyNode(mergedLeaf);
                } else {
                    T.replaceLeaf(outEdges.size(), mergedLeaf, &addedLeaves);
                }
            }

            for (int i = 0; i < outEdges.size(); i++) {
                leafRepresentation[outEdges[i]] = addedLeaves[i];
            }
        } else {
            leafRepresentation[outEdges.front()] = mergedLeaf;
        }
    }

    return true;
}

int main(int argc, char **argv) {
    int nodeCount = 0;
    int edgeCount = 0;
    int seed = 0;
    bool planar = false;
    std::unique_ptr<ConsecutiveOnesInterface> dumpT;

    int c;
    while ((c = getopt(argc, argv, "n:m:ps:t:")) != -1) {
        switch (c) {
            case 'n':
                nodeCount = std::stoi(optarg);
                break;
            case 'm':
                edgeCount = std::stoi(optarg);
                break;
            case 'p':
                planar = true;
                break;
            case 's':
                seed = std::stoi(optarg);
                break;
            case 't':
                dumpT = getAdapter(optarg);
                break;
            default:
                std::cerr << "Invalid arguments" << std::endl;
                return 1;
        }
    }

    if (!dumpT)
        dumpT = std::make_unique<UnionFindPCTreeAdapter>();

    if (optind >= argc) {
        std::cerr << "Expected output directory for matrices" << std::endl;
        return 1;
    }

    if (nodeCount < 1) {
        std::cerr << "Missing node count parameter." << std::endl;
        return 1;
    }
    if (edgeCount < 1) {
        std::cerr << "Missing edge count parameter." << std::endl;
        return 1;
    }

    json graphJson;
    std::filesystem::path matrixDir = argv[optind++];

    if (!std::filesystem::exists(matrixDir)) {
        std::cerr << "Output directory does not exist." << std::endl;
        return 1;
    }

    graphJson["nodes"] = nodeCount;
    graphJson["edges"] = edgeCount;
    graphJson["seed"] = seed;
    graphJson["planar"] = planar;
    std::string id;
    std::stringstream sb;
    sb << "G-n" << nodeCount << "-m" << edgeCount << "-s" << seed << "-p" << planar;
    graphJson["id"] = id = sb.str();

    Graph G;
    setSeed(seed);
    randomPlanarBiconnectedGraph(G, nodeCount, edgeCount);
    if (!planar) {
        int added = 0;
        while (ogdf::isPlanar(G)) {
            node n1 = G.chooseNode();
            node n2 = G.chooseNode();
            if (G.searchEdge(n1, n2) == nullptr) {
                G.newEdge(n1, n2);
                added++;
            }
        }
        graphJson["added_edges"] = added;
    }

    std::vector<std::string> matrices;
    graphJson["planarity_success"] = isPlanar(G, id, matrices, dumpT.get(), matrixDir, planar);
    graphJson["matrices_count"] = matrices.size();
    graphJson["matrices"] = std::move(matrices);

    std::cout << "GRAPH:" << graphJson << std::endl;
    return 0;
}