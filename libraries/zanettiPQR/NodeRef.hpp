#pragma once
namespace cpp_zanetti {
    struct Node;
    struct PQRNode;

    struct NodeRef {
        Node *src;
        Node *ptr = nullptr;

        explicit NodeRef(Node *src = nullptr, Node *ptr = nullptr)
                : src(src) {
            assign(ptr);
        }

        virtual ~NodeRef() {
            assign(nullptr);
        }

        void assign(Node *new_ptr);

        NodeRef &operator=(Node *b) {
            assign(b);
            return *this;
        }

        operator Node *() const {
            return ptr;
        }

        Node &operator*() const {
            return *ptr;
        }

        Node *operator->() const {
            return ptr;
        }

        bool operator==(const NodeRef &rhs) const {
            return ptr == rhs.ptr;
        }

        bool operator==(Node *rhs) const {
            return ptr == rhs;
        }

        bool operator!=(const NodeRef &rhs) const {
            return ptr != rhs.ptr;
        }

        bool operator!=(Node *rhs) const {
            return ptr != rhs;
        }
    };
}
