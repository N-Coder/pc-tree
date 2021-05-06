#include "PCTree.h"
#include "PCNode.h"
#include "PCArc.h"

using namespace pc_tree::hsu;

bool PCTree::makeConsecutive(std::vector<PCNode *> &consecutiveLeaves) {
    return makeConsecutive(consecutiveLeaves, nullptr, nullptr, nullptr, nullptr);
}

bool PCTree::makeConsecutive(std::vector<PCNode *> &consecutiveLeaves, PCArc **fullBlockFirst, PCArc **fullBlockLast,
                             PCArc **emptyBlockFirst, PCArc **emptyBlockLast) {
    OGDF_HEAVY_ASSERT(isValid());
    for (PCNode *node : terminalPath) {
        deleteNodePointer(node);
    }

    terminalPath.clear();
    if (consecutiveLeaves.size() <= 1 || consecutiveLeaves.size() == leaves.size()) {
        if (consecutiveLeaves.size() == 1) {
            OGDF_ASSERT(consecutiveLeaves.front()->tree == this);
            if (fullBlockFirst) *fullBlockFirst = consecutiveLeaves.front()->parentArc;
            if (fullBlockLast) *fullBlockLast = consecutiveLeaves.front()->parentArc;
            if (emptyBlockFirst) *emptyBlockFirst = consecutiveLeaves.front()->parentArc->neighbor1;
            if (emptyBlockLast) *emptyBlockLast = consecutiveLeaves.front()->parentArc->neighbor2;
        }

        return true;
    }

    if (consecutiveLeaves.size() == leaves.size() - 1 && !fullBlockFirst && !fullBlockLast && !emptyBlockFirst &&
        !emptyBlockLast) {
        // If pointers to blocks are not required, we can immediately stop,
        // otherwise we have to find the one non-full leaf.
        return true;
    }

    timestamp++;
    invalid = false;
    partialNodes.clear();

    // assign labels
    assignLabels(consecutiveLeaves);

    if (consecutiveLeaves.size() == leaves.size() - 1) {
        // Find the non-full leaf and update the pointers.
        PCNode *emptyLeaf = nullptr;
        for (PCNode *leaf : leaves) {
            if (leaf->getLabel(timestamp) == NodeLabel::Unknown) {
                emptyLeaf = leaf;
                break;
            }
        }

        if (fullBlockFirst) *fullBlockFirst = emptyLeaf->parentArc->neighbor1;
        if (fullBlockLast) *fullBlockLast = emptyLeaf->parentArc->neighbor2;
        if (emptyBlockFirst) *emptyBlockFirst = emptyLeaf->parentArc;
        if (emptyBlockLast) *emptyBlockLast = emptyLeaf->parentArc;
        return true;
    }

    // find partial nodes
    for (PCNode *leaf : consecutiveLeaves) {
        PCArc *informedArc = leaf->getInformedArc(timestamp);

        while (informedArc != nullptr) {
            PCNode *node = informedArc->getyNode(timestamp);

            if (node == nullptr || node->isMarkedLabeling(timestamp)) {
                break;
            }

            node->setMarkedLabeling(true, timestamp);

            // First non-full node has to be partial.
            if (node->getLabel(timestamp) != NodeLabel::Full) {
                partialNodes.push_back(node);
                node->setLabel(NodeLabel::Partial, timestamp);
            }

            informedArc = node->getInformedArc(timestamp);
        }
    }

    if (partialNodes.empty()) {
        return true;
    }


    findTerminalPath();

    if (invalid) {
        return false;
    }

    if (terminalPath.size() == 1 && terminalPath.front()->nodeType == PCNodeType::CNode) {
        // Reduction is trivial, find the start/end of the blocks if necessary and stop.

        if (fullBlockFirst) {
            PCArc *firstFullArc;
            PCArc *lastFullArc;
            PCArc *firstEmptyArc;
            PCArc *lastEmptyArc;

            firstFullArc = terminalPath.front()->temporaryBlockPointer;
            lastFullArc = firstFullArc->getBlockPointer(timestamp);

            if (firstFullArc->neighbor2->twin->getyNode(timestamp) == nullptr ||
                firstFullArc->neighbor2->twin->getyNode(timestamp)->getLabel(timestamp) != NodeLabel::Full) {
                firstEmptyArc = firstFullArc->neighbor2;
            } else {
                firstEmptyArc = firstFullArc->neighbor1;
            }

            if (lastFullArc->neighbor1->twin->getyNode(timestamp) == nullptr ||
                lastFullArc->neighbor1->twin->getyNode(timestamp)->getLabel(timestamp) != NodeLabel::Full) {
                lastEmptyArc = lastFullArc->neighbor1;
            } else {
                lastEmptyArc = lastFullArc->neighbor2;
            }

            if (fullBlockFirst) *fullBlockFirst = firstFullArc;
            if (fullBlockLast) *fullBlockLast = lastFullArc;
            if (emptyBlockFirst) *emptyBlockFirst = firstEmptyArc;
            if (emptyBlockLast) *emptyBlockLast = lastEmptyArc;
        }

        return true;
    }

    updateTerminalPath();

    if (fullBlockFirst) *fullBlockFirst = centralCNode->fullBlockFirst;
    if (fullBlockLast) *fullBlockLast = centralCNode->fullBlockLast;
    if (emptyBlockFirst) *emptyBlockFirst = centralCNode->emptyBlockFirst;
    if (emptyBlockLast) *emptyBlockLast = centralCNode->emptyBlockLast;

    centralCNode->resetLists();
    delete centralCNode;
    return true;
}

