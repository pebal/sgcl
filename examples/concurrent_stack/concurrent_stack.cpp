#include "sgcl/sgcl.h"

#include <chrono>
#include <iostream>

using namespace sgcl;

template<typename T>
class concurrent_stack {
    struct Node;
    using Root = root_ptr<Node>;
    using Ptr = tracked_ptr<Node>;
    using Data = root_ptr<T>;

    struct Node {
        Node(const T& d): data(d) {}
        T data;
        Ptr next;
    };

    atomic<Root> _head;

    inline static auto _make = [](const T& data) {
        return make_tracked<Node>(data);
    };

public:
    void push(const T& data) {
        Root node = _make(data);
        node->next = _head.load();
        while (!_head.compare_exchange_weak(node->next, node));
    }

    Data pop() {
        auto node = _head.load();
        while(node && !_head.compare_exchange_weak(node, node->next));
        if (node) {
            node->next = nullptr;
            return Data(&node->data);
        }
        return nullptr;
    }
};

int main() {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    auto t = high_resolution_clock::now();

    concurrent_stack<int> stack;
    std::atomic<int64_t> result = 0;
    std::thread threads[2];

    auto test = [&]{
        int64_t sum = 0;
        for(int i = 0; i < 1000000; ++i) {
            stack.push(i);
            sum += i;
            sum -= *stack.pop();
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
