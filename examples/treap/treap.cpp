#include "sgcl/sgcl.h"

#include <chrono>
#include <iostream>

using namespace sgcl;

class Treap {
    struct Node {
        const int x;
        const int y = rand();
        TrackedPtr<Node> left;
        TrackedPtr<Node> right;
    };

public:
    void insert(int x) {
        auto [lower, equal, greater] = _split(x);
        if (!equal) {
            equal = make_tracked<Node>(x);
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
    static UnsafePtr<Node> _merge(UnsafePtr<Node> lower, UnsafePtr<Node> greater) {
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

    static UnsafePtr<Node> _merge(UnsafePtr<Node> lower, UnsafePtr<Node> equal, UnsafePtr<Node> greater) {
        return _merge(_merge(lower, equal), greater);
    }

    std::array<StackPtr<Node>, 3> _split(int val) const {
        struct Local {
            static void split(UnsafePtr<Node> orig, TrackedPtr<Node>& lower, TrackedPtr<Node>& greater, int val) {
                if (!orig) {
                    lower = nullptr;
                    greater = nullptr;
                } else if (orig->x < val) {
                    lower = orig;
                    split(lower->right, lower->right, greater, val);
                } else {
                    greater = orig;
                    split(greater->left, lower, greater->left, val);
                }
            }
        };
        StackPtr<Node> lower, equal, equal_or_greater, greater;
        Local::split(_root, lower, equal_or_greater, val);
        Local::split(equal_or_greater, equal, greater, val + 1);
        return {lower, equal, greater};
    }

    TrackedPtr<Node> _root;
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    auto treap = make_tracked<Treap>();
    int value = 5;
    int result = 0;

    for(int i = 1; i < 1000000; ++i) {
        value = (value * 57 + 43) % 10007;
        switch(i % 3) {
        case 0:
            treap->insert(value);
            break;
        case 1:
            treap->erase(value);
            break;
        case 2:
            result += treap->has_value(value);
            break;
        }
    }

    std::cout << result << std::endl;
    std::cout << duration<double, std::milli>(high_resolution_clock::now() - t).count() << "ms\n";
}