void PCTree::assignLabels(std::vector<PCNode *> &fullLeaves, std::vector<PCArc *> *informedArcOrder) {
    for (PCNode *node : fullLeaves) {
        OGDF_ASSERT(node->tree == this);
        node->setLabel(NodeLabel::Full, timestamp);
        nextArcQueue.push(node->parentArc);
    }

    while (!nextArcQueue.empty()) {
        PCArc *nextArc = nextArcQueue.front();
        nextArcQueue.pop();

        PCNode *nextNode = nextArc->getyNode(timestamp);

        if (nextNode != nullptr) {
            if (nextNode->nodeType == PCNodeType::PNode && nextNode->getLabel(timestamp) != NodeLabel::Full) {
                // Set arc as informed arc for the previous node and add it to the full neighbors of the informed node.
                PCNode *previousNode = nextArc->twin->getyNode(timestamp);
                previousNode->setInformedArc(nextArc, timestamp);
                if (informedArcOrder && previousNode->nodeType != PCNodeType::Leaf) informedArcOrder->push_back(nextArc);
                unsigned long fullNeighborCounter = nextNode->addFullNeighbor(timestamp, nextArc);
                if (fullNeighborCounter >= nextNode->degree - 1) {
                    // Once yNode is full, label it full and inform its non-full neighbor.
                    nextNode->setLabel(NodeLabel::Full, timestamp);
                    PCArc *initial = nextNode->parentArc != nullptr ? nextNode->parentArc->twin : nextArc;
                    PCArc *previous = initial->neighbor2;
                    PCArc *current = initial;
                    while (current != initial->neighbor2 && current->twin->getyNode(timestamp) != nullptr &&
                           current->twin->getyNode(timestamp)->getLabel(timestamp) == NodeLabel::Full) {
                        getNextArc(previous, current);
                    }
                    nextArcQueue.push(current->twin);

                }
            }
        } else {
            // manage blocks at C-node
            PCNode *previousNode = nextArc->twin->getyNode(timestamp);
            previousNode->setInformedArc(nextArc, timestamp);
            if (informedArcOrder && previousNode->nodeType != PCNodeType::Leaf) informedArcOrder->push_back(nextArc);
            expandBlocks(nextArc);
        }
    }
}

