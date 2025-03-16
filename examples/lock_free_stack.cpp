#include "sgcl/sgcl.h"

#include <chrono>
#include <future>
#include <iostream>

template<typename T>
class LockFreeStack {
    struct Node;
    using NodePtr = sgcl::tracked_ptr<Node>;

    struct Node {
        T data;
        NodePtr next;
    };    
    sgcl::atomic<NodePtr> _head;

public:
    LockFreeStack() = default;
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator= (const LockFreeStack&) = delete;

    void push(T data) {
        NodePtr node = sgcl::make_tracked<Node>(std::move(data));
        node->next = _head.load();
        while(!_head.compare_exchange_weak(node->next, node));
        _head.notify_one();
    }

    std::optional<T> try_pop() {
        auto node = _head.load();
        while(node && !_head.compare_exchange_weak(node, node->next));
        if (node) {
            return std::move(node->data);
        }
        return {};
    }

    T pop() {
        for(;;) {
            auto result = try_pop();
            if (result.has_value()) {
                return std::move(result.value());
            }
            _head.wait(nullptr);
        }
    }
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    LockFreeStack<int> stack;

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
