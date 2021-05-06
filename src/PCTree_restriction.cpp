#include "PCTree.h"
#include "PCNode.h"

#include <queue>
#include <iterator>

#ifdef LIKWID_PERFMON

#include <likwid.h>

#define PC_PROFILE_ENTER(level, msg) PC_PROFILE_ENTER_ ## level (msg)
#define PC_PROFILE_EXIT(level, msg) PC_PROFILE_EXIT_ ## level (msg)

#ifdef PC_PROFILE_LEVEL_1
#define PC_PROFILE_ENTER_1(msg) likwid_markerStartRegion(msg)
#define PC_PROFILE_EXIT_1(msg) likwid_markerStopRegion(msg)
#else
#define PC_PROFILE_ENTER_1(msg)
#define PC_PROFILE_EXIT_1(msg)
#endif

#ifdef PC_PROFILE_LEVEL_2
#define PC_PROFILE_ENTER_2(msg) likwid_markerStartRegion(msg)
#define PC_PROFILE_EXIT_2(msg) likwid_markerStopRegion(msg)
#else
#define PC_PROFILE_ENTER_2(msg)
#define PC_PROFILE_EXIT_2(msg)
#endif

#else
#define PC_PROFILE_ENTER(level, msg)
#define PC_PROFILE_EXIT(level, msg)
#endif

using namespace pc_tree;

#ifdef UFPC_DEBUG
#define log std::cout

#include <ogdf/misclayout/CircularLayout.h>
#include <ogdf/tree/TreeLayout.h>
#include <ogdf/fileformats/GraphIO.h>

using namespace ogdf;

void dump(PCTree &T, const std::string &name) {
    Graph G;
    GraphAttributes GA(G, GraphAttributes::all);
    PCTreeNodeArray<ogdf::node> pc_repr(T, nullptr);
    T.getTree(G, &GA, pc_repr, nullptr, true);
    CircularLayout cl;
    cl.call(GA);
    GraphIO::write(GA, name + "-cl.svg");
    TreeLayout tl;
    G.reverseAllEdges();
    tl.call(GA);
    GraphIO::write(GA, name + "-tl.svg");
}

#else
#define log if(false) std::cout

void dump(PCTree &T, const std::string &name) {}

#endif


bool pc_tree::isTrivialRestriction(int restSize, int leafCount) {
    return restSize <= 1 || restSize >= leafCount - 1;
}

bool PCTree::isTrivialRestriction(int size) const {
    return pc_tree::isTrivialRestriction(size, getLeafCount());
}

bool PCTree::isTrivialRestriction(std::vector<PCNode *> &consecutiveLeaves) const {
    return pc_tree::isTrivialRestriction(consecutiveLeaves.size(), getLeafCount());
}

void PCTree::LoggingObserver::makeConsecutiveCalled(PCTree &tree, const std::vector<PCNode *> &consecutiveLeaves) {
    log << "Tree " << tree << " with consecutive leaves [";
    for (PCNode *leaf : consecutiveLeaves) log << leaf->index() << ", ";
    log << "]" << std::endl;
    dump(tree, "before");
}

void PCTree::LoggingObserver::labelsAssigned(PCTree &tree, PCNode *firstPartial, PCNode *lastPartial, int partialCount) {
    log << "Found " << partialCount << " partial nodes: [";
    int found = 0;
    for (PCNode *node = firstPartial; node != nullptr; node = node->tempInfo().nextPartial) {
        log << node << ", ";
        found++;
    }
    log << "]" << std::endl;
    OGDF_ASSERT(partialCount == found);
    dump(tree, "labelled");
}

void PCTree::LoggingObserver::terminalPathFound(PCTree &tree, PCNode *apex, PCNode *apexTPPred2, int terminalPathLength) {
    log << "Apex is " << apex->constTempInfo().label << " " << apex << ", TP length is " << terminalPathLength << ", first path is [";
    int length = 1;
    for (PCNode *cur = apex->constTempInfo().tpPred; cur != nullptr; cur = cur->constTempInfo().tpPred) {
        log << cur << ", ";
        length++;
    }
    log << "], second path is [";
    for (PCNode *cur = apexTPPred2; cur != nullptr; cur = cur->constTempInfo().tpPred) {
        log << cur << ", ";
        length++;
    }
    log << "] (effective length " << length << ")" << std::endl;
    OGDF_ASSERT(terminalPathLength == length);
}

void PCTree::LoggingObserver::centralCreated(PCTree &tree, PCNode *central) {
    log << "Tree before merge into central " << central->index() << ": " << tree << std::endl;
    dump(tree, "before-merge");
}

void PCTree::LoggingObserver::beforeMerge(PCTree &tree, int count, PCNode *tpNeigh) {
    PCNode *central = tpNeigh->getParent();
    PCNode *fullNeigh = central->getFullNeighInsertionPointConst(tpNeigh);
    log << "Merging child #" << count << " " << tpNeigh->index() << " next to " << fullNeigh->index() << " into parent " << central->index()
        << std::endl << "\t" << this << std::endl;
    dump(tree, "merge-" + std::to_string(count) + "-" + std::to_string(tpNeigh->index()));
}

void PCTree::LoggingObserver::makeConsecutiveDone(PCTree &tree, Stage stage, bool success) {
    log << "Resulting tree after stage ";
    switch (stage) {
        case Stage::Trivial:
            log << "Trivial";
            break;
        case Stage::NoPartials:
            log << "NoPartials";
            break;
        case Stage::InvalidTP:
            log << "InvalidTP";
            break;
        case Stage::SingletonTP:
            log << "SingletonTP";
            break;
        case Stage::Done:
            log << "Done";
            break;
        default:
            log << "???";
            break;
    }
    log << ", restriction " << (success ? "successful" : "invalid") << ": " << tree << std::endl;
}