void PCTree::expandBlocks(PCArc *arc) {
    PCArc *neighbor1 = arc->neighbor1;
    PCArc *neighbor2 = arc->neighbor2;

    PCArc *blockPointer1 = neighbor1->getBlockPointer(timestamp);
    PCArc *blockPointer2 = neighbor2->getBlockPointer(timestamp);

    // 4 different cases, depending on whether the two neighbors belong to a full block or not.
    if (blockPointer1 == nullptr && blockPointer2 == nullptr) {
        // Create new block with new C node object
        arc->setBlockPointer(arc, timestamp);
        PCNode *cNode = createNode(PCNodeType::CNode);
        arc->setyNode(cNode, timestamp);
        cNode->temporaryBlockPointer = arc;

        // Set parent arc if adjacent to it
        if (arc->neighbor1->twin->yParent) {
            arc->yNode.first->parentArc = arc->neighbor1->twin;
        } else if (arc->neighbor2->twin->yParent) {
            arc->yNode.first->parentArc = arc->neighbor2->twin;
        } else if (arc->twin->yParent) {
            arc->yNode.first->parentArc = arc->twin;
        }
    } else if (blockPointer1 != nullptr && blockPointer2 == nullptr) {
        appendArc(arc, neighbor1, neighbor2);
    } else if (blockPointer1 == nullptr) {
        appendArc(arc, neighbor2, neighbor1);
    } else {
        // Combine two blocks

        neighbor1->blockPointer.first = nullptr;
        neighbor2->blockPointer.first = nullptr;
        blockPointer1->setBlockPointer(blockPointer2, timestamp);
        blockPointer2->setBlockPointer(blockPointer1, timestamp);

        // If one of the two blocks' C-node object knows the parent arc, give it to the other block
        if (neighbor1->getyNode(timestamp)->parentArc != nullptr) {
            blockPointer2->setyNode(neighbor1->getyNode(timestamp), timestamp);
        } else if (neighbor2->getyNode(timestamp)->parentArc != nullptr) {
            blockPointer1->setyNode(neighbor2->getyNode(timestamp), timestamp);
        } else {
            blockPointer1->setyNode(blockPointer2->getyNode(timestamp), timestamp);
        }
        blockPointer1->getyNode(timestamp)->temporaryBlockPointer = blockPointer1;
        // Delete the old C-node objects in the neighbors unless its the end of a block.
        if (blockPointer2->getBlockPointer(timestamp) != neighbor1) {
            neighbor1->setyNode(nullptr, timestamp);
        }

        if (blockPointer1->getBlockPointer(timestamp) != neighbor2) {
            neighbor2->setyNode(nullptr, timestamp);
        }

        PCArc *commonNeighbor = getCommonNeighbor(blockPointer1, blockPointer2, arc);

        // If both block pointers have the same neighbor that is not the arc we just added to the blocks,
        // the node is full and we inform the non-full neighbor.
        if (commonNeighbor != nullptr) {
            blockPointer1->getyNode(timestamp)->setLabel(NodeLabel::Full, timestamp);
            commonNeighbor->setyNode(blockPointer1->getyNode(timestamp), timestamp);
            nextArcQueue.push(commonNeighbor->twin);
        }
    }
}

PCArc *PCTree::getCommonNeighbor(PCArc *arc1, PCArc *arc2, PCArc *ignoredArc) {
    if ((arc1->neighbor1 == arc2->neighbor1 || arc1->neighbor1 == arc2->neighbor2) && arc1->neighbor1 != ignoredArc) {
        return arc1->neighbor1;
    } else if ((arc1->neighbor2 == arc2->neighbor1 || arc1->neighbor2 == arc2->neighbor2) &&
               arc1->neighbor2 != ignoredArc) {
        return arc1->neighbor2;
    }

    return nullptr;
}

void PCTree::appendArc(PCArc *arc, PCArc *blockArc, PCArc *otherNeighbor) {
    // adopt temporary c node object
    arc->setyNode(blockArc->getyNode(timestamp), timestamp);
    arc->blockPointer = blockArc->blockPointer;
    blockArc->getBlockPointer(timestamp)->setBlockPointer(arc, timestamp);
    if (arc != blockArc->getBlockPointer(timestamp)) {
        blockArc->blockPointer.first = nullptr;
        blockArc->setyNode(nullptr, timestamp);
    }

    PCNode *node = arc->getyNode(timestamp);
    node->temporaryBlockPointer = arc;

    // If the block is adjacent to the parent edge, give the C-node a pointer to it
    if (arc->twin->yParent) {
        node->parentArc = arc->twin;
    } else if (otherNeighbor->twin->yParent) {
        node->parentArc = otherNeighbor->twin;
    }


    PCArc *current = otherNeighbor;
    PCArc *previous = arc;
    getNextArc(previous, current);
    // If node is full, inform non-full neighbor
    if (arc->getBlockPointer(timestamp) == current) {
        otherNeighbor->setyNode(arc->getyNode(timestamp), timestamp);
        node->setLabel(NodeLabel::Full, timestamp);
        nextArcQueue.push(otherNeighbor->twin);
    }

}

