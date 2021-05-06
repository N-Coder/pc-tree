#pragma once

#include "PCEnum.h"
#include "utils/RegisteredArray.h"
#include "utils/RegisteredElementSet.h"

namespace pc_tree {
    template<class Key>
    class PCTreeRegistry : public ogdf::RegistryBase<Key, PCTreeRegistry<Key>> {
        PCTree *m_pTree;
        int *m_nextKeyIndex;

    public:
        PCTreeRegistry(PCTree *pcTree, int *nextKeyIndex) : m_pTree(pcTree), m_nextKeyIndex(nextKeyIndex) {}

        bool isKeyAssociated(Key key) const override;

        int keyToIndex(Key key) const override;

        int keyArrayTableSize() const override {
            return ogdf::RegistryBase<Key, PCTreeRegistry<Key>>::calculateTableSize(*m_nextKeyIndex);
        }

        int maxKeyIndex() const override {
            return (*m_nextKeyIndex) - 1;
        }

        operator PCTree &() const {
            return *m_pTree;
        }

        operator PCTree *() const {
            return m_pTree;
        }

        PCTree &getTree() const {
            return *m_pTree;
        }
    };

    template<typename Value>
    using PCTreeNodeArray = ogdf::RegisteredArray<PCTreeRegistry<PCNode *>, PCNode *, Value>;

    using PCTreeNodeSet = ogdf::RegisteredElementSet<PCNode *, PCTreeRegistry<PCNode *>>;
}