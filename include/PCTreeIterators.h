#pragma once

#include "PCTree.h"

#include <stack>
#include <queue>

namespace pc_tree::hsu {
    using namespace ogdf;

    template<bool dfs>
    class FilteringPCTreeSearch : public std::iterator<std::input_iterator_tag, PCArc *> {
        using container_type = typename std::conditional<dfs, std::stack<PCArc *>, std::queue<PCArc *>>::type;

        container_type m_pending;
        PCTreeArcArray<bool> m_visited;
        std::function<bool(PCArc * )> m_visit;
        std::function<bool(PCArc * )> m_descend;

    public:
        template<typename T>
        static bool return_true(T t) { return true; }

        explicit FilteringPCTreeSearch() {}

        template<typename Container>
        explicit FilteringPCTreeSearch(const PCTree &T, Container &arcs,
                                       std::function<bool(PCArc * )> visit = return_true<PCArc *>,
                                       std::function<bool(PCArc * )> descend_from = return_true<PCArc *>)
                : m_pending(), m_visited(T, false), m_visit(visit), m_descend(descend_from) {
            for (PCArc *a : arcs) {
                m_pending.push(a);
            }
        }

        explicit FilteringPCTreeSearch(const PCTree &T, std::initializer_list<PCArc *> arcs,
                                       std::function<bool(PCArc * )> visit = return_true<PCArc *>,
                                       std::function<bool(PCArc * )> descend_from = return_true<PCArc *>)
                : m_pending(arcs), m_visited(T, false), m_visit(visit), m_descend(descend_from) {}

        bool operator==(const FilteringPCTreeSearch &rhs) const {
            return m_pending == rhs.m_pending;
        }

        bool operator!=(const FilteringPCTreeSearch &rhs) const {
            return m_pending != rhs.m_pending;
        }

        FilteringPCTreeSearch &begin() {
            return *this;
        }

        FilteringPCTreeSearch end() const {
            return FilteringPCTreeSearch();
        }

        PCArc *top() {
            OGDF_ASSERT(!m_pending.empty());
            if constexpr (dfs) {
                return m_pending.top();
            } else {
                return m_pending.front();
            }
        }

        PCArc *operator*() {
            return top();
        }

        //! Increment operator (prefix, returns result).
        FilteringPCTreeSearch &operator++() {
            next();
            return *this;
        }

        //! Increment operator (postfix, returns previous value).
        OGDF_DEPRECATED("Calling DelimitedBFS++ will copy the array of visited nodes")
        FilteringPCTreeSearch operator++(int) {
            FilteringPCTreeSearch before = *this;
            next();
            return before;
        }

        void next() {
            OGDF_ASSERT(!m_pending.empty());

            PCArc *a = top();
            m_pending.pop();

            OGDF_ASSERT(!m_visited[a]);
            m_visited[a] = true;
            if (m_descend(a)) {
                for (PCArc *next : *a) {
                    m_visited[next] = true;
                    PCArc *twin = next->getTwinArc();
                    if (!m_visited[twin] && m_visit(twin)) m_pending.push(twin);
                }
            }
            while (!m_pending.empty() && m_visited[top()]) m_pending.pop();
        }

        operator bool() const {
            return valid();
        }

        bool valid() const {
            return !m_pending.empty();
        }

        void append(PCArc *a) {
            m_pending.push(a);
        }

        bool hasVisited(PCArc *a) const {
            return m_visited[a];
        }

        int pendingCount() const {
            return m_pending.size();
        }
    };

    using FilteringPCTreeDFS = FilteringPCTreeSearch<true>;
    using FilteringPCTreeBFS = FilteringPCTreeSearch<false>;
}
