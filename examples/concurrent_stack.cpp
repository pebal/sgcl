#include "sgcl/sgcl.h"

#include <chrono>
#include <iostream>

using namespace sgcl;

template<typename T>
class ConcurrentStack {
    struct Node {
        Node(const T& d): data(d) {}
        T data;
        TrackedPtr<Node> next;
    };

    Atomic<TrackedPtr<Node>> _head;

public:
    void push(const T& data) {
        StackPtr<Node> node = make_tracked<Node>(data);
        node->next = _head.load();
        while (!_head.compare_exchange_weak(node->next, node));
    }

    StackPtr<T> pop() {
        auto node = _head.load();
        while(node && !_head.compare_exchange_weak(node, node->next));
        if (node) {
            node->next = nullptr;
            return StackPtr<T>(&node->data);
        }
        return nullptr;
    }
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    std::thread threads[2];

    auto stack = make_tracked<ConcurrentStack<int>>();
    std::atomic<int64_t> result = 0;
    auto test = [&]{
        int64_t sum = 0;
        for(int i = 0; i < 1000000; ++i) {
            stack->push(i);
            sum += i;
            sum -= *stack->pop();
        }
        result += sum;
    };

    for (auto& thread: threads) {
        std::thread(test).swap(thread);
    }
    for (auto& thread: threads) {
        thread.join();
    }

    std::cout << result << std::endl;
    std::cout << duration<double, std::milli>(high_resolution_clock::now() - t).count() << "ms\n";
}
