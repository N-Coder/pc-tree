#pragma once

#include "ConsecutiveOnesInterface.h"
#include "PCTree.h"

class UnionFindPCTreeAdapter : public ConsecutiveOnesInterface {
private:
    std::unique_ptr<PCTree> T;
    std::vector<PCNode *> leaves;

public:
    TreeType type() override {
        return TreeType::UFPC;
    }

    void initTree(int leaf_count) override {
        T.reset(nullptr);
        leaves.clear();
        leaves.reserve(leaf_count);
        time_point<high_resolution_clock> initStart = high_resolution_clock::now();
        T = std::make_unique<PCTree>(leaf_count, &leaves);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - initStart).count();
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        std::vector<PCNode *> nextRestriction;
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
        std::vector<PCNode *> nodeOrder;
        nodeOrder.reserve(order.size());
        for (int nr : order) {
            nodeOrder.push_back(leaves[nr]);
        }
        return T->isValidOrder(nodeOrder);
    }

    std::ostream &uniqueID(std::ostream &os) override {
        return T->uniqueID(os, uid_utils::nodeToPosition);
    }

    json stats() override {
        // int sum_p_deg, sum_c_deg;
        // T.getNodeStats(p_nodes, c_nodes, sum_p_deg, sum_c_deg);
        int inner_nodes = T->getPNodeCount() + T->getCNodeCount();
//        std::stringstream sb;
//        sb << *T;
        json stats{
//                {"p_frac",      ((double) T->getPNodeCount()) / inner_nodes},
//                {"c_frac",      ((double) T->getCNodeCount()) / inner_nodes},
                // {"avg_deg",     ((double) sum_p_deg + sum_c_deg) / inner_nodes},
                // {"avg_p_deg",   ((double) sum_p_deg) / T.getPNodeCount()},
                // {"avg_c_deg",   ((double) sum_c_deg) / T.getCNodeCount()},
                {"p_nodes",     (T->getPNodeCount())},
                {"c_nodes",     (T->getCNodeCount())},
//                {"inner_nodes", inner_nodes},
                {"leaves",      T->getLeafCount()},
//                {"tree_size",   inner_nodes + T->getLeafCount()},
//                {"tree",        sb.str()},
        };
//        stats["order"] = json::array();
//        for (PCNode *n :T->currentLeafOrder()) {
//            stats["order"].push_back(n->index());
//        }
        return stats;
    }
};