bool PCTree::makeConsecutive(std::vector<PCNode *> &consecutiveLeaves) {
    for (auto obs:observers) obs->makeConsecutiveCalled(*this, consecutiveLeaves);
    OGDF_HEAVY_ASSERT(checkValid());

    timestamp++;
    firstPartial = lastPartial = nullptr;
    partialCount = 0;
    apexCandidate = nullptr;
    apexCandidateIsFix = false;
    terminalPathLength = 0;
    apexTPPred2 = nullptr;
    if (isTrivialRestriction(consecutiveLeaves)) {
        for (PCNode *leaf : consecutiveLeaves) {
            OGDF_ASSERT(leaf->tree == this);
        }
        for (auto obs:observers) obs->makeConsecutiveDone(*this, Observer::Stage::Trivial, true);
        return true;
    }

    PC_PROFILE_ENTER(1, "label");
    assignLabels(consecutiveLeaves);
    PC_PROFILE_EXIT(1, "label");
    if (firstPartial == nullptr) {
        OGDF_ASSERT(lastPartial == nullptr);
        OGDF_ASSERT(partialCount == 0);
        for (auto obs:observers) obs->makeConsecutiveDone(*this, Observer::Stage::NoPartials, true);
        return true;
    }
    OGDF_ASSERT(lastPartial != nullptr);
    OGDF_ASSERT(partialCount > 0);
    for (auto obs:observers) obs->labelsAssigned(*this, firstPartial, lastPartial, partialCount);

    PC_PROFILE_ENTER(1, "find_tp");
    bool find_tp = findTerminalPath();
    PC_PROFILE_EXIT(1, "find_tp");
    if (!find_tp) {
        for (auto obs:observers) obs->makeConsecutiveDone(*this, Observer::Stage::InvalidTP, false);
        return false;
    }
    OGDF_ASSERT(apexCandidate != nullptr);
    OGDF_ASSERT(apexCandidateIsFix == true);
    for (auto obs:observers) obs->terminalPathFound(*this, apexCandidate, apexTPPred2, terminalPathLength);

    PC_PROFILE_ENTER(1, "update_tp");
    if (terminalPathLength == 1) {
        OGDF_ASSERT(apexCandidate->tempInfo().tpPred == nullptr);
        updateSingletonTerminalPath();
        PC_PROFILE_EXIT(1, "update_tp");
        for (auto obs:observers) obs->makeConsecutiveDone(*this, Observer::Stage::SingletonTP, true);
        return true;
    }
    OGDF_ASSERT(apexCandidate->tempInfo().tpPred != nullptr);
    PC_PROFILE_ENTER(2, "update_tp_central");
    PCNode *central = createCentralNode();
    PC_PROFILE_EXIT(2, "update_tp_central");
    for (auto obs:observers) obs->centralCreated(*this, central);

    PCNode::TempInfo &ctinfo = central->tempInfo();
    int merged = updateTerminalPath(central, ctinfo.tpPred);
    if (apexTPPred2 != nullptr)
        merged += updateTerminalPath(central, apexTPPred2);
    OGDF_ASSERT(merged == terminalPathLength - 1);
    PC_PROFILE_EXIT(1, "update_tp");

    for (auto obs:observers) obs->makeConsecutiveDone(*this, Observer::Stage::Done, true);
#ifdef OGDF_HEAVY_DEBUG
    OGDF_HEAVY_ASSERT(checkValid());
    std::list<PCNode *> order;
    currentLeafOrder(order);
    while (order.front()->tempInfo().label != NodeLabel::Full) {
        order.push_back(order.front());
        order.pop_front();
    }
    while (order.back()->tempInfo().label == NodeLabel::Full) {
        order.push_front(order.back());
        order.pop_back();
    }
    OGDF_ASSERT(order.front()->tempInfo().label == NodeLabel::Full);
    OGDF_ASSERT(order.back()->tempInfo().label != NodeLabel::Full);
    OGDF_ASSERT(order.size() == leaves.size());
    while (order.front()->tempInfo().label == NodeLabel::Full)
        order.pop_front();
    OGDF_ASSERT(order.size() == leaves.size() - consecutiveLeaves.size());
#endif
    return true;
}

void PCTree::addPartialNode(PCNode *partial) {
    OGDF_ASSERT(partial->tempInfo().predPartial == nullptr);
    OGDF_ASSERT(partial->tempInfo().nextPartial == nullptr);
    partial->tempInfo().predPartial = lastPartial;
    if (firstPartial == nullptr) {
        firstPartial = partial;
    } else {
        lastPartial->tempInfo().nextPartial = partial;
    }
    lastPartial = partial;
    partialCount++;
}

void PCTree::removePartialNode(PCNode *partial) {
    PCNode::TempInfo &tinfo = partial->tempInfo();
    OGDF_ASSERT((tinfo.predPartial == nullptr) == (firstPartial == partial));
    if (tinfo.predPartial == nullptr) {
        firstPartial = tinfo.nextPartial;
    } else {
        tinfo.predPartial->tempInfo().nextPartial = tinfo.nextPartial;
    }
    OGDF_ASSERT((tinfo.nextPartial == nullptr) == (lastPartial == partial));
    if (tinfo.nextPartial == nullptr) {
        lastPartial = tinfo.predPartial;
    } else {
        tinfo.nextPartial->tempInfo().predPartial = tinfo.predPartial;
    }
    tinfo.predPartial = tinfo.nextPartial = nullptr;
    partialCount--;
}

