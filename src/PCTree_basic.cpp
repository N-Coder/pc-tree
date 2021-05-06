#include "PCTree.h"
#include "PCNode.h"

#include <variant>
#include <stack>
#include <queue>

using namespace pc_tree;
using namespace ogdf;
using namespace Dodecahedron;


bool PCTree::isTrivial() const {
    if (leaves.empty()) {
        OGDF_ASSERT(rootNode == nullptr);
        return true;
    }
    return rootNode->getNodeType() == PCNodeType::PNode && rootNode->childCount == leaves.size();
}

void PCTree::getTree(Graph &tree, GraphAttributes *g_a, PCTreeNodeArray<ogdf::node> &pc_repr,
                     ogdf::NodeArray<PCNode *> *g_repr, std::vector<PCNode *> full_leaves) {
    std::vector<PCNode *> fullNodeOrder;
    timestamp++;
    assignLabels(full_leaves, &fullNodeOrder);
    getTree(tree, g_a, pc_repr, g_repr, true);
}

void PCTree::getTree(Graph &tree, GraphAttributes *g_a, PCTreeNodeArray<ogdf::node> &pc_repr, ogdf::NodeArray<PCNode *> *g_repr, bool mark_full) const {
    tree.clear();
    if (leaves.empty()) {
        return;
    }

    bool nodeGraphics = false, nodeLabel = false;
    if (g_a != nullptr) {
        nodeGraphics = g_a->has(GraphAttributes::nodeGraphics);
        nodeLabel = g_a->has(GraphAttributes::nodeLabel);
    }

    for (PCNode *pc_node : allNodes()) {
        ogdf::node g_node = pc_repr[pc_node] = tree.newNode();
        if (g_repr != nullptr) {
            (*g_repr)[g_node] = pc_node;
        }

        if (nodeGraphics) {
            if (pc_node->nodeType == PCNodeType::CNode) {
                g_a->shape(g_node) = Shape::Rhomb;
            } else if (pc_node->nodeType == PCNodeType::PNode) {
                g_a->shape(g_node) = Shape::Ellipse;
            } else {
                OGDF_ASSERT(pc_node->isLeaf());
                g_a->shape(g_node) = Shape::Triangle;
            }
            if (mark_full) {
                NodeLabel label = pc_node->tempInfo().label;
                if (label == NodeLabel::Full) {
                    g_a->fillColor(g_node) = (Color(Color::Name::Darkblue));
                } else if (label == NodeLabel::Partial) {
                    g_a->fillColor(g_node) = (Color(Color::Name::Lightblue));
                }
            }
        }
        if (nodeLabel) {
            g_a->label(g_node) = std::to_string(pc_node->id);
        }
        PCNode *parent = pc_node->getParent();
        if (parent != nullptr) {
            tree.newEdge(g_node, pc_repr[parent]);
        }
    }
}

std::ostream &operator<<(std::ostream &os, const PCTree &tree) {
    return os << &tree;
}

std::ostream &operator<<(std::ostream &os, const PCTree *tree) {
    std::stack<std::variant<PCNode *, std::string>> stack;
    stack.push(tree->rootNode);
    if (tree->rootNode == nullptr) {
        return os << "empty";
    }

    while (!stack.empty()) {
        auto next = stack.top();
        stack.pop();

        // Next stack element is either a string we need to append or an arc pointing to a
        // subtree we still need to process.
        if (std::holds_alternative<std::string>(next)) {
            os << std::get<std::string>(next);
            continue;
        }

        PCNode *base = std::get<PCNode *>(next);
        if (base->nodeType == PCNodeType::CNode) {
            os << base->id << ":[";
            stack.push("]");
        } else if (base->nodeType == PCNodeType::PNode) {
            os << base->id << ":(";
            stack.push(")");
        } else {
            OGDF_ASSERT(base->nodeType == PCNodeType::Leaf);
            os << base->id;
            continue;
        }

        bool space = false;
        for (PCNode *node:base->children()) {
            if (space)
                stack.push(", ");
            OGDF_ASSERT(node != nullptr);
            stack.push(node);
            space = true;
        }
    }

    return os;
}

void pc_tree::uid_utils::nodeToID(std::ostream &os, PCNode *n, int pos) {
    os << n->index();
    if (!n->isLeaf()) os << ":";
}

