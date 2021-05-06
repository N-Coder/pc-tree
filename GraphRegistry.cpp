#include "utils/GraphRegistry.h"

using namespace pc_tree::hsu;

template<>
int GraphObjectRegistry<node>::keyArrayTableSize() const {
    return m_pGraph->nodeArrayTableSize();
}

template<>
int GraphObjectRegistry<node>::maxKeyIndex() const {
    return m_pGraph->maxNodeIndex();
}

template<>
void GraphObjectRegistry<node>::nodeAdded(node v) {
    enlargeArrayTables();
}

template<>
void GraphObjectRegistry<node>::edgeAdded(edge e) {
}


template<>
int GraphObjectRegistry<edge>::keyArrayTableSize() const {
    return m_pGraph->edgeArrayTableSize();
}

template<>
int GraphObjectRegistry<edge>::maxKeyIndex() const {
    return m_pGraph->maxEdgeIndex();
}

template<>
void GraphObjectRegistry<edge>::nodeAdded(node v) {
}

template<>
void GraphObjectRegistry<edge>::edgeAdded(edge e) {
    enlargeArrayTables();
}


template<>
int GraphObjectRegistry<adjEntry>::keyArrayTableSize() const {
    return m_pGraph->adjEntryArrayTableSize();
}

template<>
int GraphObjectRegistry<adjEntry>::maxKeyIndex() const {
    return m_pGraph->maxAdjEntryIndex();
}

template<>
void GraphObjectRegistry<adjEntry>::nodeAdded(node v) {
}

template<>
void GraphObjectRegistry<adjEntry>::edgeAdded(edge e) {
    enlargeArrayTables();
}