void PCTree::assignLabels(std::vector<PCNode *> &fullLeaves, std::vector<PCNode *> *fullNodeOrder) {
    if (fullNodeOrder != nullptr)
        fullNodeOrder->reserve(cNodeCount + pNodeCount);
    std::queue<PCNode *> full_nodes;
    for (PCNode *leaf : fullLeaves) {
        OGDF_ASSERT(leaf && leaf->tree == this);
        OGDF_ASSERT(leaf->isLeaf());
        full_nodes.emplace(leaf);
    }

    while (!full_nodes.empty()) {
        // Once node is full, label it full and inform its non-full neighbor.
        PCNode *full_node = full_nodes.front();
        full_nodes.pop();

        if (!full_node->isLeaf()) {
            OGDF_ASSERT(full_node->tempInfo().fullNeighbors.size() == full_node->getDegree() - 1);
            OGDF_ASSERT(full_node->tempInfo().label == NodeLabel::Partial);
            removePartialNode(full_node);
        }
        full_node->tempInfo().label = NodeLabel::Full;

        PCNode *partial_neigh = full_node->getParent();
        PCNode::TempInfo *pn_tinfo = partial_neigh == nullptr ? nullptr : &partial_neigh->tempInfo();
        if (partial_neigh == nullptr || pn_tinfo->label == NodeLabel::Full) {
            // if we are the root or our parent node got full before us, we need to find our one non-full neighbor
            PC_PROFILE_ENTER(2, "label_process_neigh");
            PCNode *pred = nullptr;
            partial_neigh = full_node->child1;
            pn_tinfo = &partial_neigh->tempInfo();
            while (partial_neigh != nullptr && pn_tinfo->label == NodeLabel::Full) {
                proceedToNextSibling(pred, partial_neigh);
                pn_tinfo = &partial_neigh->tempInfo();
            }
            PC_PROFILE_EXIT(2, "label_process_neigh");
        }
        OGDF_ASSERT(partial_neigh != nullptr);
        OGDF_ASSERT(pn_tinfo->label != NodeLabel::Full);
        if (partial_neigh->isLeaf()) {
            // when a leaf becomes partial, all other leaves need to be full, which only happens during getRestriction
            OGDF_ASSERT(fullNodeOrder != nullptr);
            OGDF_ASSERT(fullNodeOrder->size() == getPNodeCount() + getCNodeCount());
            OGDF_ASSERT(full_nodes.empty());
            return;
        }

        unsigned long fullNeighborCounter = partial_neigh->addFullNeighbor(full_node);
        OGDF_ASSERT(fullNeighborCounter >= 1);
        OGDF_ASSERT(fullNeighborCounter <= partial_neigh->getDegree() - 1);
        if (fullNeighborCounter == 1) {
            OGDF_ASSERT(pn_tinfo->label == NodeLabel::Unknown);
            pn_tinfo->label = NodeLabel::Partial;
            addPartialNode(partial_neigh);
        } else {
            OGDF_ASSERT(pn_tinfo->label == NodeLabel::Partial);
        }
        if (fullNeighborCounter == partial_neigh->getDegree() - 1) {
            full_nodes.push(partial_neigh);
            if (fullNodeOrder != nullptr)
                fullNodeOrder->push_back(partial_neigh);
        }
    }
}

bool PCTree::checkTPPartialCNode(PCNode *node) {
    // check that C node's full neighbors are consecutive
    PCNode::TempInfo &tinfo = node->tempInfo();
    if (tinfo.ebEnd1 == nullptr) {
        PC_PROFILE_ENTER(2, "find_tp_cnode");
        PCNode *fullChild = tinfo.fullNeighbors.front();
        PCNode *sib1 = node->getNextNeighbor(nullptr, fullChild);
        PCNode *sib2 = node->getNextNeighbor(sib1, fullChild);
        int count = 1;
        count += findEndOfFullBlock(node, fullChild, sib1, tinfo.fbEnd1, tinfo.ebEnd1);
        count += findEndOfFullBlock(node, fullChild, sib2, tinfo.fbEnd2, tinfo.ebEnd2);
        PC_PROFILE_EXIT(2, "find_tp_cnode");
        if (count != tinfo.fullNeighbors.size()) {
            log << "C-node's full-block isn't consecutive, abort!" << std::endl;
            return false;
        }
    }
    OGDF_ASSERT(tinfo.ebEnd1 != nullptr);
    OGDF_ASSERT(tinfo.ebEnd2 != nullptr);
    OGDF_ASSERT(tinfo.fbEnd1 != nullptr);
    OGDF_ASSERT(tinfo.fbEnd2 != nullptr);
    if (tinfo.tpPred != nullptr) {
        if (tinfo.tpPred != tinfo.ebEnd1 && tinfo.tpPred != tinfo.ebEnd2) {
            log << "C-node's TP pred is not adjacent to its empty block, abort!" << std::endl;
            return false;
        }
    }
    if (node == apexCandidate && apexTPPred2 != nullptr) {
        if (apexTPPred2 != tinfo.ebEnd1 && apexTPPred2 != tinfo.ebEnd2) {
            log << "C-node's TP second pred is not adjacent to its empty block, abort!" << std::endl;
            return false;
        }
    }
    return true;
}

