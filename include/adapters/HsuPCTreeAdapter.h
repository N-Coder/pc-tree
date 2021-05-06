#pragma once

#include "ConsecutiveOnesInterface.h"
#include <hsuPC/include/PCTree.h>
#include <hsuPC/include/PCNode.h>

class HsuPCTreeAdapter : public ConsecutiveOnesInterface {
private:
    std::unique_ptr<pc_tree::hsu::PCTree> T;
    std::vector<pc_tree::hsu::PCNode *> leaves;

public:
    TreeType type() override {
        return TreeType::HsuPC;
    }

    void initTree(int leaf_count) override {
        T.reset(nullptr);
        leaves.clear();
        leaves.reserve(leaf_count);
        time_point<high_resolution_clock> initStart = high_resolution_clock::now();
        T = std::make_unique<pc_tree::hsu::PCTree>(leaf_count, &leaves);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - initStart).count();
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        std::vector<pc_tree::hsu::PCNode *> nextRestriction;
        nextRestriction.reserve(restriction.size());
        for (int nr : restriction) {
            nextRestriction.push_back(leaves[nr]);
        }

        time_point<high_resolution_clock> restrictionStart = high_resolution_clock::now();
        bool result = T->makeConsecutive(nextRestriction);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - restrictionStart).count();

        return result;
    }

    int terminalPathLength() override {
        return T->getTerminalPathLength();
    }

    Bigint possibleOrders() override {
        return T->possibleOrders();
    }

    bool isValidOrder(const std::vector<int> &order) override {
        std::vector<pc_tree::hsu::PCNode *> nodeOrder;
        nodeOrder.reserve(order.size());
        for (int nr : order) {
            nodeOrder.push_back(leaves[nr]);
        }
        return T->isValidOrder(nodeOrder);
    }

    ostream &uniqueID(ostream &os) override {
        return T->uniqueID(os, [](std::ostream &os, pc_tree::hsu::PCNode *n, int pos) -> void {
            os << pos;
            if (n->getNodeType() != pc_tree::hsu::PCTree::PCNodeType::Leaf) os << ":";
        }, [](pc_tree::hsu::PCNode *a, pc_tree::hsu::PCNode *b) -> bool {
            return a->index() < b->index();
        });
    }

    json stats() override {
        int p_nodes, c_nodes, sum_p_deg, sum_c_deg;
        T->getNodeStats(p_nodes, c_nodes, sum_p_deg, sum_c_deg);
        int inner_nodes = p_nodes + c_nodes;
        std::stringstream sb;
//        sb << *T;
        json stats{
                {"p_frac",      ((double) p_nodes) / inner_nodes},
                {"c_frac",      ((double) c_nodes) / inner_nodes},
                {"avg_deg",     ((double) sum_p_deg + sum_c_deg) / inner_nodes},
                {"avg_p_deg",   ((double) sum_p_deg) / p_nodes},
                {"avg_c_deg",   ((double) sum_c_deg) / c_nodes},
                {"p_nodes",     p_nodes},
                {"c_nodes",     c_nodes},
                {"inner_nodes", inner_nodes},
                {"leaves",      T->getLeafCount()},
                {"tree_size",   inner_nodes + T->getLeafCount()},
//                {"tree",        sb.str()},
        };
//        stats["order"] = json::array();
//        for (auto n :T->currentLeafOrder()) {
//            stats["order"].push_back(n->index());
//        }
        return stats;
    }
};