void pc_tree::uid_utils::nodeToPosition(std::ostream &os, PCNode *n, int pos) {
    os << pos;
    if (!n->isLeaf()) os << ":";
}

void pc_tree::uid_utils::leafToID(std::ostream &os, PCNode *n, int pos) {
    if (n->isLeaf()) os << n->index();
}

void pc_tree::uid_utils::leafToPosition(std::ostream &os, PCNode *n, int pos) {
    if (n->isLeaf()) os << pos;
}

bool pc_tree::uid_utils::compareNodesByID(PCNode *a, PCNode *b) {
    return a->index() < b->index();
}

std::ostream &PCTree::uniqueID(std::ostream &os,
                               const std::function<void(std::ostream &os, PCNode *, int)> &printNode,
                               const std::function<bool(PCNode *, PCNode *)> &compareNodes) {
    if (rootNode == nullptr) {
        return os << "empty";
    }
    std::vector<PCNode *> sortedLeaves(leaves.begin(), leaves.end());
    std::sort(sortedLeaves.begin(), sortedLeaves.end(), compareNodes);

    PCTreeNodeArray<int> order(*this, -1);
    int i = 0;
    for (PCNode *leaf:sortedLeaves) order[leaf] = i++;

    PCNode *lastLeaf = sortedLeaves.back();
    sortedLeaves.resize(leaves.size() - 1);
    std::vector<PCNode *> fullNodeOrder;
    timestamp++;
    assignLabels(sortedLeaves, &fullNodeOrder);
    for (PCNode *node:fullNodeOrder) order[node] = i++;

    std::stack<std::variant<PCNode *, std::string>> stack;
    stack.push(fullNodeOrder.back());
    while (!stack.empty()) {
        auto next = stack.top();
        stack.pop();

        if (std::holds_alternative<std::string>(next)) {
            os << std::get<std::string>(next);
            continue;
        }

        PCNode *node = std::get<PCNode *>(next);
        std::list<PCNode *> children;
        if (node->nodeType == PCNodeType::CNode) {
            printNode(os, node, order[node]);
            os << "[";
            stack.push("]");
            if (node == fullNodeOrder.back()) {
                children.assign(node->neighbors().begin(), node->neighbors().end());
                PCNode *minChild = *std::min_element(children.begin(), children.end(), [&order](PCNode *elem, PCNode *min) {
                    return order[elem] < order[min];
                });
                while (children.front() != minChild) {
                    children.push_back(children.front());
                    children.pop_front();
                }
                PCNode *second = *(++children.begin());
                if (order[second] > order[children.back()]) {
                    children.push_back(children.front());
                    children.pop_front();
                    children.reverse();
                }
                second = *(++children.begin());
                OGDF_ASSERT(children.front() == minChild);
                OGDF_ASSERT(order[second] < order[children.back()]);
            } else {
                PCNode *informedNeighbor = nullptr;
                for (PCNode *neigh:node->neighbors()) {
                    if (order[neigh] > order[node]) {
                        OGDF_ASSERT(informedNeighbor == nullptr);
                        informedNeighbor = neigh;
                    }
                }
                OGDF_ASSERT(informedNeighbor != nullptr);
                PCNode *neigh1 = node->getNextNeighbor(nullptr, informedNeighbor);
                PCNode *neigh2 = node->getNextNeighbor(neigh1, informedNeighbor);
                if (order[neigh2] < order[neigh1]) std::swap(neigh1, neigh2);
                for (PCNode *pred = informedNeighbor, *curr = neigh1; curr != informedNeighbor; node->proceedToNextNeighbor(pred, curr))
                    children.push_back(curr);
            }
        } else if (node->nodeType == PCNodeType::PNode) {
            printNode(os, node, order[node]);
            if (node->getDegree() <= 3) {
                os << "[";
                stack.push("]");
            } else {
                os << "(";
                stack.push(")");
            }
            std::vector<PCNode *> &fullNeighbors = node->tempInfo().fullNeighbors;
            children.assign(fullNeighbors.begin(), fullNeighbors.end());
            if (node == fullNodeOrder.back())
                children.push_back(lastLeaf);
            children.sort([&order](PCNode *a, PCNode *b) {
                return order[a] < order[b];
            });
        } else {
            OGDF_ASSERT(node->nodeType == PCNodeType::Leaf);
            printNode(os, node, order[node]);
            continue;
        }
        if (node == fullNodeOrder.back()) {
            OGDF_ASSERT(node->isParentOf(lastLeaf));
            OGDF_ASSERT(children.size() == node->tempInfo().fullNeighbors.size() + 1);
        } else {
            OGDF_ASSERT(children.size() == node->tempInfo().fullNeighbors.size());
        }
        OGDF_ASSERT(order[children.front()] < order[children.back()]);

        bool space = false;
        for (PCNode *child : children) {
            if (space)
                stack.push(", ");
            OGDF_ASSERT(child != nullptr);
            stack.push(child);
            space = true;
        }
    }

    return os;
}