bool PCTree::findTerminalPath() {
    while (firstPartial != nullptr) {
        OGDF_ASSERT(lastPartial != nullptr);
        OGDF_ASSERT(partialCount > 0);

        PCNode *node = firstPartial;
        removePartialNode(node);
        PCNode *parent = node->getParent();
        PCNode::TempInfo &tinfo = node->tempInfo();
        OGDF_ASSERT(tinfo.label != NodeLabel::Full);
        log << "Processing " << tinfo.label << " " << node;
        if (parent != nullptr)
            log << " (" << parent->tempInfo().label << ")";
        log << ", current TP length is " << terminalPathLength << ": ";

        if (node->nodeType == PCNodeType::CNode && tinfo.label == NodeLabel::Partial) {
            if (!checkTPPartialCNode(node))
                return false;
        }

        OGDF_ASSERT((parent == nullptr) == (node == rootNode));
        if (node == apexCandidate || tinfo.tpSucc != nullptr) {
            // we never process a node twice, proceeding to the next entry if we detect this case
            if (tinfo.tpSucc == nullptr) {
                // we haven't counted the apex yet
                terminalPathLength++;
                tinfo.tpSucc = node;
            }
            log << "dupe!" << std::endl;
        } else if (firstPartial == nullptr && apexCandidate == nullptr) {
            // we can stop early if the queue size reached 1 right before we removed the current node, but we have neither found an apex nor an apex candidate,
            // marking the node the remaining arc points to as apex candidate
            apexCandidate = node;
            terminalPathLength++;
            log << "early stop!" << std::endl;
        } else if (node == rootNode || parent->tempInfo().label == NodeLabel::Full) {
            // we can't ascend from the root node or if our parent is full
            log << "can't ascend from root / node with full parent!" << std::endl;
            tinfo.tpSucc = node;
            terminalPathLength++;
            if (!setApexCandidate(node, false)) return false;
        } else {
            PCNode::TempInfo &parent_tinfo = parent->tempInfo();
            tinfo.tpSucc = parent;
            terminalPathLength++;
            if (node->nodeType == PCNodeType::CNode) {
                if (tinfo.label == NodeLabel::Empty) {
                    if (!node->isChildOuter(tinfo.tpPred)) {
                        log << "can't ascend from empty C-Node where TP pred is not adjacent to parent!" << std::endl;
                        tinfo.tpSucc = node;
                        if (!setApexCandidate(node, false)) return false;
                        continue;
                    } else {
                        tinfo.ebEnd1 = tinfo.fbEnd2 = tinfo.tpPred;
                        tinfo.ebEnd2 = tinfo.fbEnd1 = parent;
                    }
                } else {
                    OGDF_ASSERT(tinfo.label == NodeLabel::Partial);
                    if (!node->isChildOuter(tinfo.fbEnd1) && !node->isChildOuter(tinfo.fbEnd2)) {
                        log << "can't ascend from partial C-Node where full block is not adjacent to parent!" << std::endl;
                        tinfo.tpSucc = node;
                        if (!setApexCandidate(node, false)) return false;
                        continue;
                    }
                }
            }
            if (parent_tinfo.tpPred == nullptr) {
                parent_tinfo.tpPred = node;
                if (parent_tinfo.label != NodeLabel::Partial) {
                    if (tinfo.label == NodeLabel::Partial) {
                        parent_tinfo.tpPartialPred = node;
                        parent_tinfo.tpPartialHeight = 1;
                    } else {
                        parent_tinfo.tpPartialPred = tinfo.tpPartialPred;
                        parent_tinfo.tpPartialHeight = tinfo.tpPartialHeight + 1;
                    }
                    OGDF_ASSERT(parent_tinfo.tpPartialPred != nullptr);
                    addPartialNode(parent);
                    log << "proceed to non-partial parent (whose partial height is " << parent_tinfo.tpPartialHeight << ")" << std::endl;
                } else {
                    log << "partial parent is already queued" << std::endl;
                }
            } else if (parent_tinfo.tpPred != node) {
                log << "parent is A-shaped apex!" << std::endl;
                if (!setApexCandidate(parent, true)) return false;
                if (apexTPPred2 != nullptr && apexTPPred2 != node) {
                    log << "Conflicting apexTPPred2!" << std::endl;
                    return false;
                }
                apexTPPred2 = node;
                if (parent->nodeType == PCNodeType::CNode && parent_tinfo.label == NodeLabel::Empty) {
                    if (!node->areNeighborsAdjacent(parent_tinfo.tpPred, apexTPPred2)) {
                        log << "Apex is empty C-Node, but partial predecessors aren't adjacent!" << std::endl;
                        return false;
                    } else {
                        parent_tinfo.ebEnd1 = parent_tinfo.fbEnd2 = parent_tinfo.tpPred;
                        parent_tinfo.ebEnd2 = parent_tinfo.fbEnd1 = apexTPPred2;
                    }
                }
            }
            // if the parent is a partial C-node, it might have been processed before we were added as its tpPred(2), so re-run the checks
            if (parent->nodeType == PCNodeType::CNode && parent_tinfo.label == NodeLabel::Partial) {
                if (!checkTPPartialCNode(parent))
                    return false;
            }
        }
    }
    OGDF_ASSERT(lastPartial == nullptr);
    OGDF_ASSERT(partialCount == 0);
    if (!apexCandidateIsFix) {
        OGDF_ASSERT(apexCandidate != nullptr);
        if (apexCandidate->tempInfo().label != NodeLabel::Partial) {
            log << "Backtracking from " << apexCandidate << " to " << apexCandidate->tempInfo().tpPartialPred
                << ", TP length " << terminalPathLength << "-" << apexCandidate->tempInfo().tpPartialHeight << "=";
            terminalPathLength -= apexCandidate->tempInfo().tpPartialHeight; // this must happen before we overwrite apexCandidate
            apexCandidate = apexCandidate->tempInfo().tpPartialPred;
            log << terminalPathLength << std::endl;
        }
        apexCandidateIsFix = true;
    }
    if (apexCandidate->nodeType == PCNodeType::CNode && apexCandidate->tempInfo().label == NodeLabel::Empty) {
        OGDF_ASSERT(apexCandidate->tempInfo().tpPred != nullptr);
        OGDF_ASSERT(apexTPPred2 != nullptr);
    }
    apexCandidate->tempInfo().tpSucc = nullptr;
    return true;
}