void PCTree::findTerminalPath() {
    unsigned long originalCount = partialNodes.size();
    unsigned long pathCount = partialNodes.size();

    PCNode *highestNode = partialNodes.front();

    deleteNodePointer(apexCandidate);
    apexCandidate = nullptr;
    invalid = false;

    while (!partialNodes.empty() && !invalid) {
        PCNode *currentNode = partialNodes.front();
        partialNodes.pop_front();

        if (currentNode->isMarkedParallelSearch(timestamp)) {
            pathCount--;
            continue;
        }
        currentNode->setMarkedParallelSearch(true, timestamp);

        if (pathCount == 1) {
            highestNode = currentNode;
            break;
        }

        PCNode *parent = getTerminalPathParent(currentNode);
        if (parent == nullptr) {
            // highest point of the subtree is reached, do not decrease path counter and let all other paths finish.
            highestNode = currentNode;
            continue;
        }

        setPredecessor(parent, currentNode);
        partialNodes.push_back(parent);
    }

    if (invalid) {
        return;
    }

    unsigned long partialCounter = 0;

    if (apexCandidate == nullptr) {
        apexCandidate = assignNodePointer(findApex(highestNode), apexCandidate);
    }


    terminalPath.push_front(assignNodePointer(apexCandidate));
    if (apexCandidate->getLabel(timestamp) == NodeLabel::Partial) {
        partialCounter++;
    }

    // Add the predecessors of the apex at different ends of the list to ensure the result is sorted.
    partialCounter += addToTerminalPath(apexCandidate->getTerminalPathPredecessor2(timestamp), false);
    partialCounter += addToTerminalPath(apexCandidate->getTerminalPathPredecessor1(timestamp), true);

    if (partialCounter != originalCount) {
        invalid = true;
    }


}

PCNode *PCTree::findApex(PCNode *highestNode) const {
    auto current = highestNode;

    // The first partial node below the highest node has to be the apex
    while (current->getLabel(timestamp) != NodeLabel::Partial) {
        current = current->getTerminalPathPredecessor1(timestamp);
    }

    return current;
}

unsigned long PCTree::addToTerminalPath(PCNode *start, bool front) {
    auto current = start;

    unsigned long partialCounter = 0;

    // Add all predecessors at the right side of the terminal path list and count all partial nodes
    while (current != nullptr) {
        if (front) {
            terminalPath.push_front(assignNodePointer(current));
        } else {
            terminalPath.push_back(assignNodePointer(current));
        }

        if (current->getLabel(timestamp) == NodeLabel::Partial) {
            partialCounter++;
        }

        current = current->getTerminalPathPredecessor1(timestamp);
    }

    return partialCounter;
}

void PCTree::setApexCandidate(PCNode *node) {
    if (apexCandidate != nullptr && apexCandidate != node) {
        invalid = true;
    } else {
        apexCandidate = assignNodePointer(node, apexCandidate);
    }
}

void PCTree::setPredecessor(PCNode *node, PCNode *pred) {
    if (node->getTerminalPathPredecessor2(timestamp) != nullptr) {
        // No node can have three predecessors
        invalid = true;
    } else if (node->getTerminalPathPredecessor1(timestamp) != nullptr) {
        if (node->getTerminalPathPredecessor1(timestamp)->parentArc == pred->parentArc) {
            // If both predecessors have the same parent arc, there are two C-node objects for the same C-node,
            // which means the full blocks are not adjacent
            invalid = true;
            return;
        }
        node->setTerminalPathPredecessor2(pred, timestamp);
        setApexCandidate(node);
    } else {
        node->setTerminalPathPredecessor1(pred, timestamp);
    }
}