std::ostream &operator<<(ostream &os, const pc_tree::PCNodeType t) {
    switch (t) {
        case pc_tree::PCNodeType::Leaf:
            return os << "Leaf";
        case pc_tree::PCNodeType::PNode:
            return os << "PNode";
        case pc_tree::PCNodeType::CNode:
            return os << "CNode";
        default:
            OGDF_ASSERT(false);
            return os << "PCNodeType???";
    }
}

std::ostream &operator<<(ostream &os, const pc_tree::NodeLabel l) {
    switch (l) {
        case pc_tree::NodeLabel::Unknown:
            return os << "Empty/Unknown";
        case pc_tree::NodeLabel::Partial:
            return os << "Partial";
        case pc_tree::NodeLabel::Full:
            return os << "Full";
        default:
            OGDF_ASSERT(false);
            return os << "NodeLabel???";
    }
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
class pc_tree::PCTreeRegistry<PCNode *>;


bool PCTree::isValidOrder(std::vector<PCNode *> &order) const {
    OGDF_ASSERT(order.size() == leaves.size());
    PCTreeNodeArray<PCNode *> leafMapping(*this);
    PCTree copy(*this, leafMapping);
    PCNode *previous = nullptr;
    for (PCNode *node : order) {
        OGDF_ASSERT(node->tree == this);
        if (previous == nullptr) {
            previous = node;
            continue;
        }
        if (!copy.makeConsecutive({leafMapping[previous], leafMapping[node]})) {
            return false;
        }
        previous = node;
    }
#ifdef OGDF_DEBUG
    OGDF_ASSERT(copy.possibleOrders() == 2);
    OGDF_ASSERT(copy.makeConsecutive({order.front(), order.back()}));
    std::list<PCNode *> res_order;
    copy.currentLeafOrder(res_order);
    OGDF_ASSERT(res_order.size() == order.size());
    for (int i = 0; res_order.front() != order.front(); i++) {
        res_order.push_back(res_order.front());
        res_order.pop_front();
        OGDF_ASSERT(i < order.size());
    }
    OGDF_ASSERT(res_order.size() == order.size());
    OGDF_ASSERT(std::equal(res_order.begin(), res_order.end(), order.begin(), order.end()));
#endif

    return true;
}

bool PCTree::checkValid() const {
    OGDF_ASSERT (leaves.size() > 2);
    OGDF_ASSERT(rootNode != nullptr);
    std::queue<PCNode *> todo;
    for (PCNode *leaf : leaves) {
        OGDF_ASSERT(leaf->isLeaf());
        todo.push(leaf);
    }
    bool leaves_done = false, root_found = false;
    int leaves_found = 0, p_nodes_found = 0, c_nodes_found = 0;
    std::vector<PCNode *> id_seen(nextNodeId, nullptr);
    while (!todo.empty()) {
        PCNode *node = todo.front();
        todo.pop();

        if (id_seen.at(node->id) == node) continue;
        OGDF_ASSERT(node->tree == this);
        OGDF_ASSERT(id_seen[node->id] == nullptr);
        id_seen[node->id] = node;
        PCNode *parent = node->getParent();
        OGDF_ASSERT((node == rootNode) == (parent == nullptr));
        if (node == rootNode) {
            OGDF_ASSERT(node->childCount >= 3);
            root_found = true;
        } else {
            todo.push(parent);
        }

        OGDF_ASSERT(leaves_done == (node->nodeType != PCNodeType::Leaf));
        if (node->nodeType == PCNodeType::PNode) {
            OGDF_ASSERT(node->childCount >= 2);
            p_nodes_found++;
        } else if (node->nodeType == PCNodeType::CNode) {
            OGDF_ASSERT(node->childCount >= 2);
            c_nodes_found++;
        } else {
            OGDF_ASSERT(node->isLeaf());
            leaves_found++;
        }

        // check that my siblings know me or that my parent knows me as outer child
        if (node->sibling1 != nullptr) {
            OGDF_ASSERT(node->sibling1->isSiblingAdjacent(node));
            OGDF_ASSERT(node->sibling1->getParent() == parent);
        } else if (parent != nullptr) {
            OGDF_ASSERT(parent->isChildOuter(node));
        }
        if (node->sibling2 != nullptr) {
            OGDF_ASSERT(node->sibling2->isSiblingAdjacent(node));
            OGDF_ASSERT(node->sibling2->getParent() == parent);
        } else if (parent != nullptr) {
            OGDF_ASSERT(parent->isChildOuter(node));
        }

        // check that all my children know me and that my degree is right
        PCNode *pred = nullptr;
        PCNode *curr = node->child1;
        int children = 0;
        while (curr != nullptr) {
            OGDF_ASSERT(curr->getParent() == node);
            if (node->getNodeType() == PCNodeType::CNode) {
                OGDF_ASSERT(curr->parentPNode == nullptr);
                OGDF_ASSERT(curr->parentCNodeId == node->nodeListIndex);
            } else {
                OGDF_ASSERT(curr->parentPNode == node);
                OGDF_ASSERT(curr->parentCNodeId == -1);
            }
            children++;
            proceedToNextSibling(pred, curr);
        }
        OGDF_ASSERT(children == node->childCount);
        OGDF_ASSERT(pred == node->child2);

        if (node == leaves.back()) {
            leaves_done = true;
        }
    }
    OGDF_ASSERT(leaves_done);
    OGDF_ASSERT(root_found);
    OGDF_ASSERT(leaves_found == leaves.size());
    OGDF_ASSERT(p_nodes_found == pNodeCount);
    OGDF_ASSERT(c_nodes_found == cNodeCount);
    c_nodes_found = 0;
    for (int cid = 0; cid < cNodes.size(); cid++) {
        PCNode *node = cNodes.at(cid);
        OGDF_ASSERT(node == nullptr || parents.getRepresentative(cid) == cid);
        if (node == nullptr) continue;
        c_nodes_found++;
        OGDF_ASSERT(id_seen.at(node->id) == node);
    }
    OGDF_ASSERT(c_nodes_found == cNodeCount);

//    if (isTrivial()) {
//        OGDF_HEAVY_ASSERT(possibleOrders() == factorial(getLeafCount() - 1));
//    } else {
//        OGDF_HEAVY_ASSERT(possibleOrders() <= factorial(getLeafCount() - 1));
//    }

    return true;
}

void PCTree::getRestrictions(std::vector<std::vector<PCNode *>> &restrictions, PCNode *fixedLeaf) const {
    PCTreeNodeArray<int> readyChildren(*this, 0);
    PCTreeNodeArray<std::list<PCNode *>> subtreeLeaves(*this);
    std::queue<PCNode *> todo;
    for (PCNode *leaf : leaves) {
        if (leaf == fixedLeaf) continue;
        subtreeLeaves[leaf].push_back(leaf);
        PCNode *parent = leaf->getParent();
        if ((readyChildren[parent] += 1) == parent->getDegree() - 1)
            todo.push(parent);
    }
    PCNode *central = nullptr;
    while (!todo.empty()) {
        PCNode *node = todo.front();
        todo.pop();
        OGDF_ASSERT(node != fixedLeaf);

        PCNode *next = nullptr;
        PCNode *parent = node->getParent();
        if (parent != nullptr && subtreeLeaves[parent].empty()) {
            next = parent;
        }
#ifndef OGDF_DEBUG
        else
#endif
        {
            for (PCNode *neigh : node->neighbors()) {
                if (subtreeLeaves[neigh].empty()) {
                    OGDF_ASSERT(next == nullptr || (next == parent && neigh == parent));
                    next = neigh;
#ifndef OGDF_DEBUG
                    break;
#endif
                }
            }
        }
        if (next == nullptr) {
            OGDF_ASSERT(fixedLeaf == nullptr);
            OGDF_ASSERT(central == nullptr);
            OGDF_ASSERT(todo.empty());
            central = node;
        }

        PCNode *pred = nullptr;
        for (PCNode *curr : node->neighbors(next)) {
            if (curr == next) continue;
            OGDF_ASSERT(!subtreeLeaves[curr].empty());
            if (node->nodeType == PCNodeType::CNode && pred != nullptr) {
                unsigned long size = subtreeLeaves[pred].size() + subtreeLeaves[curr].size();
                if (!isTrivialRestriction(size)) {
                    std::vector<PCNode *> &back = restrictions.emplace_back();
                    back.reserve(size);
                    back.insert(back.end(), subtreeLeaves[pred].begin(), subtreeLeaves[pred].end());
                    back.insert(back.end(), subtreeLeaves[curr].begin(), subtreeLeaves[curr].end());
                }
            }
            if (pred != nullptr)
                subtreeLeaves[node].splice(subtreeLeaves[node].end(), subtreeLeaves[pred]);
            pred = curr;
        }
        if (pred != next)
            subtreeLeaves[node].splice(subtreeLeaves[node].end(), subtreeLeaves[pred]);

        if (node->nodeType == PCNodeType::PNode && !isTrivialRestriction(subtreeLeaves[node].size()))
            restrictions.emplace_back(subtreeLeaves[node].begin(), subtreeLeaves[node].end());

        if (next != nullptr && (readyChildren[next] += 1) == next->getDegree() - 1 && next != fixedLeaf)
            todo.push(next);
    }
    if (fixedLeaf != nullptr) {
        OGDF_ASSERT(central == nullptr);
        central = fixedLeaf->getParent();
        OGDF_ASSERT(readyChildren[central] == central->getDegree() - 1);
        OGDF_ASSERT(subtreeLeaves[central].size() == getLeafCount() - 1);
    } else {
        OGDF_ASSERT(central != nullptr);
        OGDF_ASSERT(readyChildren[central] == central->getDegree());
        OGDF_ASSERT(subtreeLeaves[central].size() == getLeafCount());
    }
}

Bigint PCTree::PCTree::possibleOrders() const {
    Bigint orders(1);
    for (PCNode *node : innerNodes()) {
        if (node->getNodeType() == PCNodeType::CNode) {
            orders *= 2;
        } else {
            int children = node->getChildCount();
            if (node == rootNode) {
                children -= 1; // don't count circular shifts
            }
            orders *= factorial(children);
        }
    }
    return orders;
}

PCNode *PCTree::setRoot(PCNode *newRoot) {
    OGDF_ASSERT(newRoot->isDetached());
    PCNode *oldRoot = rootNode;
    rootNode = newRoot;
    OGDF_HEAVY_ASSERT(checkValid());
    return oldRoot;
}

PCNode *PCTree::changeRoot(PCNode *newRoot) {
    // FIXME
    OGDF_HEAVY_ASSERT(checkValid());
    std::stack<PCNode *> path;
    for (PCNode *node = newRoot; node != nullptr; node = newRoot->getParent()) {
        OGDF_ASSERT(node != nullptr);
        OGDF_ASSERT(node->tree == this);
        path.push(node);
    }
    while (path.size() > 1) {
        PCNode *old_parent = path.top();
        path.pop();
        PCNode *new_parent = path.top();

        old_parent->child1->replaceSibling(nullptr, old_parent->child2);
        old_parent->child2->replaceSibling(nullptr, old_parent->child1);
        new_parent->sibling1->replaceSibling(new_parent, nullptr);
        new_parent->sibling2->replaceSibling(new_parent, nullptr);
        old_parent->child1 = new_parent->sibling1;
        old_parent->child2 = new_parent->sibling2;
        old_parent->setParent(new_parent);
    }
    OGDF_ASSERT(path.size() == 1);
    OGDF_ASSERT(path.top() == newRoot);


    return setRoot(newRoot);
}