void PCTree::appendNeighbor(PCNode *central, std::vector<PCNode *> &neighbors, PCNode *append, bool is_parent) {
    if (is_parent) {
        OGDF_ASSERT(!apexCandidate->isDetached());
        apexCandidate->replaceWith(append);
        append->appendChild(central);
    } else if (neighbors.size() < 2) {
        central->appendChild(append);
    } else if (neighbors.size() == 2) {
        OGDF_ASSERT(!central->isDetached() == (neighbors.back() == central->getParent()));
        if (!central->isDetached()) {
            central->appendChild(append, true);
        } else {
            central->appendChild(append);
        }
    } else {
        append->insertBetween(neighbors.back(), neighbors.front());
    }
    neighbors.push_back(append);
}


PCNode *PCTree::createCentralNode() {
    PCNode::TempInfo &atinfo = apexCandidate->tempInfo();
    PCNode::TempInfo *ctinfo;
    PCNode *parent = apexCandidate->getParent();
    PCNode *central;
    PCNode *tpStubApex1 = atinfo.tpPred;
    PCNode *tpStubApex2 = apexTPPred2;
    OGDF_ASSERT(tpStubApex1 != nullptr);
    OGDF_ASSERT(atinfo.label != NodeLabel::Unknown || tpStubApex2 != nullptr);

    if (apexCandidate->nodeType == PCNodeType::PNode) {
        // calculate the sizes of all sets
        bool isParentFull = false;
        if (parent != nullptr) {
            OGDF_ASSERT(parent->tempInfo().label != NodeLabel::Partial);
            isParentFull = parent->tempInfo().label == NodeLabel::Full;
        }
        int fullNeighbors = atinfo.fullNeighbors.size();
        int partialNeighbors = 1; // tpStubApex1
        if (tpStubApex2 != nullptr) partialNeighbors = 2;
        int emptyNeighbors = apexCandidate->getDegree() - fullNeighbors - partialNeighbors;
        log << "Apex is " << atinfo.label << " " << apexCandidate << " with neighbors: full=" << fullNeighbors << ", partial=" << partialNeighbors << ", empty=" << emptyNeighbors << std::endl;
        if (parent != nullptr)
            log << "Parent is " << parent->tempInfo().label << " " << parent << std::endl;

        // isolate the apex candidate
        tpStubApex1->detach();
        if (tpStubApex2 != nullptr) tpStubApex2->detach();

        // create the new central node
        c_neighbors.clear();
        central = newNode(PCNodeType::CNode);
        ctinfo = &central->tempInfo();
        ctinfo->label = atinfo.label;
        ctinfo->fullNeighbors = atinfo.fullNeighbors;
        log << "New Central has index " << central->index();

        // first step of assembly: tpStubApex1 == apexCandidate.tempInfo().tpPred
        appendNeighbor(central, c_neighbors, tpStubApex1);
        tpStubApex1->tempInfo().replaceNeighbor(apexCandidate, central);

        // second step of assembly: fullNode
        if (fullNeighbors == 1 && isParentFull) {
            apexCandidate->replaceWith(central);
            c_neighbors.push_back(parent);
            log << ", full parent is node " << parent->index();
            OGDF_ASSERT(atinfo.fullNeighbors.front() == parent);
        } else if (fullNeighbors > 0) {
            PCNode *fullNode = splitOffFullPNode(apexCandidate, isParentFull);
            appendNeighbor(central, c_neighbors, fullNode, isParentFull);
            log << ", full " << (fullNeighbors == 1 ? "neigh" : "node") << " is " << fullNode;
            OGDF_ASSERT(fullNeighbors == 1 || fullNode->getDegree() == fullNeighbors + 1);
        }

        // third step of assembly: tpStubApex2 == apexTPPred2
        int indexOfTpStubApex2 = c_neighbors.size();
        if (tpStubApex2 != nullptr) {
            appendNeighbor(central, c_neighbors, tpStubApex2);
            tpStubApex2->tempInfo().replaceNeighbor(apexCandidate, central);
        }

        // fourth step of assembly: apexCandidate (the empty node)
        if (emptyNeighbors == 1) {
            if (isParentFull || parent == nullptr) {
                PCNode *emptyNode;
                emptyNode = apexCandidate->child1;
                emptyNode->detach();
                appendNeighbor(central, c_neighbors, emptyNode);
                log << ", empty neigh is " << emptyNode;
            } else {
                apexCandidate->replaceWith(central);
                c_neighbors.push_back(parent);
                log << ", empty parent is node " << parent->index();
            }
        } else if (emptyNeighbors > 1) {
            if (isParentFull) {
                appendNeighbor(central, c_neighbors, apexCandidate);
            } else {
                apexCandidate->appendChild(central);
                c_neighbors.push_back(apexCandidate);
            }
            log << ", empty node is " << apexCandidate;
            OGDF_ASSERT(apexCandidate->getDegree() == emptyNeighbors + 1);
        }
        if (emptyNeighbors <= 1) {
            if (apexCandidate == rootNode) {
                log << ", replaced root by central";
                rootNode = central;
            }
            destroyNode(apexCandidate);
        } else {
            OGDF_ASSERT(apexCandidate->getDegree() > 2);
            OGDF_ASSERT(apexCandidate->isDetached() == (rootNode == apexCandidate));
        }
        log << std::endl;
        OGDF_ASSERT(central->isDetached() == (rootNode == central));

        // assembly done, verify
#ifdef OGDF_DEBUG
        log << "Central " << central << " and neighbors [";
        for (PCNode *neigh : neighbors)
            log << neigh->index() << ", ";
        log << "]" << std::endl;
        OGDF_ASSERT(central->getDegree() == neighbors.size());
        OGDF_ASSERT(central->getDegree() == partialNeighbors + (fullNeighbors > 0 ? 1 : 0) + (emptyNeighbors > 0 ? 1 : 0));
        OGDF_ASSERT(central->getDegree() >= 3);
        std::list<PCNode *> actual_neighbors{central->neighbors().begin(), central->neighbors().end()};
        OGDF_ASSERT(actual_neighbors.size() == neighbors.size());
        for (int i = 0; actual_neighbors.front() != neighbors.front(); i++) {
            actual_neighbors.push_back(actual_neighbors.front());
            actual_neighbors.pop_front();
            OGDF_ASSERT(i < neighbors.size());
        }
        OGDF_ASSERT(actual_neighbors.size() == neighbors.size());
        if (!std::equal(actual_neighbors.begin(), actual_neighbors.end(), neighbors.begin(), neighbors.end())) {
            actual_neighbors.push_back(actual_neighbors.front());
            actual_neighbors.pop_front();
            OGDF_ASSERT(std::equal(actual_neighbors.rbegin(), actual_neighbors.rend(), neighbors.begin(), neighbors.end()));
        }
#endif

        // update temporary pointers around central
        ctinfo->tpPred = tpStubApex1;
        ctinfo->ebEnd1 = tpStubApex1;
        ctinfo->fbEnd1 = c_neighbors[1];
        if (tpStubApex2 != nullptr) {
            ctinfo->ebEnd2 = tpStubApex2;
            ctinfo->fbEnd2 = c_neighbors[indexOfTpStubApex2 - 1];
        }
    } else {
        central = apexCandidate;
        ctinfo = &atinfo;
        if (atinfo.label == NodeLabel::Partial) {
            if (atinfo.ebEnd2 == tpStubApex1) {
                OGDF_ASSERT(atinfo.ebEnd1 == tpStubApex2 || tpStubApex2 == nullptr);
                std::swap(atinfo.ebEnd1, atinfo.ebEnd2);
                std::swap(atinfo.fbEnd1, atinfo.fbEnd2);
            }
        } else {
            OGDF_ASSERT(atinfo.label == NodeLabel::Empty);
            OGDF_ASSERT(tpStubApex2 != nullptr);
            OGDF_ASSERT(central->areNeighborsAdjacent(tpStubApex1, tpStubApex2));
            atinfo.ebEnd1 = tpStubApex1;
            atinfo.fbEnd1 = tpStubApex2;
            atinfo.fbEnd2 = tpStubApex1;
            atinfo.ebEnd2 = tpStubApex2;
        }
    }

    OGDF_ASSERT(ctinfo->tpPred == tpStubApex1);
    OGDF_ASSERT(ctinfo->ebEnd1 == tpStubApex1);
    OGDF_ASSERT(central->getFullNeighInsertionPoint(tpStubApex1) == ctinfo->fbEnd1);
    if (apexTPPred2 != nullptr) {
        OGDF_ASSERT(ctinfo->ebEnd2 == tpStubApex2);
        OGDF_ASSERT(central->getFullNeighInsertionPoint(tpStubApex2) == ctinfo->fbEnd2);
    }
    return central;
}

