#include "sgcl/sgcl.h"

#include <iostream>
#include <optional>
#include <thread>
#include <vector>

// A graph built by several threads while the collector runs: nothing is
// counted, no thread waits for a cycle, and cycles in the graph are fine.
struct Node {
    explicit Node(int id) : id(id) {}
    ~Node() {
        // A destructor runs on a collector thread. It reads the tracked_ptr
        // members of its object only through if_alive(): a copy when the
        // target is alive, null when it is dying in the same sweep.
        if (auto p = peer.if_alive()) {
            ++p->orphaned;
        }
    }
    int id;
    int orphaned = 0;
    sgcl::vector<sgcl::tracked_ptr<Node>> edges;   // any graph, cycles included
    sgcl::tracked_ptr<Node> peer;
};

// A lock-free stack: an atomic tracked_ptr, no ABA (a node is never reused
// while a thread holds a pointer to it) and no hazard pointers to manage.
template<class T>
class Stack {
    struct Item {
        explicit Item(T v) : value(std::move(v)) {}
        T value;
        sgcl::tracked_ptr<Item> next;
    };
    sgcl::atomic<sgcl::tracked_ptr<Item>> _head;

public:
    void push(T v) {
        sgcl::tracked_ptr item = sgcl::make_tracked<Item>(std::move(v));
        item->next = _head.load();
        while (!_head.compare_exchange_weak(item->next, item)) {}
    }
    std::optional<T> pop() {
        auto item = _head.load();
        while (item && !_head.compare_exchange_weak(item, item->next)) {}
        if (item) {
            return std::move(item->value);
        }
        return std::nullopt;
    }
};

int main() {
    Stack<sgcl::tracked_ptr<Node>> ready;
    sgcl::vector<sgcl::thread> workers;
    for (int t : sgcl::range(4)) {
        // A thread registers itself the first time it copies a managed pointer
        workers.emplace_back([&ready, t] {
            for (int round : sgcl::range(1000)) {
                sgcl::tracked_ptr root = sgcl::make_tracked<Node>(t);
                sgcl::tracked_ptr<Node> prev = root;
                for (int i : sgcl::range(1, 100)) {
                    sgcl::tracked_ptr node = sgcl::make_tracked<Node>(i);
                    node->peer = prev;
                    prev->edges.push_back(node);
                    prev = node;
                }
                prev->edges.push_back(root);   // a cycle: collected all the same
                if (round % 100 == 0) {
                    ready.push(root);          // one graph in a hundred is kept
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    // The other graphs are garbage by now, reclaimed by the collector
    // without anyone having stopped for it.
    size_t kept = 0;
    while (auto root = ready.pop()) {
        ++kept;
    }
    std::cout << kept << " graphs kept\n";
}