PCNode *PCTree::getTerminalPathParent(PCNode *node) {
    if (node->parentArc == nullptr) {
        if (node->nodeType == PCNodeType::CNode && node->getLabel(timestamp) == NodeLabel::Partial) {
            // A partial C-node that does not know its parent arc has to be the apex
            setApexCandidate(node);
        }
        return nullptr;
    }


    auto parent = node->parentArc->getyNode(timestamp);
    if (parent != nullptr) {
        if (parent->getLabel(timestamp) == NodeLabel::Full) {
            // No full node can be on the terminal path
            return nullptr;
        } else {
            return parent;
        }
    }

    // Obtain a C-node object from one of the neighbors, if one exists.
    auto parentNodeNeighbor1 = node->parentArc->neighbor1->getyNode(timestamp);
    auto parentNodeNeighbor2 = node->parentArc->neighbor2->getyNode(timestamp);

    if (parentNodeNeighbor1 != nullptr && parentNodeNeighbor2 != nullptr) {
        if (parentNodeNeighbor1 != parentNodeNeighbor2) {
            // If both neighbors have different C-node objects, the full blocks are not adjacent.
            invalid = true;
            return nullptr;
        }
        node->parentArc->setyNode(parentNodeNeighbor1, timestamp);
        return parentNodeNeighbor1;
    } else if (parentNodeNeighbor1 != nullptr) {
        if (parentNodeNeighbor1->getLabel(timestamp) == PCTree::NodeLabel::Partial &&
            !isFullArc(node->parentArc->neighbor1)) {
            // If neighbor 1 holds a partial C-node object, but does not belong to a full node itself,
            // there has to be a full block that's not between two terminal path edges.
            invalid = true;
            return nullptr;
        }
        node->parentArc->setyNode(parentNodeNeighbor1, timestamp);
        return parentNodeNeighbor1;
    } else if (parentNodeNeighbor2 != nullptr) {
        if (parentNodeNeighbor2->getLabel(timestamp) == PCTree::NodeLabel::Partial &&
            !isFullArc(node->parentArc->neighbor2)) {
            invalid = true;
            return nullptr;
        }
        node->parentArc->setyNode(parentNodeNeighbor2, timestamp);
        return parentNodeNeighbor2;
    }

    // Otherwise create a new one
    auto newParent = createNode(PCNodeType::CNode);
    node->parentArc->setyNode(newParent, timestamp);

    // Set the parent arc if adjacent to it
    if (!node->parentArc->neighbor1->yParent) {
        newParent->parentArc = node->parentArc->neighbor1->twin;
    } else if (!node->parentArc->neighbor2->yParent) {
        newParent->parentArc = node->parentArc->neighbor2->twin;
    }

    return newParent;
}

void PCTree::updateTerminalPath() {
    PCNode *previous = nullptr;
    for (PCNode *node : terminalPath) {
        // delete edges to neighbors on terminal path and find pointers to the ends of the blocks
        deleteEdge(previous, node);
        previous = node;
    }

    centralCNode = createNode(PCNodeType::CNode);

    for (PCNode *node : terminalPath) {
        if (node->nodeType == PCNodeType::PNode) {
            splitPNode(node);
        } else {
            splitCNode(node);
        }
        node->resetLists();
    }

    combineLists(centralCNode->fullBlockFirst, centralCNode->emptyBlockFirst);
    combineLists(centralCNode->fullBlockLast, centralCNode->emptyBlockLast);

    // C node of degree 2
    if (terminalPath.size() == 1 && terminalPath.front()->nodeType == PCNodeType::PNode &&
        centralCNode->fullBlockFirst->neighbor1 == centralCNode->fullBlockFirst->neighbor2) {
        deleteDegreeTwoNode(centralCNode->fullBlockFirst);
        centralCNode->emptyBlockFirst = centralCNode->fullBlockFirst->neighbor1;
        centralCNode->emptyBlockLast = centralCNode->fullBlockFirst->neighbor2;
    }
}

