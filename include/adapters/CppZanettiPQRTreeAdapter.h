#pragma once

#include "zanettiPQR/PQRTree.h"

using namespace cpp_zanetti;

class CppZanettiPQRTreeAdapter : public ConsecutiveOnesInterface {
private:

    std::unique_ptr<PQRTree> T;
    std::vector<Leaf *> leaves;

public:
    TreeType type() override {
        return TreeType::CppZanetti;
    }

    void initTree(int leaf_count) override {
        T.reset(nullptr);
        leaves.clear();
        leaves.reserve(leaf_count);
        time_point<high_resolution_clock> initStart = high_resolution_clock::now();
        T = std::make_unique<PQRTree>(leaf_count, leaves);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - initStart).count();
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        std::vector<Leaf *> nextRestriction;
        nextRestriction.reserve(restriction.size());
        for (int nr : restriction) {
            nextRestriction.push_back(leaves[nr]);
        }

        time_point<high_resolution_clock> restrictionStart = high_resolution_clock::now();
        bool result = T->reduce(nextRestriction);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - restrictionStart).count();

        return result;
    }

    std::string toString() override {
        return T->toString();
    }

    Bigint possibleOrders() override {
        return T->getPossibleOrders();
    }


    std::ostream &uniqueID(std::ostream &os) override {
        os << T->uniqueID();
        return os;
    };

};