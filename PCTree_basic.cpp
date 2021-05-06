#include "PCTree.h"
#include "PCNode.h"
#include "PCArc.h"
#include "PCTreeIterators.h"

#include <variant>

using namespace pc_tree::hsu;

void PCTree::getTree(Graph &tree, GraphAttributes *g_a, NodeArray<PCNode *> &representation) {
    tree.clear();
    if (leaves.empty()) {
        return;
    }

    std::stack<std::pair<PCArc *, node>> stack; // Stores the next arc and the previous node for that arc.

    node firstLeaf = tree.newNode();
    bool nodeGraphics = false, nodeLabel = false;
    if (g_a != nullptr) {
        nodeGraphics = g_a->has(GraphAttributes::nodeGraphics);
        nodeLabel = g_a->has(GraphAttributes::nodeLabel);
    }
    if (nodeGraphics)
        g_a->shape(firstLeaf) = Shape::Triangle;
    if (nodeLabel)
        g_a->label(firstLeaf) = std::to_string(leaves.front()->id);
    representation[firstLeaf] = leaves.front();

    stack.emplace(leaves.front()->parentArc, firstLeaf);

    while (!stack.empty()) {
        auto pair = stack.top();
        stack.pop();
        node previousNode = pair.second;
        PCArc *startArc = pair.first;

        node node = tree.newNode();

        PCNode *pcNode = startArc->getyNode(timestamp);
        if (pcNode == nullptr || pcNode->nodeType == PCNodeType::CNode) {
            if (nodeGraphics)
                g_a->shape(node) = Shape::Rhomb;
        } else if (pcNode->nodeType == PCNodeType::PNode) {
            if (nodeGraphics)
                g_a->shape(node) = Shape::Ellipse;
            if (nodeLabel)
                g_a->label(node) = std::to_string(pcNode->id);
            representation[node] = pcNode;
        } else if (pcNode->nodeType == PCNodeType::Leaf) {
            if (nodeGraphics)
                g_a->shape(node) = Shape::Triangle;
            if (nodeLabel)
                g_a->label(node) = std::to_string(pcNode->id);
            representation[node] = pcNode;
        }

        if (startArc->yParent) {
            tree.newEdge(previousNode, node);
        } else {
            tree.newEdge(node, previousNode);
        }

        PCArc *current = startArc->neighbor1;
        PCArc *previous = startArc;
        while (current != startArc) {
            stack.emplace(current->twin, node);
            getNextArc(previous, current);
        }
    }

}

void PCTree::getNextArc(PCArc *&previous, PCArc *&current) {
    if (current->neighbor1 == previous) {
        previous = current;
        current = current->neighbor2;
    } else {
        previous = current;
        current = current->neighbor1;
    }
}

std::ostream &operator<<(std::ostream &os, const PCTree &obj) {
    return obj.printNodes(os);
}

std::ostream &PCTree::printNodes(std::ostream &os) const {
    std::stack<std::variant<PCArc *, std::string>> stack;
    stack.push(leaves.front()->parentArc);

    bool ignoreFirstNeighbor = false;
    while (!stack.empty()) {
        auto next = stack.top();
        stack.pop();

        // Next stack element is either a string we need to append or an arc pointing to a
        // subtree we still need to process.
        if (std::holds_alternative<std::string>(next)) {
            os << std::get<std::string>(next);
            continue;
        }

        PCArc *startArc = std::get<PCArc *>(next);

        string end;
        PCNode *node = startArc->getyNode(timestamp);
        if (node == nullptr) {
            os << "-1:[";
            end = "]";
        } else if (node->nodeType == PCNodeType::CNode) {
            os << node->id << ":[";
            end = "]";
        } else if (node->nodeType == PCNodeType::PNode) {
            os << node->id << ":(";
            end = ")";
        } else {
            os << node->id;
            end = "";
        }

        bool space = false;
        PCArc *previous = startArc;
        PCArc *current = startArc->neighbor1;

        stack.push(end);
        while (current != startArc) {
            if (space) {
                stack.push(", ");
            }
            stack.push(current->twin);
            space = true;
            getNextArc(previous, current);
        }

        if (!ignoreFirstNeighbor) {
            stack.push(", ");
            stack.push(current->twin);
            ignoreFirstNeighbor = true;
        }

    }
    return os;
}