void PCTree::findBlocks(PCNode *node, PCArc *entryArc) {
    if (node->nodeType == PCNodeType::PNode) {
        return;
    }

    // For a C-node, determine the pointers to the ends of the full and empty blocks and extract
    // them from the circular list by setting the neighbors of the ends to nullptrs.
    PCArc *neighbor1 = entryArc->neighbor1;
    PCArc *neighbor2 = entryArc->neighbor2;
    PCArc *fullArc = nullptr;
    PCArc *otherArc = nullptr;
    if (isFullArc(neighbor1)) {
        fullArc = neighbor1;
        otherArc = neighbor2;
    } else if (isFullArc(neighbor2)) {
        fullArc = neighbor2;
        otherArc = neighbor1;
    }

    if (node == terminalPath.front() || node == terminalPath.back()) {
        node->fullBlockLast = fullArc;
        node->fullBlockFirst = fullArc->getBlockPointer(timestamp);
        node->emptyBlockLast = otherArc;
        if (!isFullArc(node->fullBlockFirst->neighbor1) && node->fullBlockFirst->neighbor1 != entryArc) {
            node->emptyBlockFirst = node->fullBlockFirst->neighbor1;
        } else {
            node->emptyBlockFirst = node->fullBlockFirst->neighbor2;
        }

        if (node == terminalPath.back()) {
            // The lists are oppositely directed for the two terminal nodes.
            node->swapEnds();
        }
    } else {
        if (fullArc != nullptr) {
            node->fullBlockFirst = fullArc;
            node->fullBlockLast = fullArc->getBlockPointer(timestamp);
            if (!isTerminalPathArc(node, otherArc)) {
                // PCNode contains both a full and an empty block.
                node->emptyBlockFirst = otherArc;
                if (!isFullArc(node->fullBlockLast->neighbor1) && node->fullBlockLast->neighbor1 != entryArc) {
                    node->emptyBlockLast = node->fullBlockLast->neighbor1->getNextArc(node->fullBlockLast);
                    setNeighborNull(node->emptyBlockLast, node->fullBlockLast->neighbor1);
                    setNeighborNull(node->fullBlockLast, node->fullBlockLast->neighbor1);
                } else {
                    node->emptyBlockLast = node->fullBlockLast->neighbor2->getNextArc(node->fullBlockLast);
                    setNeighborNull(node->emptyBlockLast, node->fullBlockLast->neighbor2);
                    setNeighborNull(node->fullBlockLast, node->fullBlockLast->neighbor2);
                }
            } else {
                // PCNode contains only a full block.
                setNeighborNull(node->fullBlockLast, otherArc);
            }
        } else if (isTerminalPathArc(node, neighbor1)) {
            // PCNode contains only an empty block
            node->emptyBlockFirst = neighbor2;
            node->emptyBlockLast = neighbor1->getNextArc(entryArc);
            setNeighborNull(node->emptyBlockLast, neighbor1);
        } else if (isTerminalPathArc(node, neighbor2)) {
            // PCNode contains only an empty block
            node->emptyBlockFirst = neighbor1;
            node->emptyBlockLast = neighbor2->getNextArc(entryArc);
            setNeighborNull(node->emptyBlockLast, neighbor2);
        }
    }

    // If a block only consists of a single arc, remember its rotation
    if (node->fullBlockFirst != nullptr && node->fullBlockFirst == node->fullBlockLast) {
        fullArc->preferNeighbor1 = fullArc->neighbor1 == entryArc;
    }

    if (node->emptyBlockFirst != nullptr && node->emptyBlockFirst == node->emptyBlockLast) {
        node->emptyBlockFirst->preferNeighbor1 = node->emptyBlockFirst->neighbor1 == entryArc;
    }

    setNeighborNull(neighbor1, entryArc);
    setNeighborNull(neighbor2, entryArc);

}

void PCTree::deleteEdge(PCNode *previous, PCNode *current) {
    if (previous == nullptr) {
        return;
    }

    PCArc *currentNodeEntryArc;
    // Determine the terminal path arc between previous and current
    if (current->parentArc != nullptr && current->parentArc->getyNode(timestamp) == previous) {
        currentNodeEntryArc = current->parentArc->twin;
    } else {
        currentNodeEntryArc = previous->parentArc;
    }

    findBlocks(current, currentNodeEntryArc);

    if (previous == terminalPath.front()) {
        findBlocks(previous, currentNodeEntryArc->twin);
    }

    if (previous->nodeType == PCNodeType::PNode) {
        previous->degree--;
        currentNodeEntryArc->twin->extract();

        // Keep pointers to the neighbors of the deleted edge in case P-node has no full neighbors.
        previous->emptyBlockFirst = currentNodeEntryArc->twin->neighbor2;
        previous->emptyBlockLast = currentNodeEntryArc->twin->neighbor1;

    }

    if (current->nodeType == PCNodeType::PNode) {
        current->degree--;
        currentNodeEntryArc->extract();

        // Keep pointers to the neighbors of the deleted edge in case P-node has no full neighbors.
        current->emptyBlockFirst = currentNodeEntryArc->neighbor2;
        current->emptyBlockLast = currentNodeEntryArc->neighbor1;
    }


    delete currentNodeEntryArc->twin;
    delete currentNodeEntryArc;
}