int PCTree::updateTerminalPath(PCNode *central, PCNode *tpNeigh) {
    int count = 0;
    PCNode *&fullNeigh = central->getFullNeighInsertionPoint(tpNeigh);
    while (tpNeigh != nullptr) {
        PCNode::TempInfo &tinfo = tpNeigh->tempInfo();
        OGDF_ASSERT(central->areNeighborsAdjacent(tpNeigh, fullNeigh));
        OGDF_ASSERT(tinfo.tpSucc != nullptr);
        OGDF_ASSERT(tinfo.label != NodeLabel::Full);
        OGDF_ASSERT(tinfo.label != NodeLabel::Unknown || tinfo.tpPred != nullptr);
        for (auto obs:observers) obs->beforeMerge(*this, count, tpNeigh);
        PCNode *nextTPNeigh = tinfo.tpPred;
        PCNode *otherEndOfFullBlock;
        if (tpNeigh->nodeType == PCNodeType::PNode) {
            PC_PROFILE_ENTER(2, "update_tp_pnode");
            if (tinfo.label == NodeLabel::Partial) {
                PCNode *fullNode = splitOffFullPNode(tpNeigh, false);
                fullNode->insertBetween(tpNeigh, fullNeigh);
                otherEndOfFullBlock = fullNeigh = fullNode;
                log << "\tFull child is " << fullNode << std::endl;
            } else {
                otherEndOfFullBlock = nullptr;
            }
            if (tinfo.tpPred != nullptr) {
                tinfo.tpPred->detach();
                tinfo.tpPred->insertBetween(fullNeigh, tpNeigh);
            }
            if (tpNeigh->childCount == 0) {
                tpNeigh->detach();
                destroyNode((PCNode *const) tpNeigh);
            } else if (tpNeigh->childCount == 1) {
                PCNode *child = tpNeigh->child1;
                child->detach();
                tpNeigh->replaceWith(child);
                destroyNode((PCNode *const) tpNeigh);
            }
            PC_PROFILE_EXIT(2, "update_tp_pnode");
        } else {
            PC_PROFILE_ENTER(2, "update_tp_cnode");
            OGDF_ASSERT(tpNeigh->nodeType == PCNodeType::CNode);
            PCNode *otherNeigh = central->getNextNeighbor(fullNeigh, tpNeigh);
            if (tpNeigh->sibling1 == fullNeigh) {
                std::swap(tpNeigh->sibling1, tpNeigh->sibling2);
            }
            OGDF_ASSERT(tpNeigh->sibling1 == otherNeigh || tpNeigh->sibling1 == nullptr);
            OGDF_ASSERT(tpNeigh->sibling2 == fullNeigh || (tpNeigh->sibling2 == nullptr && central->getParent() == fullNeigh));

            if (tpNeigh->child1 == tinfo.fbEnd1 || tpNeigh->child1 == tinfo.fbEnd2) {
                tpNeigh->flip();
            }
            PCNode *emptyOuterChild = tpNeigh->child1;
            PCNode *fullOuterChild = tpNeigh->child2;

            if (fullOuterChild == tinfo.fbEnd2) {
                std::swap(tinfo.ebEnd1, tinfo.ebEnd2);
                std::swap(tinfo.fbEnd1, tinfo.fbEnd2);
            }
            OGDF_ASSERT(fullOuterChild == tinfo.fbEnd1);
            OGDF_ASSERT(tpNeigh->getParent() == tinfo.ebEnd1);
            OGDF_ASSERT(tinfo.tpPred == nullptr || tinfo.tpPred == tinfo.ebEnd2);

            tpNeigh->mergeIntoParent();
            OGDF_ASSERT(central->areNeighborsAdjacent(fullNeigh, fullOuterChild));
            OGDF_ASSERT(central->areNeighborsAdjacent(otherNeigh, emptyOuterChild));

            if (tinfo.label == NodeLabel::Partial) {
                OGDF_ASSERT(fullOuterChild->tempInfo().label == NodeLabel::Full);
                fullNeigh = tinfo.fbEnd2;
                otherEndOfFullBlock = tinfo.fbEnd1;
            } else {
                OGDF_ASSERT(tinfo.label == NodeLabel::Empty);
                OGDF_ASSERT(fullOuterChild == tinfo.tpPred);
                otherEndOfFullBlock = nullptr;
            }
            OGDF_ASSERT(tinfo.tpPred == tinfo.ebEnd2 || tinfo.tpPred == nullptr);
            destroyNode((PCNode *const) tpNeigh);
            PC_PROFILE_EXIT(2, "update_tp_cnode");
        }

        replaceTPNeigh(central, tpNeigh, nextTPNeigh, fullNeigh, otherEndOfFullBlock);
        if (nextTPNeigh != nullptr)
            nextTPNeigh->tempInfo().replaceNeighbor(tpNeigh, central);
        tpNeigh = nextTPNeigh;
        count++;
        for (auto obs:observers) obs->afterMerge(*this, tpNeigh);
    }
    return count;
}

