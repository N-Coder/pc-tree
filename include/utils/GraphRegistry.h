#pragma once

#include "RegisteredArray.h"

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphObserver.h>

namespace pc_tree::hsu {
    using namespace ogdf;

    template<typename Key>
    class GraphObjectRegistry : public RegistryBase<Key, GraphObjectRegistry<Key>>, public GraphObserver {
        const Graph *m_pGraph;
        ListIterator<GraphObserver *> m_it;

    public:
        explicit GraphObjectRegistry(const Graph *pGraph) : m_pGraph(pGraph) {
            OGDF_ASSERT(m_pGraph);
            m_it = m_pGraph->registerStructure(this);
        }

        explicit GraphObjectRegistry(const Graph &graph) : m_pGraph(&graph) {
            m_it = m_pGraph->registerStructure(this);
        }

        virtual ~GraphObjectRegistry() {
            m_pGraph->unregisterStructure(m_it);
        }

        int keyArrayTableSize() const override;

        int maxKeyIndex() const override;

        bool isKeyAssociated(Key key) const override {
            return key->graphOf() == m_pGraph;
        }

        int keyToIndex(Key key) const override {
            return key->index();
        }

        operator const Graph &() const {
            return *m_pGraph;
        }

        operator const Graph *() const {
            return m_pGraph;
        }

        const Graph &getGraph() const {
            return *m_pGraph;
        }

        using RegistryBase<Key, GraphObjectRegistry<Key>>::reinitArrays;
        using RegistryBase<Key, GraphObjectRegistry<Key>>::enlargeArrayTables;

        void nodeDeleted(node v) override {}

        void nodeAdded(node v) override;

        void edgeDeleted(edge e) override {}

        void edgeAdded(edge e) override;

        void reInit() override {
            reinitArrays();
        }

        void cleared() override {}
    };
}
