#include "sgcl/sgcl.h"

#include <chrono>
#include <iostream>

class Treap {
    struct Node;
    using NodePtr = sgcl::tracked_ptr<Node>;

    struct Node {
        Node(int x): x(x) {}
        const int x;
        const int y = rand();
        NodePtr left;
        NodePtr right;
    };

public:
    void insert(int x) {
        auto [lower, equal, greater] = _split(x);
        if (!equal) {
            equal = sgcl::make_tracked<Node>(x);
        }
        _root = _merge(lower, equal, greater);
    }

    void erase(int x) {
        auto [lower, equal, greater] = _split(x);
        _root = _merge(lower, greater);
    }

    bool has_value(int x) {
        auto [lower, equal, greater] = _split(x);
        _root = _merge(lower, equal, greater);
        return equal != nullptr;
    }

private:
    static NodePtr& _merge(NodePtr& lower, NodePtr& greater) {
        if (!lower) {
            return greater;
        }
        if (!greater) {
            return lower;
        }
        if (lower->y < greater->y) {
            lower->right = _merge(lower->right, greater);
            return lower;
        }
        else {
            greater->left = _merge(lower, greater->left);
            return greater;
        }
    }

    static NodePtr& _merge(NodePtr& lower, NodePtr& equal, NodePtr& greater) {
        return _merge(_merge(lower, equal), greater);
    }

    std::array<NodePtr, 2> _split(NodePtr orig, int value) const {
        if (!orig)
            return {nullptr, nullptr};
        if (orig->x < value) {
            auto [less, greater] = _split(orig->right, value);
            orig->right = less;
            return {orig, greater};
        } else {
            auto [less, greater] = _split(orig->left, value);
            orig->left = greater;
            return {less, orig};
        }
    }

    std::array<NodePtr, 3> _split(int value) const {
        auto [less, greater_or_equal] = _split(_root, value);
        auto [equal, greater] = _split(greater_or_equal, value + 1);
        return {less, equal, greater};
    }

    NodePtr _root;
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    Treap treap;
    int value = 5;
    int result = 0;

    for(int i = 1; i < 1000000; ++i) {
        value = (value * 57 + 43) % 10007;
        switch(i % 3) {
        case 0:
            treap.insert(value);
            break;
        case 1:
            treap.erase(value);
            break;
        case 2:
            result += treap.has_value(value);
            break;
        }
    }

    std::cout << result << std::endl;
    std::cout << duration<double, std::milli>(high_resolution_clock::now() - t).count() << "ms\n";
}
