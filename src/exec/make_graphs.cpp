#include <ogdf/basic/basic.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/basic/graph_generators.h>

using namespace ogdf;

int main(int argc, char **argv) {
    int nodeCount = std::stoi(argv[1]);
    int edgeCount = std::stoi(argv[2]);
    int copies = std::stoi(argv[3]);
    int seed = std::stoi(argv[4]);
    bool enforcePlanar = std::stoi(argv[5]);
    std::string outputFolder = argv[6];

    setSeed(seed);
    for (int i = 0; i < copies; i++) {
        Graph G;
        if (enforcePlanar) {
            randomPlanarBiconnectedGraph(G, nodeCount, edgeCount);
        } else {
            randomBiconnectedGraph(G, nodeCount, edgeCount);
        }

        std::stringstream ss;
        ss << outputFolder << "/graphn" << nodeCount << "e" << edgeCount << "s" << seed << "i" << i
           << (enforcePlanar ? "planar" : "") << ".gml";
        std::string s = ss.str();
        GraphIO::write(G, s, GraphIO::writeGML);
    }

}