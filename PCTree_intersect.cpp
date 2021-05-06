#include "PCTree.h"
#include "PCNode.h"
#include "PCArc.h"

using namespace pc_tree::hsu;

bool PCTree::intersect(PCTree &other, PCTreeNodeArray<PCNode *> &mapping) {
    OGDF_HEAVY_ASSERT(isValid() && other.isValid());
    OGDF_ASSERT(leaves.size() == other.leaves.size());
    OGDF_ASSERT(mapping.registeredAt() == &other.nodeArrayRegistry);
    if (other.isTrivial()) {
        return true;
    }

    auto oldLeaves = leaves;
    bool possible = other.findNodeRestrictions(*this, mapping);
    restoreSubtrees(oldLeaves);
    OGDF_ASSERT(leaves.size() == oldLeaves.size());
    OGDF_HEAVY_ASSERT(isValid());

    return possible;
}

bool PCTree::findNodeRestrictions(PCTree &applyTo, PCTreeNodeArray<PCNode *> &mapping) {
    std::vector<PCArc *> informedArcOrder;
    std::vector<PCNode *> labelLeaves(leaves.begin(), --leaves.end());
    timestamp++;

    // Label all leaves except one in order to obtain the order in which inner nodes
    // become full (Stores the informed arc for all inner nodes).
    assignLabels(labelLeaves, &informedArcOrder);

    for (PCArc *arc : informedArcOrder) {
        PCArc *startArc = arc->twin;
        std::vector<PCNode *> consecutiveOriginal;
        std::vector<PCNode *> consecutiveOther;

        PCArc *current = startArc->neighbor1;
        PCArc *previous = startArc;
        do {
            if (current->twin->getyNode(timestamp) != nullptr &&
                current->twin->getyNode(timestamp)->nodeType == PCNodeType::Leaf) {
                consecutiveOther.push_back(current->twin->getyNode(timestamp));
                consecutiveOriginal.push_back(mapping[current->twin->getyNode(timestamp)]);
            }

            // If the current node is a C-node, make its adjacent leaves pairwise consecutive in applyTo.
            if ((startArc->getyNode(timestamp) == nullptr ||
                 startArc->getyNode(timestamp)->nodeType == PCNodeType::CNode) && consecutiveOriginal.size() >= 2) {
                std::vector<PCNode *> pair(-- --consecutiveOriginal.end(), consecutiveOriginal.end());
                if (!applyTo.makeConsecutive(pair)) {
                    return false;
                }
            }

            getNextArc(previous, current);
        } while (current != startArc);

        PCNode *merged = mergeLeaves(consecutiveOther);

        // Make the leaves consecutive in applyTo and replace the subtree with a new leaf.
        PCArc *fullBlockFirst = nullptr;
        PCArc *fullBlockLast = nullptr;
        PCArc *emptyBlockFirst = nullptr;
        PCArc *emptyBlockLast = nullptr;
        if (!applyTo.makeConsecutive(consecutiveOriginal, &fullBlockFirst, &fullBlockLast, &emptyBlockFirst,
                                     &emptyBlockLast)) {
            return false;
        }
        setNeighborNull(fullBlockFirst, emptyBlockFirst);
        setNeighborNull(emptyBlockFirst, fullBlockFirst);
        setNeighborNull(emptyBlockLast, fullBlockLast);
        setNeighborNull(fullBlockLast, emptyBlockLast);

        PCNode *newLeaf = applyTo.insertNode(PCNodeType::Leaf, emptyBlockFirst, emptyBlockLast);

        PCNode *newLeafParent = newLeaf->parentArc->getyNode(applyTo.timestamp);
        if (newLeafParent != nullptr && newLeafParent->nodeType == PCNodeType::PNode) {
            newLeafParent->degree--;
            if (newLeafParent->parentArc == fullBlockFirst->twin) {
                // If the merged subtree contains the root node, declare this P-node as new root
                // node but remember its old parent arc.
                newLeafParent->oldParentArc = newLeafParent->parentArc;
                newLeafParent->parentArc = nullptr;
            }
        }

        for (PCNode *n : consecutiveOriginal) {
            applyTo.removeLeafFromList(n);
        }


        // Delete parent node if it has degree two, unless it's the only remaining inner node.
        PCArc *parentArc = newLeaf->parentArc;
        if (parentArc->neighbor1 == parentArc->neighbor2 &&
            (applyTo.leaves.size() != 2 ||
             applyTo.leaves.front()->parentArc->neighbor1 != applyTo.leaves.back()->parentArc)) {
            applyTo.deleteDegreeTwoNode(parentArc);
        }
        // Store the merged block in the leaf so we can restore it later.
        newLeaf->parentArc->blockStart = fullBlockFirst;
        newLeaf->parentArc->blockEnd = fullBlockLast;
        mapping[merged] = newLeaf;
    }

    // If the last remaining node is a C-node, make its leaves pairwise consecutive.
    PCArc *lastNodeEntryArc = leaves.front()->parentArc;
    if (lastNodeEntryArc->getyNode(timestamp) == nullptr ||
        lastNodeEntryArc->getyNode(timestamp)->nodeType == PCNodeType::CNode) {
        std::vector<PCNode *> consecutiveOriginal;
        PCArc *current = lastNodeEntryArc->neighbor1;
        PCArc *previous = lastNodeEntryArc;
        do {
            consecutiveOriginal.push_back(mapping[current->twin->getyNode(timestamp)]);
            if (consecutiveOriginal.size() >= 2) {
                std::vector<PCNode *> pair(-- --consecutiveOriginal.end(), consecutiveOriginal.end());
                if (!applyTo.makeConsecutive(pair)) {
                    return false;
                }
            }
            getNextArc(previous, current);
        } while (current != lastNodeEntryArc);
    }

    return true;
}

void PCTree::restoreSubtrees(std::vector<PCNode *> &oldLeaves) {
    std::queue<PCArc *> queue;
    queue.push(leaves.front()->parentArc);

    while (!queue.empty()) {
        PCArc *startArc = queue.front();
        queue.pop();

        PCArc *previous = startArc;
        PCArc *current = startArc->neighbor1;
        while (current != startArc) {
            if (current->blockStart != nullptr) {
                // Replace the merged leaf with the subtree it represents.
                PCArc *neighbor1 = current->neighbor1;
                PCArc *neighbor2 = current->neighbor2;
                setNeighborNull(neighbor1, current);
                setNeighborNull(neighbor2, current);
                combineLists(neighbor1, current->blockStart);
                combineLists(neighbor2, current->blockEnd);

                PCNode *currentNode = current->getyNode(timestamp);
                if (currentNode != nullptr && currentNode->nodeType == PCNodeType::PNode && currentNode->oldParentArc != nullptr) {
                    currentNode->parentArc = currentNode->oldParentArc;
                    currentNode->oldParentArc = nullptr;
                }

                if (current->blockStart->getYNodeType() == PCNodeType::PNode) {
                    OGDF_ASSERT(current->blockStart == current->blockEnd);
                    current->blockStart->setyNode(current->getYNode(), timestamp);
                }

                PCArc *oldCurrent = current;
                if (neighbor1 == previous) {
                    current = current->blockStart;
                } else {
                    current = current->blockEnd;
                }

                delete oldCurrent->twin;
                delete oldCurrent;
                continue;
            } else if (current->twin->getyNode(timestamp) == nullptr ||
                       current->twin->getyNode(timestamp)->nodeType != PCNodeType::Leaf) {
                queue.push(current->twin);
                getNextArc(previous, current);
            } else {
                getNextArc(previous, current);
            }
        }
    }
    leaves = oldLeaves;
}