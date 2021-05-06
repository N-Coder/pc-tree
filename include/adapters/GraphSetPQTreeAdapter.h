#pragma once

#include "ConsecutiveOnesInterface.h"
#include <graphSetPQ/PQtree.h>

class GraphSetPQTreeAdapter : public ConsecutiveOnesInterface {
private:
    std::unique_ptr<CPQtree> T;
    std::vector<CLeafnode *> leaves;
    int v = 0;

public:
    TreeType type() override {
        return TreeType::GraphSet;
    }

    void initTree(int leaf_count) override {
        v = 0;
        T.reset(nullptr);

        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T = make_unique<CPQtree>(leaf_count);
        std::list<CLeafnode *> leavesList;
        T->FindLeaves(&leavesList);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();

        leaves.assign(leavesList.begin(), leavesList.end());
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        ++v;
        std::list<CLeafnode *> nextRestriction;
        for (int leaf : restriction) {
            nextRestriction.push_back(leaves[leaf]);
        }

        time_point<high_resolution_clock> restrictionStart = high_resolution_clock::now();
        T->prepareReduction(&nextRestriction, v);
        bool result = T->Reduce(&nextRestriction, v);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - restrictionStart).count();

        return result;
    }

    void cleanUp() override {
        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T->ReInitialize();
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
    }

    Bigint possibleOrders() override {
        return T->possibleOrders();
    }
};