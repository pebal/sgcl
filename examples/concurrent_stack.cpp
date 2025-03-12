#include "sgcl/sgcl.h"

#include <chrono>
#include <future>
#include <iostream>

using namespace sgcl;

template<typename T>
class ConcurrentStack {
    struct Node {
        Node(const T& d) : data(d) {}
        T data;
        TrackedPtr<Node> next;
    };    
    Atomic<TrackedPtr<Node>> _head;

public:
    ConcurrentStack() = default;
    ConcurrentStack(const ConcurrentStack&) = delete;
    ConcurrentStack& operator= (const ConcurrentStack&) = delete;

    void push(const T& data) {
        StackPtr node = make_tracked<Node>(data);
        node->next = _head.load();
        while(!_head.compare_exchange_weak(node->next, node));
        _head.notify_one();
    }

    std::optional<StackType<T>> try_pop() {
        StackPtr node = _head.load();
        while(node && !_head.compare_exchange_weak(node, node->next));
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
            _head.wait(nullptr);
        }
    }
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    auto stack = make_tracked<ConcurrentStack<int>>();

    auto fpush = std::async([&stack]{
        int64_t sum = 0;
        for(int i = 0; i < 1000000; ++i) {
            stack->push(i);
            sum += i;
        }
        return sum;
    });

    auto fpop = std::async([&stack]{
        int64_t sum = 0;
        for(int i = 0; i < 1000000; ++i) {
            sum += stack->pop();
        }
        return sum;
    });

    std::cout << fpush.get() - fpop.get() << std::endl;
    std::cout << duration<double, std::milli>(high_resolution_clock::now() - t).count() << "ms\n";
}
