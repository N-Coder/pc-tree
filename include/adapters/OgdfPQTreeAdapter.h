#pragma once

#include "ConsecutiveOnesInterface.h"
#include <ogdf/basic/PQTree.h>
#include <variant>

using namespace ogdf;

class CustomPQTree : public PQTree<int, void *, void *> {
public:
    bool Bubble(SListPure<PQLeafKey<int, void *, void *> *> &leafKeys) override {
        return PQTree::Bubble(leafKeys);
    }
};


class OgdfPQTreeAdapter : public ConsecutiveOnesInterface {
    using PQTreeType = CustomPQTree;
    using PQLeafKeyType = PQLeafKey<int, void *, void *>;
    using PQNodeType = PQNode<int, void *, void *>;

private:
    std::unique_ptr<PQTreeType> T;
    std::vector<PQLeafKeyType> leaves;

public:
    TreeType type() override {
        return TreeType::OGDF;
    }

    void initTree(int leaf_count) override {
        T.reset(nullptr);
        leaves.clear();
        leaves.reserve(leaf_count);

        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T = std::make_unique<PQTreeType>();
        SListPure<PQLeafKeyType *> initList;
        for (int i = 0; i < leaf_count; i++) {
            leaves.emplace_back(i);
            initList.pushBack(&leaves.back());
        }
        T->Initialize(initList);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
    }

    bool applyRestriction(const std::vector<int> &restriction) override {
        SListPure<PQLeafKeyType *> nextRestriction;
        for (int leaf : restriction) {
            nextRestriction.pushBack(&leaves[leaf]);
        }

        time_point<high_resolution_clock> start = high_resolution_clock::now();
        bool result = T->Reduction(nextRestriction);
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();

        return result;
    }

    void cleanUp() override {
        time_point<high_resolution_clock> start = high_resolution_clock::now();
        T->emptyAllPertinentNodes();
        time = duration_cast<nanoseconds>(high_resolution_clock::now() - start).count();
    }

    Bigint possibleOrders() override {
        return possibleOrders(*T);
    }

    template<class X, class Y, class Z>
    static Bigint possibleOrders(PQTree<X, Y, Z> &T) {
        using PQNodeType = PQNode<X, Y, Z>;
        Bigint orders(1);

        std::queue<PQNodeType *> queue;
        queue.push(T.root());

        while (!queue.empty()) {
            PQNodeType *nextNode = queue.front();
            queue.pop();
            if (nextNode->type() == PQNodeRoot::PQNodeType::PNode) {
                int childCount = nextNode == T.root() ? nextNode->childCount() - 1 : nextNode->childCount();
                orders *= factorial(childCount);
                PQNodeType *firstChild = nextNode->referenceChild();
                PQNodeType *current = firstChild;
                do {
                    queue.push(current);
                    current = current->getSib(PQNodeRoot::SibDirection::Left);
                } while (current != firstChild);
            } else if (nextNode->type() == PQNodeRoot::PQNodeType::QNode) {
                orders *= 2;
                PQNodeType *firstChild = nextNode->getEndmost(PQNodeRoot::SibDirection::Left);
                PQNodeType *current = firstChild;
                PQNodeType *previous = nullptr;
                do {
                    queue.push(current);
                    PQNodeType *tmp = current;
                    current = current->getNextSib(previous);
                    previous = tmp;
                } while (current != nullptr && current != firstChild);
            }
        }

        return orders;
    }

