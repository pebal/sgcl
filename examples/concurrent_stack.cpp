#include "sgcl/sgcl.h"

#include <chrono>
#include <future>
#include <iostream>

using namespace sgcl;

template<typename T, PointerPolicy P = PointerPolicy::Tracked>
class ConcurrentStack {
    struct Node {
        Node(const T& d) : data(d) {}
        T data;
        TrackedPtr<Node> next;
    };
    Pointer<Node, P> _head;

public:
    ConcurrentStack() = default;
    ConcurrentStack(const ConcurrentStack&) = delete;
    ConcurrentStack& operator= (const ConcurrentStack&) = delete;

    void push(const T& data) {
        StackPtr node = make_tracked<Node>(data);
        node->next = AtomicRef(_head).load();
        while(!AtomicRef(_head).compare_exchange_weak(node->next, node));
        AtomicRef(_head).notify_one();
    }

    std::optional<StackType<T>> try_pop() {
        StackPtr node = AtomicRef(_head).load();
        while(node && !AtomicRef(_head).compare_exchange_weak(node, node->next));
        if (node) {
            return node->data;
        }
        return {};
    }

    StackType<T> pop() {
        for(;;) {
            auto o = try_pop();
            if (o.has_value()) {
                return o.value();
            }
            AtomicRef(_head).wait(nullptr);
        }
    }
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    ConcurrentStack<int, PointerPolicy::Stack> stack;

    auto fpush = std::async([&stack]{
        int64_t sum = 0;
        for(int i = 0; i < 1000000; ++i) {
            stack.push(i);
            sum += i;
        }
        return sum;
    });

    auto fpop = std::async([&stack]{
        int64_t sum = 0;
        for(int i = 0; i < 1000000; ++i) {
            sum += stack.pop();
        }
        return sum;
    });

    std::cout << fpush.get() - fpop.get() << std::endl;
    std::cout << duration<double, std::milli>(high_resolution_clock::now() - t).count() << "ms\n";
}
