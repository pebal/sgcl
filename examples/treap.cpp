#include "sgcl/sgcl.h"

#include <chrono>
#include <iostream>

using namespace sgcl;

class treap {
    struct node;
    using Root = root_ptr<node>;
    using Ptr = tracked_ptr<node>;

    struct node {
        node(int v): value(v) {}
        const int value;
        const int priority = rand();
        Ptr left;
        Ptr right;
    };

    inline static auto _make = [](int value) {
        return make_tracked<node>(value);
    };

public:
    void insert(int value) {
        auto [lower, equal, greater] = _split(value);
        if (!equal) {
            equal = _make(value);
        }
        _root = _merge(lower, equal, greater);
    }

    void erase(int value) {
        auto [lower, equal, greater] = _split(value);
        _root = _merge(lower, greater);
    }

    bool has_value(int value) {
        auto [lower, equal, greater] = _split(value);
        _root = _merge(lower, equal, greater);
        return equal != nullptr;
    }

private:
    static const Ptr& _merge(const Ptr& lower, const Ptr& greater) {
        if (!lower) {
            return greater;
        }
        if (!greater) {
            return lower;
        }
        if (lower->priority < greater->priority) {
            lower->right = _merge(lower->right, greater);
            return lower;
        }
        else {
            greater->left = _merge(lower, greater->left);
            return greater;
        }
    }

    static const Ptr& _merge(const Ptr& lower, const Ptr& equal, const Ptr& greater) {
        return _merge(_merge(lower, equal), greater);
    }

    std::array<Root, 3> _split(int value) const {
        auto split = [](auto&& self, const Ptr& orig, Ptr& lower, Ptr& greater, int value) -> void {
            if (!orig) {
                lower = nullptr;
                greater = nullptr;
            }
            else if (orig->value < value) {
                lower = orig;
                self(self, lower->right, lower->right, greater, value);
            }
            else {
                greater = orig;
                self(self, greater->left, lower, greater->left, value);
            }
        };
        Root lower, equal, equal_or_greater, greater;
        split(split, _root, lower, equal_or_greater, value);
        split(split, equal_or_greater, equal, greater, value + 1);
        return {lower, equal, greater};
    }

    Root _root;
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;

    auto t = high_resolution_clock::now();
    treap treap;
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