std::ostream &PCTree::uniqueID(std::ostream &os,
                               const std::function<void(std::ostream &os, PCNode *, int)> &printNode,
                               const std::function<bool(PCNode *, PCNode *)> &compareNodes) {
    if (leaves.size() < 3) {
        return os << "too small";
    }
    std::vector<PCNode *> sortedLeaves(leaves.begin(), leaves.end());
    std::sort(sortedLeaves.begin(), sortedLeaves.end(), compareNodes);

    PCTreeArcArray<int> order(*this, -1);
    int i = 0;
    for (PCNode *leaf:sortedLeaves)
        order[leaf->getParentArc()] = i++;

    PCNode *lastLeaf = sortedLeaves.back();
    sortedLeaves.resize(leaves.size() - 1);
    std::vector<PCArc *> fullNodeOrder;
    timestamp++;
    assignLabels(sortedLeaves, &fullNodeOrder);
    for (PCArc *arc:fullNodeOrder) {
        order[arc] = i++;
    }

    std::stack<std::variant<PCArc *, std::string>> stack;
    order[lastLeaf->parentArc->twin] = i++;
    stack.push(lastLeaf->parentArc->twin);
    int foundLeaves = 0;
    while (!stack.empty()) {
        auto next = stack.top();
        stack.pop();

        if (std::holds_alternative<std::string>(next)) {
            os << std::get<std::string>(next);
            continue;
        }

        PCArc *startArc = std::get<PCArc *>(next)->twin;
        PCNode *node = startArc->getyNode(timestamp);

        if (node != nullptr && node->nodeType == PCNodeType::Leaf) {
            foundLeaves++;
            printNode(os, node, order[startArc->twin]);
            continue;
        }

        std::list<PCArc *> children;
        int childCount = 0;
        PCArc *previous = startArc->neighbor1;
        PCArc *current = startArc;
        do {
            if (order[current] != -1) {
                children.push_back(current);
            }
            childCount++;
            getNextArc(previous, current);
        } while (current != startArc);

        if (node == nullptr || node->nodeType == PCNodeType::CNode) {
            printNode(os, node, order[startArc->twin]);
            os << "[";
            stack.push("]");
            if (startArc == lastLeaf->parentArc) {
                PCArc *minChild = *std::min_element(children.begin(), children.end(), [&order](PCArc *elem, PCArc *min) {
                    return order[elem] < order[min];
                });
                while (children.front() != minChild) {
                    children.push_back(children.front());
                    children.pop_front();
                }
                PCArc *second = *(++children.begin());
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
            OGDF_ASSERT(node->nodeType == PCNodeType::PNode);
            OGDF_ASSERT(childCount == node->degree);
            printNode(os, node, order[startArc->twin]);
            if (node->degree <= 3) {
                os << "[";
                stack.push("]");
            } else {
                os << "(";
                stack.push(")");
            }
            children.sort([&order](PCArc *a, PCArc *b) {
                return order[a] < order[b];
            });
        }
        OGDF_ASSERT(order[children.front()] < order[children.back()]);

        bool space = false;
        for (PCArc *arc:children) {
            if (space)
                stack.push(", ");
            OGDF_ASSERT(arc != nullptr);
            stack.push(arc);
            space = true;
        }
    }
    OGDF_ASSERT(foundLeaves == leaves.size());

    return os;
}

bool PCTree::isTrivial() const {
    if (leaves.empty()) {
        return true;
    }

    PCNode *parentNode = leaves.front()->parentArc->getyNode(timestamp);
    return parentNode != nullptr && parentNode->nodeType == PCNodeType::PNode && parentNode->degree == leaves.size();
}

PCNode *PCTree::getParent(const PCNode *node) const {
    return node->parentArc ? node->parentArc->getyNode(timestamp) : nullptr;
};

void PCTree::removeLeaf(PCNode *leaf) {
    OGDF_ASSERT(leaf && leaf->tree == this);
    OGDF_ASSERT(leaf->nodeType == PCNodeType::Leaf);
    timestamp++;

    PCArc *parentArc = leaf->parentArc;
    parentArc->extract();
    removeLeafFromList(leaf);
    PCNode *parent = parentArc->getyNode(timestamp);

    if (parent != nullptr && parent->nodeType == PCNodeType::PNode) {
        parent->degree--;
    }

    // Delete parent node if it has degree two, unless it's the only remaining inner node.
    if (parentArc->neighbor2->getNextArc(parentArc->neighbor1) == parentArc->neighbor1 &&
        (leaves.size() != 2 || leaves.front()->parentArc->neighbor1 != leaves.back()->parentArc)) {
        deleteDegreeTwoNode(parentArc->neighbor1);
    }

    delete parentArc->twin;
    delete parentArc;

}

PCNode *PCTree::mergeLeaves(std::vector<PCNode *> &consecutiveLeaves, bool assumeConsecutive) {
    OGDF_ASSERT(!consecutiveLeaves.empty());
    if (!assumeConsecutive && !makeConsecutive(consecutiveLeaves)) {
        return nullptr;
    }

    // Remove all consecutive leaves except the first one.
    for (auto it = ++consecutiveLeaves.begin(); it != consecutiveLeaves.end(); ++it) {
        removeLeaf(*it);
    }

    // Return the remaining leaf.
    return consecutiveLeaves.front();
}

void PCTree::setIndex(PCNode *node) {
    setIndex(node, nextNodeId);
}

void PCTree::setIndex(PCNode *node, int index) {
    int oldSize = nodeArrayRegistry.keyArrayTableSize();
    node->id = index;
    nextNodeId = std::max(nextNodeId, index + 1);
    if (oldSize != nodeArrayRegistry.keyArrayTableSize()) nodeArrayRegistry.enlargeArrayTables();
}

void PCTree::setIndex(PCArc *arc) {
    int oldSize = arcArrayRegistry.keyArrayTableSize();
    arc->id = nextArcId;
    ++nextArcId;
    if (oldSize != arcArrayRegistry.keyArrayTableSize()) arcArrayRegistry.enlargeArrayTables();
}

template<class Key>
bool PCTreeRegistry<Key>::isKeyAssociated(Key key) const {
#ifdef OGDF_DEBUG
    return key && key->treeOf() == m_pTree;
#else
    return key;
#endif
}

template<class Key>
int PCTreeRegistry<Key>::keyToIndex(Key key) const {
    return key->index();
}

template
class pc_tree::hsu::PCTreeRegistry<PCNode *>;

template
class pc_tree::hsu::PCTreeRegistry<PCArc *>;

void PCTree::replaceLeaf(int leafCount, PCNode *leaf, std::vector<PCNode *> *added) {
    OGDF_ASSERT(leaf && leaf->tree == this);
    OGDF_ASSERT(leaf->nodeType == PCNodeType::Leaf);
    OGDF_ASSERT(leafCount > 1);

    bool removeRoot = leaves.size() == 2;
    removeLeafFromList(leaf);
    leaf->nodeType = PCNodeType::PNode;

    insertLeaves(leafCount, leaf, added);

    if (removeRoot) {
        deleteDegreeTwoNode(leaf->parentArc);
    }
}

void PCTree::deleteNodePointer(PCNode *pointer) {
    if (pointer != nullptr) {
        pointer->refCount--;
        if (pointer->refCount == 0) {
            delete pointer;
        }
    }
}

PCNode *PCTree::assignNodePointer(PCNode *newPointer, PCNode *oldPointer) {
    deleteNodePointer(oldPointer);

    if (newPointer != nullptr) {
        newPointer->refCount++;
    }
    return newPointer;
}

bool PCTree::isValidOrder(std::vector<PCNode *> &order) {
    OGDF_ASSERT(order.size() == leaves.size());
    PCTreeNodeArray<PCNode *> leafMapping(*this);
    PCTree copy(*this, &leafMapping);
    PCNode *previous = nullptr;
    for (PCNode *node : order) {
        OGDF_ASSERT(node->tree == this);
        if (previous == nullptr) {
            previous = node;
            continue;
        }
        std::vector<PCNode *> pair;
        pair.push_back(leafMapping[previous]);
        pair.push_back(leafMapping[node]);
        if (!copy.makeConsecutive(pair)) {
            return false;
        }
        previous = node;
    }

    return true;
}

void PCTree::setNeighborNull(PCArc *arc, PCArc *nullNeighbor) {
    if (arc == nullptr) {
        return;
    }

    // Replace a neigbhor of arc with a nullptr to simplify combining different lists.
    if (arc->neighbor1 == nullNeighbor) {
        arc->neighbor1 = nullptr;
    } else if (arc->neighbor2 == nullNeighbor) {
        arc->neighbor2 = nullptr;
    }
}

void PCTree::combineLists(PCArc *listBegin, PCArc *listEnd) {
    if (listBegin == nullptr || listEnd == nullptr) {
        return;
    }

    if (listEnd->neighbor1 == nullptr && (listEnd->neighbor2 != nullptr || listEnd->preferNeighbor1)) {
        listEnd->neighbor1 = listBegin;
    } else if (listEnd->neighbor2 == nullptr && (listEnd->neighbor1 != nullptr || !listEnd->preferNeighbor1)) {
        listEnd->neighbor2 = listBegin;
    }

    if (listBegin->neighbor1 == nullptr && (listBegin->neighbor2 != nullptr || listBegin->preferNeighbor1)) {
        listBegin->neighbor1 = listEnd;
    } else if (listBegin->neighbor2 == nullptr && (listBegin->neighbor1 != nullptr || !listBegin->preferNeighbor1)) {
        listBegin->neighbor2 = listEnd;
    }

}

PCNode *PCTree::createNode(PCTree::PCNodeType type) {
    PCNode *n = new PCNode(type);
    setIndex(n);
#ifdef OGDF_DEBUG
    n->tree = this;
#endif

    return n;
}

PCNode *PCTree::createNode(PCNode &other) {
    PCNode *n = new PCNode(other);
    setIndex(n);
#ifdef OGDF_DEBUG
    n->tree = this;
#endif

    return n;
}

PCArc *PCTree::createArc(PCArc &other) {
    PCArc *a = new PCArc(other);
    setIndex(a);

#ifdef OGDF_DEBUG
    a->tree = this;
#endif

    return a;
}

PCArc *PCTree::createArc() {
    PCArc *a = new PCArc();
    setIndex(a);

#ifdef OGDF_DEBUG
    a->tree = this;
#endif

    return a;
}

PCArc *PCTree::findRootEntryArc() const {
    if (timestamp == 0) {
        return tempRootNodeEntryArc;
    }

    OGDF_ASSERT(!leaves.empty());
    std::function<bool(PCArc *)> visit = [](PCArc *a) {
        return a->isYParent();
    };

    FilteringPCTreeDFS dfs(*this, {leaves.front()->parentArc}, visit);
    PCArc *latestParentArc = leaves.front()->parentArc;
    for (PCArc *a : dfs) {
        latestParentArc = a;
    }

    return latestParentArc;
}

bool PCTree::isValid() const {
    if (leaves.size() <= 1) {
        return false;
    } else if (leaves.size() == 2 && leaves.front()->parentArc->neighbor1 == leaves.back()->parentArc &&
               leaves.front()->parentArc->neighbor2 == leaves.back()->parentArc) {
        // special case: root node with two leaves
        return true;
    }

    FilteringPCTreeDFS dfs(*this, {leaves.front()->parentArc});
    for (PCArc *arc : dfs) {
        if ((arc->getYNodeType() == PCNodeType::Leaf && !arc->isAdjacent(arc)) ||
            (arc->getYNodeType() != PCNodeType::Leaf && (arc->neighbor1 == arc->neighbor2))) {
            return false;
        }
    }

    return true;
}

void PCTree::getRestrictions(std::vector<std::vector<PCNode *>> &restrictions, PCNode *startLeaf) {
    OGDF_HEAVY_ASSERT(isValid());
    if (startLeaf == nullptr) {
        startLeaf = leaves.front();
    }

    restrictions.clear();
    timestamp++;
    std::vector<PCArc *> informedArcOrder;
    std::vector<PCNode *> fullLeaves;
    std::copy_if(leaves.begin(), leaves.end(), std::back_inserter(fullLeaves),
                 [&](const PCNode *leaf) { return leaf != startLeaf; });
    PCTreeArcArray<std::vector<PCNode *>> innerNodeRestrictions(*this);

    assignLabels(fullLeaves, &informedArcOrder);

    PCArc *last = startLeaf->parentArc;
    if (last->getYNodeType() == PCNodeType::CNode) {
        informedArcOrder.push_back(last->twin);
    }

    for (PCArc *currNodeArc : informedArcOrder) {
        PCNodeType nodeType = currNodeArc->twin->getYNodeType();
        std::vector<PCNode *> currentNodeRestriction;
        int previousRestrictionSize = 0;

        for (PCArc *a : *currNodeArc->twin) {
            if (a == currNodeArc->twin) {
                continue;
            }

            int currentRestrictionSize;
            if (a->twin->getYNodeType() == PCNodeType::Leaf) {
                currentNodeRestriction.push_back(a->twin->getYNode());
                currentRestrictionSize = 1;
            } else {
                currentRestrictionSize = innerNodeRestrictions[a].size();
                OGDF_ASSERT(currentRestrictionSize >= 2);
                currentNodeRestriction.insert(currentNodeRestriction.end(), innerNodeRestrictions[a].begin(),
                                              innerNodeRestrictions[a].end());
            }

            if (nodeType == PCNodeType::CNode && previousRestrictionSize > 0) {
                restrictions.emplace_back(
                        currentNodeRestriction.end() - (previousRestrictionSize + currentRestrictionSize),
                        currentNodeRestriction.end());
                OGDF_ASSERT(restrictions.back().size() >= 2);
            }

            previousRestrictionSize = currentRestrictionSize;
        }
        if (nodeType == PCNodeType::PNode) {
            restrictions.push_back(currentNodeRestriction);
        }
        innerNodeRestrictions[currNodeArc] = std::move(currentNodeRestriction);

    }
}

Bigint PCTree::possibleOrders() {
    Bigint orders(1);
    FilteringPCTreeDFS dfs(*this, {getLeaves().front()->getParentArc()}, [](PCArc *a) {
        return a->getYNodeType() != PCTree::PCNodeType::Leaf;
    });

    for (PCArc *a : dfs) {
        OGDF_ASSERT(a->getYNodeType() != PCTree::PCNodeType::Leaf);
        if (a->getYNodeType() == PCTree::PCNodeType::CNode) {
            orders *= 2;
        } else {
            orders *= factorial(a->getYNode()->getDegree() - 1);
        }
    }

    return orders;
}

int PCTree::getInnerNodeCount() {
    FilteringPCTreeDFS dfs(*this, {getLeaves().front()->getParentArc()}, [](PCArc *a) {
        return a->getYNodeType() != PCTree::PCNodeType::Leaf;
    });

    int count = 0;

    for (PCArc *a : dfs) {
        OGDF_ASSERT(a->getYNodeType() != PCTree::PCNodeType::Leaf);
        count++;
    }

    return count;
}

void PCTree::getNodeStats(int &p_nodes, int &c_nodes, int &sum_p_deg, int &sum_c_deg) const {
    FilteringPCTreeDFS dfs(*this, {getLeaves().front()->getParentArc()}, [](PCArc *a) {
        return a->getYNodeType() != PCTree::PCNodeType::Leaf;
    });

    p_nodes = c_nodes = sum_p_deg = sum_c_deg = 0;
    for (PCArc *a : dfs) {
        if (a->getYNodeType() == PCTree::PCNodeType::PNode) {
            p_nodes++;
            sum_p_deg += (int) a->getYNode()->degree;
        } else {
            OGDF_ASSERT(a->getYNodeType() == PCTree::PCNodeType::CNode);
            c_nodes++;
            for (PCArc *b : *a) {
                sum_c_deg++;
            }
        }
    }
}

std::vector<PCNode *> PCTree::currentLeafOrder() const {
    std::vector<PCNode *> order;
    for (PCArc *a:FilteringPCTreeDFS(*this, {getLeaves().front()->getParentArc()}))
        if (a->getYNodeType() == PCTree::PCNodeType::Leaf)
            order.push_back(a->getYNode());
    return order;
}

ArcIterator PCArc::begin() {
    return ArcIterator(this, this, neighbor2, neighbor1, true, false);
}

ArcIterator PCArc::end() {
    return ArcIterator(this, this, neighbor2, neighbor1, false, true);
}