bool PCTree::isTerminalPathArc(PCNode *node, PCArc *arc) {
    // Only works for non-terminal nodes on the terminal path
    if (node == apexCandidate) {
        return node->getTerminalPathPredecessor1(timestamp)->parentArc == arc ||
               node->getTerminalPathPredecessor2(timestamp)->parentArc == arc;
    } else {
        return node->getTerminalPathPredecessor1(timestamp)->parentArc == arc ||
               node->parentArc->twin == arc;
    }
}

void PCTree::splitPNode(PCNode *node) {
    auto &fullNeighbors = node->getFullNeighbors(timestamp);
    bool fullParentArc = false;
    PCArc *fullBlockFirst = nullptr;
    PCArc *fullBlockLast = nullptr;
    PCNode *fullPNode = nullptr;
    if (fullNeighbors.size() > 1) {
        fullPNode = createNode(PCNodeType::PNode);
    }

    for (auto it = fullNeighbors.begin(); it != fullNeighbors.end(); ++it) {
        // Extract all full arcs and add them to the list of full arcs

        PCArc *fullArc = *it;
        if (it == fullNeighbors.begin()) {
            fullBlockFirst = fullArc;
            node->emptyBlockFirst = nullptr;
            node->emptyBlockLast = nullptr;
        }

        if (std::next(it) == fullNeighbors.end() && fullArc->neighbor1 != fullArc) {
            // Determine the ends of the empty block when extracting the last full arc
            node->emptyBlockFirst = fullArc->neighbor1;
            node->emptyBlockLast = fullArc->neighbor2;
        }

        if (!fullArc->yParent) {
            fullParentArc = true;
        }

        if (fullNeighbors.size() > 1) {
            fullArc->setyNode(fullPNode, timestamp);
        }

        fullArc->extract();
        fullArc->neighbor1 = nullptr;
        fullArc->neighbor2 = nullptr;
        combineLists(fullArc, fullBlockLast);
        fullBlockLast = fullArc;
    }

    if (fullBlockLast != nullptr) {
        if (fullBlockFirst == fullBlockLast) {
            // If only one full neighbor, directly add it to the central C-node.
            fullBlockLast->setyNode(nullptr, timestamp);
            addToCentralCNode(fullBlockFirst, fullBlockLast, true);
        } else {
            combineLists(fullBlockLast, fullBlockFirst);
            fullPNode->fullBlockFirst = fullBlockFirst;
            fullPNode->fullBlockLast = fullBlockLast;
            PCArc *newArc = preparePNode(fullPNode, (fullParentArc && node == apexCandidate),
                                       fullNeighbors.size() + 1);
            addToCentralCNode(newArc, newArc, true);
            fullPNode->resetLists();
        }
    }

    if (node->emptyBlockLast != nullptr) {
        if (node->emptyBlockLast == node->emptyBlockFirst) {
            // If only one empty neighbor, directly add it to the central C-node.
            setNeighborNull(node->emptyBlockLast, node->emptyBlockFirst);
            setNeighborNull(node->emptyBlockLast, node->emptyBlockFirst);
            node->emptyBlockLast->setyNode(nullptr, timestamp);
            addToCentralCNode(node->emptyBlockFirst, node->emptyBlockLast, false);
        } else {
            PCArc *newArc = preparePNode(node, (!fullParentArc && node == apexCandidate),
                                       node->degree - fullNeighbors.size() + 1);
            addToCentralCNode(newArc, newArc, false);
        }

    }

    node->resetLists();
}

PCArc *PCTree::preparePNode(PCNode *pNode, bool isParent, unsigned long degree) {
    // Create a new edge and insert it into the circular list of pNode. Return the new arc that will be added to the
    // central C-PCNode
    PCArc *first;
    PCArc *last;
    if (pNode->fullBlockLast != nullptr) {
        first = pNode->fullBlockFirst;
        last = pNode->fullBlockLast;
    } else {
        first = pNode->emptyBlockFirst;
        last = pNode->emptyBlockLast;
    }

    setNeighborNull(first, last);
    setNeighborNull(last, first);

    pNode->degree = degree;

    PCArc *newArc = createArc();
    PCArc *twin = createArc();
    newArc->twin = twin;
    newArc->setyNode(pNode, timestamp);
    twin->twin = newArc;

    // If the original node was the apex and the new node holds the parent arc, make the new node the parent of the
    // central C-node
    if (isParent) {
        newArc->yParent = true;
        twin->yParent = false;
    } else {
        newArc->yParent = false;
        twin->yParent = true;
        pNode->parentArc = twin;
    }


    combineLists(newArc, last);
    combineLists(newArc, first);

    return twin;
}

