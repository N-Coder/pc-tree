#include "PCTree.h"
#include "adapters/C1IUtils.h"
#include "adapters/UnionFindPCTreeAdapter.h"

#include <ogdf/basic/basic.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/simple_graph_alg.h>
#include <ogdf/basic/graph_generators.h>

#include <iostream>
#include <getopt.h>

PCNodeType randomNodeType(int out_degree, double p_fract) {
    if (out_degree < 1) {
        return PCNodeType::Leaf;
    } else if (out_degree < 3 || randomDouble(0, 1) >= p_fract) {
        return PCNodeType::CNode;
    } else {
        return PCNodeType::PNode;
    }
}

int main(int argc, char **argv) {
    int nodeCount = 0;
    int degree = 0;
    int width = 0;
    int seed = 0;
    double p_fract = 0.5;
    bool regular = false;
    bool circular = false;

    int c;
    while ((c = getopt(argc, argv, "cn:d:p:rs:w:")) != -1) {
        switch (c) {
            case 'c':
                circular = true;
                break;
            case 'n':
                nodeCount = std::stoi(optarg);
                break;
            case 'd':
                degree = std::stoi(optarg);
                break;
            case 'p':
                p_fract = std::stod(optarg);
                break;
            case 'r':
                regular = true;
                break;
            case 's':
                seed = std::stoi(optarg);
                break;
            case 'w':
                width = std::stoi(optarg);
                break;
            default:
                std::cerr << "Invalid arguments" << std::endl;
                return 1;
        }
    }

    if (nodeCount < 1) {
        std::cerr << "Missing node count parameter." << std::endl;
        return 1;
    }
    if (regular && width > 0) {
        std::cerr << "Regular graphs don't support maxWidth." << std::endl;
        return 1;
    }
    if (regular && degree < 1) {
        std::cerr << "Regular graphs need a node degree." << std::endl;
        return 1;
    }

    json graphJson;
    graphJson["nodes"] = nodeCount;
    graphJson["degree"] = degree;
    graphJson["width"] = width;
    graphJson["seed"] = seed;
    graphJson["regular"] = regular;
    graphJson["p_fract"] = p_fract;
    std::string id;
    {
        std::stringstream sb;
        sb << "T-n" << nodeCount << "-d" << degree << "-w" << width << "-s" << seed << "-p" << p_fract << "-r" << regular << "-c" << circular;
        graphJson["id"] = id = sb.str();
    }

    Graph G;
    setSeed(seed);
    if (regular) {
        regularTree(G, nodeCount, degree);
    } else {
        randomTree(G, nodeCount, degree, width);
    }

    node root;
    bool tree = ogdf::isArborescence(G, root);
    OGDF_ASSERT(tree);
    NodeArray<PCNode *> mapping(G);
    std::queue<node> todo{{root}};
    PCTree T;
    mapping[root] = T.newNode(randomNodeType(root->outdeg(), p_fract), nullptr);
    if (!circular)
        T.newNode(PCNodeType::Leaf, mapping[root]);

    while (!todo.empty()) {
        node next = todo.front();
        todo.pop();

        for (adjEntry adj: next->adjEntries) {
            if (!adj->isSource()) continue;
            node twin = adj->twinNode();
            while (twin->outdeg() == 1) {
                twin = twin->adjEntries.head()->isSource() ? twin->adjEntries.head()->twinNode() : twin->adjEntries.tail()->twinNode();
            }
            PCNodeType type = randomNodeType(twin->outdeg(), p_fract);
            mapping[twin] = T.newNode(type, mapping[next]);
            todo.push(twin);
        }
    }
    OGDF_ASSERT(T.checkValid());
    {
        std::stringstream sb;
        sb << T;
        graphJson["tree"] = sb.str();
    }

    std::vector<std::vector<int>> restrictions;
    getRestrictions(T, restrictions);
    UnionFindPCTreeAdapter adapter;
    dumpMatrix(T.getLeafCount(), restrictions, id, graphJson["single_matrix"] = json::object(), &adapter);
    graphJson["single_matrix"]["parent_id"] = id;
    std::cout << graphJson << std::endl;
    return 0;
}