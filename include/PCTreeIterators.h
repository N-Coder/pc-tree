#pragma once

#include "PCNode.h"
#include "PCEnum.h"

#include <stack>
#include <utility>
#include <queue>

namespace pc_tree {
    class PCNodeIterator {
        friend struct PCNodeChildrenIterable;
        friend struct PCNodeNeighborsIterable;

        PCNode *const node = nullptr;
        PCNode *pred = nullptr;
        PCNode *curr = nullptr;

        PCNodeIterator(PCNode *node, PCNode *pred, PCNode *curr)
                : node(node), pred(pred), curr(curr) {}

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = PCNode;
        using pointer = PCNode *;
        using reference = PCNode &;
        using difference_type = std::ptrdiff_t;

        PCNodeIterator() = default;

        PCNode &operator->() const {
            return *curr;
        }

        PCNode *operator*() const {
            return curr;
        }

        //! Increment operator (prefix, returns result).
        PCNodeIterator &operator++();

        //! Increment operator (postfix, returns previous value).
        PCNodeIterator operator++(int);

        bool operator==(const PCNodeIterator &rhs) const {
            return node == rhs.node &&
                   pred == rhs.pred &&
                   curr == rhs.curr;
        }

        bool operator!=(const PCNodeIterator &rhs) const {
            return !(rhs == *this);
        }

        PCNode *nodeOf() const {
            return node;
        }

        bool isParent();
    };

    struct PCNodeChildrenIterable {
        PCNode *const node;

        explicit PCNodeChildrenIterable(PCNode *node) : node(node) {}

        PCNodeIterator begin() const noexcept;

        PCNodeIterator end() const noexcept;

        unsigned long count() const;
    };

    struct PCNodeNeighborsIterable {
        PCNode *const node;
        PCNode *const first;

        explicit PCNodeNeighborsIterable(PCNode *node, PCNode *first = nullptr) :
                node(node),
                first(first != nullptr ? first : (node->child1 != nullptr ? node->child1 : node->getParent())) {
            if (this->first == nullptr) {
                OGDF_ASSERT(this->node->getDegree() == 0);
            } else {
                OGDF_ASSERT(this->node->isParentOf(this->first) || this->first->isParentOf(this->node));
            }
        }

        PCNodeIterator begin() const noexcept;

        PCNodeIterator end() const noexcept;

        unsigned long count() const;
    };

    template<bool dfs>
    class FilteringPCTreeWalk : public std::iterator<std::input_iterator_tag, PCNode *> {
        using container_type = typename std::conditional<dfs, std::stack<PCNode *>, std::queue<PCNode *>>::type;

        container_type m_pending;
        std::function<bool(PCNode * )> m_visit;
        std::function<bool(PCNode * )> m_descend;

    public:
        static bool return_true(PCNode *n) { return true; }

        explicit FilteringPCTreeWalk() = default;

        explicit FilteringPCTreeWalk(
                const PCTree &T, PCNode *start,
                std::function<bool(PCNode * )> visit = return_true,
                std::function<bool(PCNode * )> descend_from = return_true
        ) : m_pending({start}), m_visit(std::move(visit)), m_descend(std::move(descend_from)) {}

        bool operator==(const FilteringPCTreeWalk &rhs) const {
            return m_pending == rhs.m_pending;
        }

        bool operator!=(const FilteringPCTreeWalk &rhs) const {
            return m_pending != rhs.m_pending;
        }

        FilteringPCTreeWalk &begin() {
            return *this;
        }

        FilteringPCTreeWalk end() const {
            return FilteringPCTreeWalk();
        }

        PCNode *top() {
            OGDF_ASSERT(!m_pending.empty());
            if constexpr (dfs) {
                return m_pending.top();
            } else {
                return m_pending.front();
            }
        }

        PCNode *operator*() {
            return top();
        }

        //! Increment operator (prefix, returns result).
        FilteringPCTreeWalk &operator++() {
            next();
            return *this;
        }

        //! Increment operator (postfix, returns previous value).
        OGDF_DEPRECATED("Calling FilteringPCTreeWalk++ will copy the array of pending nodes")
        FilteringPCTreeWalk operator++(int) {
            FilteringPCTreeWalk before = *this;
            next();
            return before;
        }

        void next() {
            OGDF_ASSERT(!m_pending.empty());
            PCNode *node = top();
            m_pending.pop();
            if (m_descend(node))
                for (PCNode *neigh : node->children())
                    if (m_visit(neigh))
                        m_pending.push(neigh);
        }

        explicit operator bool() const {
            return valid();
        }

        bool valid() const {
            return !m_pending.empty();
        }

        void append(PCNode *a) {
            m_pending.push(a);
        }

        int pendingCount() const {
            return m_pending.size();
        }
    };

    using FilteringPCTreeDFS = FilteringPCTreeWalk<true>;
    using FilteringPCTreeBFS = FilteringPCTreeWalk<false>;
}