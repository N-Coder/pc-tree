#pragma once

#include <ogdf/basic/NodeArray.h>
#include <ogdf/basic/AdjEntryArray.h>
#include <ogdf/basic/EdgeArray.h>
#include <ogdf/basic/List.h>

namespace ogdf {
    template<class RegisteredElement, class RegistryType, bool SupportFastSizeQuery = true>
    class RegisteredElementSet {
    public:
        using ListType = typename std::conditional<SupportFastSizeQuery, List<RegisteredElement>, ListPure<RegisteredElement>>::type;
        using RegisteredArrayType = typename std::conditional<std::is_same<RegisteredElement, node>::value, NodeArray<ListIterator<node>>,
                typename std::conditional<std::is_same<RegisteredElement, edge>::value, EdgeArray<ListIterator<edge>>,
                typename std::conditional<std::is_same<RegisteredElement, adjEntry>::value, AdjEntryArray<ListIterator<adjEntry>>,
                        RegisteredArray<RegistryType, RegisteredElement, ListIterator<RegisteredElement>>>::type>::type>::type;

        //! Creates an empty node set associated with graph \p G.
        explicit RegisteredElementSet(const RegistryType &G) : m_it(G) {}

        //! Creates an empty node set associated with no graph.
        explicit RegisteredElementSet() : m_it() {}

        //! Reinitializes the set. Associates the set with no graph.
        void init() {
            m_it.init();
            m_elements.clear();
        }

        //! Reinitializes the set. Associates the set with graph \p G.
        void init(const RegistryType &G) {
            m_it.init(G);
            m_elements.clear();
        }

        //! Inserts node \p v into this set.
        /**
         * This operation has constant runtime.
         * If the node is already contained in this set, nothing happens.
         *
         * \pre \p v is a node in the associated graph.
         */
        void insert(RegisteredElement v) {
            ListIterator<RegisteredElement> &itV = m_it[v];
            if (!itV.valid()) {
                itV = m_elements.pushBack(v);
            }
        }

        //! Inserts node \p v into this set.
        /**
         * This operation has constant runtime.
         * If the node is already contained in this set, nothing happens.
         *
         * \pre \p v is a node in the associated graph.
         */
        void merge(RegisteredElementSet<RegisteredElement, RegistryType, SupportFastSizeQuery> &other) {
            OGDF_ASSERT(other.m_it.graphOf() == m_it.graphOf());
            for (const RegisteredElement &v : other.m_elements) {
                insert(v);
            }
        }

        //! Removes node \p v from this set.
        /**
         * This operation has constant runtime.
         *
         * \pre \p v is a node in the associated graph.
         * If the node is not contained in this set, nothing happens.
         */
        void remove(RegisteredElement v) {
            ListIterator<RegisteredElement> &itV = m_it[v];
            if (itV.valid()) {
                m_elements.del(itV);
                itV = ListIterator<RegisteredElement>();
            }
        }

        //! Removes all nodes from this set.
        /**
         * After this operation, this set is empty and still associated with the same graph.
         * The runtime of this operations is linear in the #size().
         */
        void clear() {
            m_it.fill(ListIterator<RegisteredElement>());
            m_elements.clear();
        }

        //! Returns \c true iff node \p v is contained in this set.
        /**
         * This operation has constant runtime.
         *
         * \pre \p v is a node in the associated graph.
         */
        bool isMember(RegisteredElement v) const {
            return m_it[v].valid();
        }

        //! Returns a reference to the list of nodes contained in this set.
        const ListType &elements() const {
            return m_elements;
        }

        //! Returns the associated graph
        [[nodiscard]] const RegistryType *registeredAt() const {
            return m_it.registeredAt();
        }

        //! Returns the associated graph
        OGDF_DEPRECATED("only here for compatibility with PCNode/EdgeArray")
        [[nodiscard]] const RegistryType *graphOf() const {
            return m_it.graphOf();
        }

        //! Returns the number of nodes in this set.
        /**
         * This operation has either linear or constant runtime, depending on \a SupportFastSizeQuery.
         */
        [[nodiscard]] int size() const {
            return m_elements.size();
        }

        //! Copy constructor.
        template<bool OtherSupportsFastSizeQuery>
        RegisteredElementSet(const RegisteredElementSet<RegisteredElement, RegistryType, OtherSupportsFastSizeQuery> &other) {
            this = other;
        }

        //! Assignment operator.
        template<bool OtherSupportsFastSizeQuery>
        RegisteredElementSet &operator=(const RegisteredElementSet<RegisteredElement, RegistryType, OtherSupportsFastSizeQuery> &other) {
            m_elements.clear();
            m_it.init(*other.graphOf());
            for (RegisteredElement v : other.elements()) {
                insert(v);
            }
        }

    private:
        //! #m_it[\a v] contains the list iterator pointing to \a v if \a v is contained in this set,
        //! or an invalid list iterator otherwise.
        RegisteredArrayType m_it;

        //! The list of nodes contained in this set.
        ListType m_elements;
    };

}