void PCTree::deleteDegreeTwoNode(PCArc *incoming1) {
    // Delete a node of degree two by making the arcs of the two neighbors point directly to each other.
    PCArc *incoming2 = incoming1->neighbor1;
    PCArc *incoming1TwinNeighbor1 = incoming1->twin->neighbor1;
    PCArc *incoming1TwinNeighbor2 = incoming1->twin->neighbor2;
    PCArc *incoming2TwinNeighbor1 = incoming2->twin->neighbor1;
    PCArc *incoming2TwinNeighbor2 = incoming2->twin->neighbor2;

    incoming1->neighbor1 = nullptr;
    incoming1->neighbor2 = nullptr;
    incoming2->neighbor1 = nullptr;
    incoming2->neighbor2 = nullptr;

    if (incoming1TwinNeighbor1 == incoming1->twin) {
        incoming2->neighbor1 = incoming2;
        incoming2->neighbor2 = incoming2;
    }

    if (incoming2TwinNeighbor1 == incoming2->twin) {
        incoming1->neighbor1 = incoming1;
        incoming1->neighbor2 = incoming1;
    }

    setNeighborNull(incoming1TwinNeighbor1, incoming1->twin);
    setNeighborNull(incoming1TwinNeighbor2, incoming1->twin);
    setNeighborNull(incoming2TwinNeighbor1, incoming2->twin);
    setNeighborNull(incoming2TwinNeighbor2, incoming2->twin);

    combineLists(incoming2, incoming1TwinNeighbor1);
    combineLists(incoming2, incoming1TwinNeighbor2);
    combineLists(incoming1, incoming2TwinNeighbor1);
    combineLists(incoming1, incoming2TwinNeighbor2);

    incoming1->setyNode(incoming2->twin->getyNode(timestamp), timestamp);
    incoming2->setyNode(incoming1->twin->getyNode(timestamp), timestamp);

    if (incoming1->yParent && incoming2->yParent) {
        // Deleting root node, declare a non-leaf node as new parent.
        if (incoming1->twin->getyNode(timestamp) == nullptr ||
            incoming1->twin->getyNode(timestamp)->nodeType != PCNodeType::Leaf) {
            incoming2->yParent = true;
            incoming1->yParent = false;
            if (incoming1->twin->getyNode(timestamp) != nullptr) {
                incoming1->twin->getyNode(timestamp)->parentArc = nullptr;
            }
        } else {
            incoming2->yParent = false;
            incoming1->yParent = true;
            if (incoming2->twin->getyNode(timestamp) != nullptr) {
                incoming2->twin->getyNode(timestamp)->parentArc = nullptr;
            }
        }

    }

    delete incoming1->twin;
    delete incoming2->twin;

    incoming1->twin = incoming2;
    incoming2->twin = incoming1;
}

void PCTree::splitCNode(PCNode *node) {
    addToCentralCNode(node->emptyBlockFirst, node->emptyBlockLast, false);
    addToCentralCNode(node->fullBlockFirst, node->fullBlockLast, true);
}

void PCTree::addToCentralCNode(PCArc *firstArc, PCArc *lastArc, bool full) {
    if (firstArc == nullptr || lastArc == nullptr) {
        return;
    }

    // Add a list of arcs to the appropriate side of the central C-node
    if (full) {
        if (centralCNode->fullBlockFirst == nullptr) {
            centralCNode->fullBlockFirst = firstArc;
        } else {
            combineLists(firstArc, centralCNode->fullBlockLast);
        }

        centralCNode->fullBlockLast = lastArc;
    } else {
        if (centralCNode->emptyBlockFirst == nullptr) {
            centralCNode->emptyBlockFirst = firstArc;
        } else {
            combineLists(firstArc, centralCNode->emptyBlockLast);
        }

        centralCNode->emptyBlockLast = lastArc;
    }
}

bool PCTree::isFullArc(PCArc *arc) const {
    return arc != nullptr && arc->twin->getyNode(timestamp) != nullptr &&
           arc->twin->getyNode(timestamp)->getLabel(timestamp) == NodeLabel::Full;
}