void PCTree::replaceTPNeigh(PCNode *central, PCNode *oldTPNeigh, PCNode *newTPNeigh, PCNode *newFullNeigh, PCNode *otherEndOfFullBlock) {
    PCNode::TempInfo &cinfo = central->tempInfo();
    OGDF_ASSERT(oldTPNeigh != nullptr);
    OGDF_ASSERT(cinfo.tpPred == cinfo.ebEnd1);
    OGDF_ASSERT(apexTPPred2 == nullptr || apexTPPred2 == cinfo.ebEnd2);
    if (oldTPNeigh == cinfo.tpPred) {
        cinfo.tpPred = cinfo.ebEnd1 = newTPNeigh;
        cinfo.fbEnd1 = newFullNeigh;
        if (cinfo.fbEnd2 == oldTPNeigh) {
            if (otherEndOfFullBlock != nullptr) {
                cinfo.fbEnd2 = otherEndOfFullBlock;
            } else {
                cinfo.fbEnd2 = newTPNeigh;
            }
        }
    } else {
        OGDF_ASSERT(oldTPNeigh == apexTPPred2);
        apexTPPred2 = cinfo.ebEnd2 = newTPNeigh;
        cinfo.fbEnd2 = newFullNeigh;
        if (cinfo.fbEnd1 == oldTPNeigh) {
            if (otherEndOfFullBlock != nullptr) {
                cinfo.fbEnd1 = otherEndOfFullBlock;
            } else {
                cinfo.fbEnd1 = newFullNeigh;
            }
        }
    }
    OGDF_ASSERT(apexTPPred2 != oldTPNeigh);
    OGDF_ASSERT(cinfo.ebEnd1 != oldTPNeigh);
    OGDF_ASSERT(cinfo.ebEnd2 != oldTPNeigh);
    OGDF_ASSERT(cinfo.fbEnd1 != oldTPNeigh);
    OGDF_ASSERT(cinfo.fbEnd2 != oldTPNeigh);
    if (cinfo.ebEnd1 != nullptr) {
        OGDF_ASSERT(central->areNeighborsAdjacent(cinfo.ebEnd1, cinfo.fbEnd1));
    }
    if (cinfo.ebEnd2 != nullptr) {
        OGDF_ASSERT(apexTPPred2 == nullptr || apexTPPred2 == cinfo.ebEnd2);
        OGDF_ASSERT(central->areNeighborsAdjacent(cinfo.ebEnd2, cinfo.fbEnd2));
    }
}

