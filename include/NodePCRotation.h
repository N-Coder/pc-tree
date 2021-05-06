#pragma once

#include "PCTree.h"
#include "utils/RegisteredElementSet.h"


namespace pc_tree::hsu {
    class NodePCRotation : public PCTree {
        Graph *m_G;
        node m_n;

        PCTreeNodeArray<edge> incidentEdgeForLeaf;
        PCTreeNodeArray<node> graphNodeForInnerNode;
        PCTreeNodeArray<List<edge>> bundleEdgesForLeaf;

    public:
        explicit NodePCRotation(Graph &G, node n, bool mapBundleEdges = true);

        static node getTrivialPartnerPole(Graph &G, node n);

        node getTrivialPartnerPole() const;

        void generateLeafForIncidentEdgeMapping(EdgeArray<PCNode *> &mapping) const {
            for (auto leaf : getLeaves()) {
                mapping[getIncidentEdgeForLeaf(leaf)] = leaf;
            }
        }

        edge getIncidentEdgeForLeaf(PCNode *n) const {
            return incidentEdgeForLeaf[n];
        }

        void generateInnerNodeForGraphNodeMapping(NodeArray<PCNode *> &mapping) const {
            for (auto leaf : getLeaves()) {
                mapping[getGraphNodeForInnerNode(leaf)] = leaf;
            }
        }

        node getGraphNodeForInnerNode(PCNode *n) const {
            return graphNodeForInnerNode[n];
        }

        void generatePartnerEdgesForIncidentEdge(EdgeArray<const List<edge> *> &mapping) const {
            for (auto leaf : getLeaves()) {
                mapping[getIncidentEdgeForLeaf(leaf)] = &getPartnerEdgesForLeaf(leaf);
            }
        }

        const List<edge> &getPartnerEdgesForLeaf(PCNode *l) const {
            return bundleEdgesForLeaf[l];
        }

        bool knowsPartnerEdges() const {
            return *bundleEdgesForLeaf.registeredAt() == this;
        }

        Graph *getGraph() const {
            return m_G;
        }

        node getNode() const {
            return m_n;
        }
    };

    struct GraphNotPlanarException : public std::exception {
    public:
        virtual const char *what() {
            return "Graph is not planar";
        }
    };
}
