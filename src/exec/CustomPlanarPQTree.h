#pragma once
// modified version of ogdf:/src/ogdf/planarity/booth_lueker/PlanarPQTree.cpp

class CustomPlanarPQTree : public PQTree<edge, void *, void *> {
    using PQNodeType = PQNode<edge, void *, void *>;
    using PQLeafKeyType = PQLeafKey<edge, void *, void *>;
    using PQLeafType = PQLeaf<edge, void *, void *>;
    using PQInternalNodeType = PQInternalNode<edge, void *, void *>;
public:
    List<PQNodeType *> pertinentNodes;
    std::vector<List<PQNodeType *>> pertinentFullChildren;
    std::vector<PQNodeRoot::PQNodeStatus> pertinentNodeLabels;
    List<PQNodeType *> pertinentRootFullChildren;
    PQNodeRoot::PQNodeStatus pertinentRootLabel = PQNodeRoot::PQNodeStatus::Empty;
    long restrictionTime = 0;

    CustomPlanarPQTree() : PQTree<edge, void *, void *>() {}

    void savePertinentNodeInfo() {
        pertinentNodes.clear();
        pertinentFullChildren.clear();
        pertinentRootFullChildren.clear();
        pertinentNodeLabels.clear();

        for (PQNodeType *node : *m_pertinentNodes) {
            if (node->status() != PQNodeRoot::PQNodeStatus::ToBeDeleted) {
                pertinentNodes.pushBack(node);
                pertinentFullChildren.push_back(*fullChildren(node));
                pertinentNodeLabels.push_back(node->status());
            }
        }

        if (m_pertinentRoot != nullptr) {
            pertinentRootFullChildren = *fullChildren(m_pertinentRoot);
            pertinentRootLabel = m_pertinentRoot->status();
        }
    }

    void restorePertinentNodeInfo() {
        auto it = pertinentNodes.begin();
        for (int i = 0; i < pertinentNodes.size(); i++) {
            fullChildren(*it)->swap(pertinentFullChildren.at(i));
            (*it)->status(pertinentNodeLabels.at(i));
            ++it;
        }

        if (m_pertinentRoot != nullptr) {
            fullChildren(m_pertinentRoot)->swap(pertinentRootFullChildren);
            m_pertinentRoot->status(pertinentRootLabel);
        }

        m_pertinentNodes->swap(pertinentNodes);
    }

    void ReplaceRoot(SListPure<PQLeafKeyType *> &leafKeys) {
        if (m_pertinentRoot->status() == PQNodeRoot::PQNodeStatus::Full)
            ReplaceFullRoot(leafKeys);
        else
            ReplacePartialRoot(leafKeys);
    }

    // Function ReplaceFullRoot either replaces the full root
    // or one full child of a partial root of a pertinent subtree
    // by a single P-node  with leaves corresponding the keys stored in leafKeys.
    void ReplaceFullRoot(SListPure<PQLeafKeyType *> &leafKeys) {
        if (!leafKeys.empty() && leafKeys.front() == leafKeys.back()) {
            //ReplaceFullRoot: replace pertinent root by a single leaf
            PQLeafType *leafPtr =
                    new PQLeafType(m_identificationNumber++,
                                   PQNodeRoot::PQNodeStatus::Empty,
                                   (PQLeafKeyType *) leafKeys.front());

            exchangeNodes(m_pertinentRoot, (PQNodeType *) leafPtr);
            if (m_pertinentRoot == m_root)
                m_root = (PQNodeType *) leafPtr;
            m_pertinentRoot = nullptr;  // check for this emptyAllPertinentNodes
        } else if (!leafKeys.empty()) // at least two leaves
        {
            PQInternalNodeType *nodePtr = nullptr; // dummy
            //replace pertinent root by a $P$-node
            if ((m_pertinentRoot->type() == PQNodeRoot::PQNodeType::PNode) ||
                (m_pertinentRoot->type() == PQNodeRoot::PQNodeType::QNode)) {
                nodePtr = (PQInternalNodeType *) m_pertinentRoot;
                nodePtr->type(PQNodeRoot::PQNodeType::PNode);
                nodePtr->childCount(0);
                while (!fullChildren(m_pertinentRoot)->empty())
                    removeChildFromSiblings(fullChildren(m_pertinentRoot)->popFrontRet());
            } else if (m_pertinentRoot->type() == PQNodeRoot::PQNodeType::Leaf) {
                nodePtr = new PQInternalNodeType(m_identificationNumber++,
                                                 PQNodeRoot::PQNodeType::PNode,
                                                 PQNodeRoot::PQNodeStatus::Empty);
                exchangeNodes(m_pertinentRoot, nodePtr);
                m_pertinentRoot = nullptr;  // check for this emptyAllPertinentNodes
            }
            addNewLeavesToTree(nodePtr, leafKeys);
        }
    }

    // Function ReplacePartialRoot replaces all full nodes by a single P-node
    // with leaves corresponding the keys stored in leafKeys.
    void ReplacePartialRoot(SListPure<PQLeafKeyType *> &leafKeys) {
        m_pertinentRoot->childCount(m_pertinentRoot->childCount() + 1 -
                                    fullChildren(m_pertinentRoot)->size());

        while (fullChildren(m_pertinentRoot)->size() > 1)
            removeChildFromSiblings(fullChildren(m_pertinentRoot)->popFrontRet());

        PQNodeType *currentNode = fullChildren(m_pertinentRoot)->popFrontRet();

        currentNode->parent(m_pertinentRoot);
        m_pertinentRoot = currentNode;
        ReplaceFullRoot(leafKeys);
    }

    void cleanUpFull() {
        for (PQNodeType *nodePtr : *m_pertinentNodes) {
            if (nodePtr->status() == PQNodeRoot::PQNodeStatus::Full)
                destroyNode(nodePtr);
        }
        if (m_pertinentRoot)
            m_pertinentRoot->status(PQNodeRoot::PQNodeStatus::Full);

        PQTree<edge, void *, void *>::emptyAllPertinentNodes();
    }
};