///////////////////////////////////////////////////////////////////////////////

// findTerminalPath utils

int PCTree::findEndOfFullBlock(PCNode *node, PCNode *pred, PCNode *curr, PCNode *&fullEnd, PCNode *&emptyEnd) const {
    PCNode *start = fullEnd = pred;
    int count = 0;
    while (curr->tempInfo().label == NodeLabel::Full) {
        PCNode *next = node->getNextNeighbor(pred, curr);
        OGDF_ASSERT(next != start);
        pred = fullEnd = curr;
        curr = next;
        count++;
    }
    emptyEnd = curr;
    return count;
}

bool PCTree::setApexCandidate(PCNode *ac, bool fix) {
    if (apexCandidate == nullptr) {
        apexCandidate = ac;
        apexCandidateIsFix = fix;
        return true;
    } else if (apexCandidate == ac) {
        if (!apexCandidateIsFix && fix)
            apexCandidateIsFix = true;
        return true;
    } else {
        // we reached a node from which we can't ascend (nonFix), but also found the actual apex (fix)
        // check whether we ascended too far, i.e. the nonFix node is a parent of the fix apex
        if ((!fix && apexCandidateIsFix) || (fix && !apexCandidateIsFix)) {
            PCNode *fixAC, *nonFixAC;
            if (!fix && apexCandidateIsFix) {
                fixAC = apexCandidate;
                nonFixAC = ac;
            } else {
                OGDF_ASSERT(fix && !apexCandidateIsFix);
                fixAC = ac;
                nonFixAC = apexCandidate;
            }
            PCNode::TempInfo &fixTI = fixAC->tempInfo();
            PCNode::TempInfo &nonFixTI = nonFixAC->tempInfo();
            if (nonFixTI.label == NodeLabel::Empty) {
                if (fixTI.label == NodeLabel::Partial) {
                    // if the fix apex is partial, it must be the tpPartialPred of the ascended-too-far empty/nonFix node
                    if (nonFixTI.tpPartialPred == fixAC) {
                        terminalPathLength -= nonFixTI.tpPartialHeight;
                        apexCandidate = fixAC;
                        apexCandidateIsFix = true;
                        return true;
                    }
                } else {
                    OGDF_ASSERT(fixTI.label == NodeLabel::Empty);
                    // otherwise the fix apex is also empty, but they must have the same tpPartialPred
                    if (nonFixTI.tpPartialPred == fixTI.tpPartialPred) {
                        terminalPathLength -= (nonFixTI.tpPartialHeight - fixTI.tpPartialHeight);
                        apexCandidate = fixAC;
                        apexCandidateIsFix = true;
                        return true;
                    }
                }
            }
        }
        log << "Conflicting " << (apexCandidateIsFix ? "" : "non-") << "fix apex candidates (" << apexCandidate << ") and (" << ac << ")" << std::endl;
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////

// updateTerminalPath utils

void PCTree::updateSingletonTerminalPath() {
    PCNode::TempInfo &atinfo = apexCandidate->tempInfo();
    OGDF_ASSERT(atinfo.tpPred == nullptr);
    OGDF_ASSERT(apexTPPred2 == nullptr);
    int fullNeighbors = atinfo.fullNeighbors.size();
    int emptyNeighbors = apexCandidate->getDegree() - fullNeighbors;
    if (apexCandidate->nodeType == PCNodeType::PNode && fullNeighbors > 1 && emptyNeighbors > 1) {
        // parent is handled and degree is always greater than 1
        PCNode *fullNode = splitOffFullPNode(apexCandidate, true);
        PCNode *parent = apexCandidate->getParent();
        if (parent != nullptr && parent->tempInfo().label == NodeLabel::Full) {
            apexCandidate->replaceWith(fullNode);
            fullNode->appendChild(apexCandidate);
        } else {
            apexCandidate->appendChild(fullNode);
        }
    }
}

PCNode *PCTree::splitOffFullPNode(PCNode *node, bool skip_parent) {
    auto &tinfo = node->tempInfo();
    auto *parent = node->getParent();
    if (tinfo.fullNeighbors.size() == 1) {
        PCNode *fullNode = tinfo.fullNeighbors.front();
        OGDF_ASSERT(fullNode != parent);
        OGDF_ASSERT(node->isParentOf(fullNode));
        fullNode->detach();
        for (auto obs:observers) obs->fullNodeSplit(*this, fullNode);
        return fullNode;
    }
    PCNode *fullNode = newNode(PCNodeType::PNode);
    fullNode->tempInfo().label = NodeLabel::Full;
    for (PCNode *fullChild : tinfo.fullNeighbors) {
        if (skip_parent) {
            if (fullChild == parent) continue;
        } else {
            OGDF_ASSERT(fullChild != parent);
        }
        OGDF_ASSERT(node->isParentOf(fullChild));
        fullChild->detach();
        fullNode->appendChild(fullChild);
        fullNode->tempInfo().fullNeighbors.push_back(fullChild);
    }
    if (skip_parent && parent != nullptr && parent->tempInfo().label == NodeLabel::Full) {
        OGDF_ASSERT(fullNode->getDegree() >= 1);
    } else {
        OGDF_ASSERT(fullNode->getDegree() >= 2);
    }
    for (auto obs:observers) obs->fullNodeSplit(*this, fullNode);
    return fullNode;
}
