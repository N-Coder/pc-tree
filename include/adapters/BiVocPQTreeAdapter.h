#pragma once

#include "ConsecutiveOnesInterface.h"
#include <bivocPQ/pqtree.h>

class BiVocPQTreeAdapter : public ConsecutiveOnesInterface {
    using PQTreeType = bivoc::pqtree<int>;

private:
    std::unique_ptr<PQTreeType> T;

public:
    TreeType type() override {
        return TreeType::BiVoc;
    }

    void initTree(int leaf_count) override {
        T.reset(nullptr);

        set<int> initSet;
        for (int i = 0; i < leaf_count; i++) {
            initSet.insert(i);
        }

        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T = std::make_unique<PQTreeType>(initSet);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        set<int> restrictionSet{restriction.begin(), restriction.end()};
        time_point<high_resolution_clock> start = high_resolution_clock::now();
        bool result = T->reduce(restrictionSet);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
        return result;
    }

    void cleanUp() override {
        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T->cleanUp();
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
    }

    Bigint possibleOrders() override {
        return T->possibleOrders();
    }
};