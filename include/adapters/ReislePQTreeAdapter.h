#pragma once

#include "ConsecutiveOnesInterface.h"
#include <creislePQ/PQTree.h>

class ReislePQTreeAdapter : public ConsecutiveOnesInterface {
    using PQTreeType = creisle::PQTree;

private:
    std::unique_ptr<PQTreeType> T;

public:
    TreeType type() override {
        return TreeType::Reisle;
    }

    void initTree(int leaf_count) override {
        T.reset(nullptr);

        vector<int> initVector;
        initVector.reserve(leaf_count);
        for (int i = 0; i < leaf_count; i++) {
            initVector.push_back(i);
        }

        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T = std::make_unique<PQTreeType>(initVector);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        time_point<high_resolution_clock> start = high_resolution_clock::now();
        bool result = T->set_consecutive(restriction);
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