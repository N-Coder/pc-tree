#include "Color.h"
#include "PQRType.h"
#include "Node.h"
#include "Leaf.h"
#include "PQRNode.h"
#include "PQRTree.h"

void cpp_zanetti::Node::setColor(Color color) {
    this->color = color;

    if (color == Color::GRAY) {
        dynamic_cast<PQRNode *>(this->getParent())->addGrayChild(this);
    }
    if (color == Color::BLACK) {
        dynamic_cast<PQRNode *>(this->getParent())->addBlackChild(this);
    }
}

void cpp_zanetti::Node::setParent(cpp_zanetti::PQRNode *parent) {
    if (parent == nullptr || parent->getType() == PQRType::P || parent->getChildCount() == 0) {
        this->parent = parent;
        this->setRepresentant(this);
    } else {
        Node *rep = parent->getRepresentativeChild();
        if (rep == this) {
            this->parent = parent;
        } else {
            this->parent = nullptr;
        }
        this->setRepresentant(rep);
    }
}

void cpp_zanetti::Node::destroy(bool remove) {
    Node *p = this->getParent();
    if (p != nullptr && remove) {
        dynamic_cast<PQRNode *>(p)->removeChild(this);
    }
    this->deleted = true;
    maybeDeleteSelf();
}

void cpp_zanetti::Node::maybeDeleteSelf() {
    OGDF_ASSERT(this->cNodeRefs >= this->cNodeRefObjs.size());
    if (!deleted) return;
    if (this->cNodeRefs <= 0) {
//        std::cout << "deleting " << this << ": ";
//        if (auto n = dynamic_cast<PQRNode *>(this)) {
//            std::cout << (n->type == PQRType::P ? "P" : "Q") << " node with " << n->childCount << " children";
//        }
//        if (auto n = dynamic_cast<Leaf *>(this)) {
//            std::cout << "Leaf " << n->value;
//        }
//        std::cout << std::endl;
        this->representant = nullptr;
        OGDF_ASSERT(this->cNodeRefs == 0);
        OGDF_ASSERT(this->cNodeRefObjs.empty());
        delete this;
    } else if (this->cNodeRefs == 1 && this->representant == this) {
        this->representant = nullptr;
    } else {
//        std::cout << "can't delete " << this << " ";
//        if (auto n = dynamic_cast<PQRNode *>(this)) {
//            std::cout << (n->type == PQRType::P ? "P" : "Q") << " node with " << n->childCount << " children";
//        }
//        if (auto n = dynamic_cast<Leaf *>(this)) {
//            std::cout << "Leaf " << n->value;
//        }
//        std::cout << " because of " << cNodeRefs << " refs, representant is " << representant << std::endl;
    }
}

void cpp_zanetti::NodeRef::assign(cpp_zanetti::Node *new_ptr) {
    if (ptr == new_ptr) return;

    if (new_ptr) {
#ifdef OGDF_DEBUG
        if (src) {
            OGDF_ASSERT(new_ptr->cNodeRefObjs.count(src) == 0);
            new_ptr->cNodeRefObjs.insert(src);
        }
#endif
        new_ptr->cNodeRefs++;
    }

    if (ptr) {
#ifdef OGDF_DEBUG
        if (src) {
            OGDF_ASSERT(ptr->cNodeRefObjs.count(src) == 1);
            ptr->cNodeRefObjs.erase(src);
        }
#endif
        ptr->cNodeRefs--;
    }

    Node *old_ptr = ptr;
    ptr = new_ptr;
    if (old_ptr)
        old_ptr->maybeDeleteSelf();
}