    std::ostream &uniqueID(std::ostream &os) override {
        if (leaves.size() < 3) {
            return os << "too small";
        }

        std::map<PQNodeType *, int> order;
        std::queue<PQNodeType *> labelingQueue;
        int i = 0;
        for (auto leaf : leaves) {
            order[leaf.nodePointer()] = i++;
            if (leaf.nodePointer() != leaves.back().nodePointer()) {
                labelingQueue.push(leaf.nodePointer());
            }
        }

        T->emptyAllPertinentNodes();
        SListPure<PQLeafKeyType *> initRestriction;
        T->front(T->root(), initRestriction);
        bool bub = T->Bubble(initRestriction); // Ensure that parent pointers are valid
        OGDF_ASSERT(bub);
        T->emptyAllPertinentNodes();

        std::map<PQNodeType *, int> fullNeighborCount;
        PQNodeType *lastNode = nullptr;
        while (!labelingQueue.empty()) {
            PQNodeType *n = labelingQueue.front();
            labelingQueue.pop();

            if (n->type() != ogdf::PQNodeRoot::PQNodeType::Leaf) {
                order[n] = i++;
            }

            lastNode = n;
            std::vector<PQNodeType *> neighbors;
            getAllNeighbors(n, neighbors, true);
            OGDF_ASSERT(neighbors.size() == neighborCount(n));
            OGDF_ASSERT(neighbors.size() >= 3 || n->type() == ogdf::PQNodeRoot::PQNodeType::Leaf);
            auto partialNeighborIt = std::find_if(std::begin(neighbors), std::end(neighbors), [&](PQNodeType *a) {
                return fullNeighborCount[a] < neighborCount(a) - 1;
            });

            if (partialNeighborIt == std::end(neighbors)) {
                continue;
            } else {
                ++fullNeighborCount[*partialNeighborIt];
                if (fullNeighborCount[*partialNeighborIt] == neighborCount(*partialNeighborIt) - 1) {
                    labelingQueue.push(*partialNeighborIt);
                }
            }
        }
        OGDF_ASSERT(lastNode == leaves.back().nodePointer()->parent());
        std::stack<std::variant<PQNodeType *, std::string>> stack;
        stack.push(lastNode);

        while (!stack.empty()) {
            auto next = stack.top();
            stack.pop();

            if (std::holds_alternative<std::string>(next)) {
                os << std::get<std::string>(next);
                continue;
            }

            PQNodeType *node = std::get<PQNodeType *>(next);
            if (node->type() == ogdf::PQNodeRoot::PQNodeType::Leaf) {
                os << order[node];
                continue;
            }

            std::list<PQNodeType *> children;
            getAllNeighbors(node, children, true);
            if (node != lastNode) {
                PQNodeType *informedNeighbor = nullptr;
                for (PQNodeType *neigh : children) {
                    if (order[neigh] > order[node]) {
                        OGDF_ASSERT(informedNeighbor == nullptr);
                        informedNeighbor = neigh;
                    }
                }
                OGDF_ASSERT(informedNeighbor != nullptr);
                if (node->type() == ogdf::PQNodeRoot::PQNodeType::QNode) {
                    while (informedNeighbor != children.back()) {
                        children.push_front(children.back());
                        children.pop_back();
                    }
                }
                children.remove(informedNeighbor);
            }
            if (node->type() == ogdf::PQNodeRoot::PQNodeType::QNode) {
                os << order[node] << ":";
                os << "[";
                stack.push("]");

                if (node == lastNode) {
                    PQNodeType *minChild = *std::min_element(children.begin(), children.end(), [&order](PQNodeType *elem, PQNodeType *min) {
                        return order[elem] < order[min];
                    });
                    while (children.front() != minChild) {
                        children.push_back(children.front());
                        children.pop_front();
                    }
                    PQNodeType *second = *(++children.begin());
                    if (order[second] > order[children.back()]) {
                        children.push_back(children.front());
                        children.pop_front();
                        children.reverse();
                    }
                    second = *(++children.begin());
                    OGDF_ASSERT(children.front() == minChild);
                    OGDF_ASSERT(order[second] <= order[children.back()]);
                }

                if (order[children.front()] > order[children.back()])
                    std::reverse(children.begin(), children.end());
            } else {
                OGDF_ASSERT(node->type() == ogdf::PQNodeRoot::PQNodeType::PNode);
                os << order[node] << ":";
                if (neighborCount(node) <= 3) {
                    os << "[";
                    stack.push("]");
                } else {
                    os << "(";
                    stack.push(")");
                }
                children.sort([&order](PQNodeType *a, PQNodeType *b) {
                    return order[a] < order[b];
                });
            }
            OGDF_ASSERT(order[children.front()] < order[children.back()]);

            bool space = false;
            for (PQNodeType *c : children) {
                if (space)
                    stack.push(", ");
                stack.push(c);
                space = true;
            }
        }

        return os;
    };

    PQNodeType *getNeighbor(PQNodeType *node, PQNodeType *neighbor) {
        if (neighborCount(neighbor) != 2) {
            return neighbor;
        } else {
            std::list<PQNodeType *> neighbors;
            getAllNeighbors(neighbor, neighbors, false);
            OGDF_ASSERT(neighbors.size() == 2);
            if (neighbors.front() == node) {
                return getNeighbor(neighbor, neighbors.back());
            } else {
                OGDF_ASSERT(neighbors.back() == node);
                return getNeighbor(neighbor, neighbors.front());
            }
        }
    }

    int neighborCount(PQNodeType *n) {
        return n == T->root() ? childCount(n) : childCount(n) + 1;
    }

    static int childCount(PQNodeType *n) {
        if (n->type() == ogdf::PQNodeRoot::PQNodeType::QNode) {
            PQNodeType *firstChild = n->getEndmost(PQNodeRoot::SibDirection::Left);
            PQNodeType *current = firstChild;
            PQNodeType *previous = nullptr;
            int i = 0;
            do {
                OGDF_ASSERT(current->parent() == n);
                PQNodeType *tmp = current;
                current = current->getNextSib(previous);
                previous = tmp;
                ++i;
            } while (current != nullptr && current != firstChild);
            return i;
        } else {
            return n->childCount();
        }
    }

    template<class Container>
    void getChildren(PQNodeType *node, Container &children, bool ignoreDeg2) {
        if (node->type() == ogdf::PQNodeRoot::PQNodeType::PNode) {
            PQNodeType *firstChild = node->referenceChild();
            PQNodeType *current = firstChild;
            do {
                if (ignoreDeg2) {
                    children.push_back(getNeighbor(node, current));
                } else {
                    children.push_back(current);
                }
                current = current->getSib(PQNodeRoot::SibDirection::Left);
            } while (current != firstChild);
        } else if (node->type() == ogdf::PQNodeRoot::PQNodeType::QNode) {
            PQNodeType *firstChild = node->getEndmost(PQNodeRoot::SibDirection::Left);
            PQNodeType *current = firstChild;
            PQNodeType *previous = nullptr;
            do {
                if (ignoreDeg2) {
                    children.push_back(getNeighbor(node, current));
                } else {
                    children.push_back(current);
                }
                PQNodeType *tmp = current;
                current = current->getNextSib(previous);
                previous = tmp;
            } while (current != nullptr && current != firstChild);
        }
    }

    template<class Container>
    void getAllNeighbors(PQNodeType *node, Container &neighbors, bool ignoreDeg2) {
        getChildren(node, neighbors, ignoreDeg2);
        if (node->parent() != nullptr) {
            if (ignoreDeg2) {
                neighbors.push_back(getNeighbor(node, node->parent()));
            } else {
                neighbors.push_back(node->parent());
            }
            OGDF_ASSERT(node != T->root());
        } else {
            OGDF_ASSERT(node == T->root());
        }
    }

};