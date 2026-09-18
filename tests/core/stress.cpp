//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Stress tests: many threads mutating cyclic graphs while the collector runs.
// Scale with SGCL_STRESS_SCALE (default 1); run under ASan/TSan via
// -DSGCL_SANITIZER=address|thread.
#include "tests/types.h"

#include <atomic>
#include <cstdlib>
#include <random>
#include <thread>

namespace {
    int stress_scale() {
        if (auto s = std::getenv("SGCL_STRESS_SCALE")) {
            return std::max(1, std::atoi(s));
        }
        return 1;
    }

    constexpr int MagicAlive = 0x5A5A1234;
    constexpr int MagicDead = 0x0DEAD0DE;

    // Immutable after construction: links are fixed, so a premature free and
    // slot reuse shows up as an id mismatch between parent and child.
    struct Node {
        Node(int id, tracked_ptr<Node> l, tracked_ptr<Node> r)
        : id(id)
        , left_id(l ? l->id : -1)
        , right_id(r ? r->id : -1)
        , left(std::move(l))
        , right(std::move(r)) {
        }

        ~Node() {
            magic = MagicDead;
        }

        int magic = MagicAlive;
        int id;
        int left_id;
        int right_id;
        tracked_ptr<Node> left;
        tracked_ptr<Node> right;
    };

    constexpr int RootCount = 64;
    constexpr int SharedCount = 16;

    struct Roots {
        tracked_ptr<Node> slot[RootCount];
    };

    struct Shared {
        atomic<tracked_ptr<Node>> slot[SharedCount];
    };

    void verify(const tracked_ptr<Node>& start, int max_depth) {
        auto n = start;
        for (int d = 0; n && d < max_depth; ++d) {
            ASSERT_EQ(n->magic, MagicAlive) << "node " << n->id << " used after destruction";
            auto l = n->left;
            auto r = n->right;
            if (l) {
                ASSERT_EQ(l->id, n->left_id) << "left link of " << n->id << " points to a reused slot";
            }
            if (r) {
                ASSERT_EQ(r->id, n->right_id) << "right link of " << n->id << " points to a reused slot";
            }
            n = (d & 1) ? l : r;
            if (!n) {
                n = (d & 1) ? r : l;
            }
        }
    }

    void churn(Roots& roots, Shared& shared, int thread_index, int iterations, sgcl::atomic<int>& next_id) {
        std::mt19937 rng(1234 + thread_index);
        std::uniform_int_distribution<int> pick_root(0, RootCount - 1);
        std::uniform_int_distribution<int> pick_shared(0, SharedCount - 1);
        std::uniform_int_distribution<int> action(0, 99);
        for (int i = 0; i < iterations; ++i) {
            int a = action(rng);
            if (a < 45) {
                // new node with random (possibly cyclic) links
                auto l = roots.slot[pick_root(rng)];
                auto r = (a & 1) ? shared.slot[pick_shared(rng)].load() : roots.slot[pick_root(rng)];
                auto node = make_tracked<Node>(next_id.fetch_add(1, std::memory_order_relaxed), l, r);
                roots.slot[pick_root(rng)] = std::move(node);
            } else if (a < 60) {
                roots.slot[pick_root(rng)] = nullptr;
            } else if (a < 70) {
                shared.slot[pick_shared(rng)].store(roots.slot[pick_root(rng)]);
            } else if (a < 80) {
                auto expected = shared.slot[pick_shared(rng)].load();
                auto desired = roots.slot[pick_root(rng)];
                shared.slot[pick_shared(rng)].compare_exchange_weak(expected, desired);
                roots.slot[pick_root(rng)] = expected;
            } else if (a < 97) {
                verify(roots.slot[pick_root(rng)], 32);
                if (::testing::Test::HasFatalFailure()) {
                    return;
                }
            } else {
                collector::force_collect();
            }
        }
    }
}

TEST(Stress_Tests, CyclicGraphChurn) {
    const int threads = 4;
    const int iterations = 20000 * stress_scale();
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::atomic<int> next_id = {0};
        tracked_ptr<Roots> roots[threads];
        tracked_ptr<Shared> shared = make_tracked<Shared>();
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t) {
            roots[t] = make_tracked<Roots>();
            workers.emplace_back([&, t] {
                churn(*roots[t], *shared, t, iterations, next_id);
            });
        }
        for (auto& w : workers) {
            w.join();
        }
        ASSERT_FALSE(::testing::Test::HasFatalFailure());
        // everything still reachable must be intact after a full cycle
        collector::force_collect(true);
        for (int t = 0; t < threads; ++t) {
            for (auto& s : roots[t]->slot) {
                verify(s, 64);
            }
        }
        for (auto& s : shared->slot) {
            verify(s.load(), 64);
        }
    });
    const size_t after = collector::get_live_object_count();
    EXPECT_EQ(after, before) << "graph not fully reclaimed after roots were dropped";
}

namespace {
    struct ListNode {
        ListNode(int id) : id(id) {}
        ~ListNode() { magic = MagicDead; }
        int magic = MagicAlive;
        int id;
        list<tracked_ptr<ListNode>> children;
        vector<tracked_ptr<ListNode>> refs;
    };
}

TEST(Stress_Tests, ContainerGraphChurn) {
    const int threads = 4;
    const int iterations = 4000 * stress_scale();
    const size_t before = collector::get_live_object_count();
    {
        std::vector<std::thread> workers;
        sgcl::atomic<bool> failed = {false};
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&, t] {
                std::mt19937 rng(99 + t);
                std::uniform_int_distribution<int> action(0, 99);
                tracked_ptr<ListNode> head = make_tracked<ListNode>(0);
                int next = 1;
                for (int i = 0; i < iterations && !failed; ++i) {
                    int a = action(rng);
                    if (a < 40) {
                        tracked_ptr<ListNode> n = make_tracked<ListNode>(next++);
                        n->children.push_back(head);          // cycle back to head
                        n->refs.push_back(n);                 // self cycle
                        head->children.push_back(std::move(n));
                    } else if (a < 60 && !head->children.empty()) {
                        head->children.pop_front();
                    } else if (a < 75 && !head->children.empty()) {
                        head->refs.push_back(head->children.back());
                    } else if (a < 85 && !head->refs.empty()) {
                        head->refs.pop_back();
                    } else if (a < 97) {
                        for (auto& c : head->children) {
                            if (c->magic != MagicAlive || c->children.front()->id != head->id) {
                                failed = true;
                                break;
                            }
                        }
                    } else {
                        head = make_tracked<ListNode>(next++);   // drop the whole graph
                        collector::force_collect();
                    }
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
        ASSERT_FALSE(failed.load()) << "container graph node used after destruction";
    }
    const size_t after = collector::get_live_object_count();
    EXPECT_EQ(after, before);
}

namespace {
    // Treiber stack on sgcl::atomic, as in examples/lock_free_stack.cpp
    struct StackNode {
        StackNode(int64_t v) : value(v) {}
        int64_t value;
        tracked_ptr<StackNode> next;
    };

    struct Stack {
        atomic<tracked_ptr<StackNode>> head;

        void push(int64_t v) {
            tracked_ptr<StackNode> n = make_tracked<StackNode>(v);
            n->next = head.load();
            while (!head.compare_exchange_weak(n->next, n)) {
            }
        }

        bool try_pop(int64_t& v) {
            auto n = head.load();
            while (n && !head.compare_exchange_weak(n, n->next)) {
            }
            if (n) {
                v = n->value;
                return true;
            }
            return false;
        }
    };
}

TEST(Stress_Tests, LockFreeStackProducersConsumers) {
    const int producers = 3;
    const int consumers = 3;
    const int per_producer = 30000 * stress_scale();
    const size_t before = collector::get_live_object_count();
    {
        tracked_ptr<Stack> stack = make_tracked<Stack>();
        sgcl::atomic<int64_t> pushed = {0};
        sgcl::atomic<int64_t> popped = {0};
        sgcl::atomic<int> pushed_count = {0};
        sgcl::atomic<int> popped_count = {0};
        sgcl::atomic<bool> producing = {true};
        std::vector<std::thread> workers;
        for (int p = 0; p < producers; ++p) {
            workers.emplace_back([&, p] {
                int64_t sum = 0;
                for (int i = 0; i < per_producer; ++i) {
                    int64_t v = int64_t(p) * per_producer + i + 1;
                    stack->push(v);
                    sum += v;
                }
                pushed.fetch_add(sum);
                pushed_count.fetch_add(per_producer);
            });
        }
        for (int c = 0; c < consumers; ++c) {
            workers.emplace_back([&] {
                int64_t sum = 0;
                int count = 0;
                int64_t v;
                for (;;) {
                    if (stack->try_pop(v)) {
                        sum += v;
                        ++count;
                    } else if (!producing.load()) {
                        if (!stack->try_pop(v)) {
                            break;
                        }
                        sum += v;
                        ++count;
                    } else {
                        std::this_thread::yield();
                    }
                }
                popped.fetch_add(sum);
                popped_count.fetch_add(count);
            });
        }
        std::thread gc([&] {
            while (producing.load()) {
                collector::force_collect();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        for (int p = 0; p < producers; ++p) {
            workers[p].join();
        }
        producing = false;
        for (int c = 0; c < consumers; ++c) {
            workers[producers + c].join();
        }
        gc.join();
        EXPECT_EQ(pushed_count.load(), popped_count.load());
        EXPECT_EQ(pushed.load(), popped.load());
    }
    const size_t after = collector::get_live_object_count();
    EXPECT_EQ(after, before);
}

// Thread registration and deletion: mutators push their Data and pages, the
// collector takes the lists with exchange and deletes the Data of exited
// threads. Many short-lived threads, some overlapping, with forced cycles
// in between.
TEST(Stress_Tests, ThreadChurn) {
    const int rounds = 30 * stress_scale();
    const size_t before = collector::get_live_object_count();
    {
        tracked_ptr<Roots> keep = make_tracked<Roots>();
        sgcl::atomic<int> next_id = {0};
        for (int r = 0; r < rounds; ++r) {
            std::vector<std::thread> workers;
            for (int t = 0; t < 8; ++t) {
                workers.emplace_back([&, t] {
                    tracked_ptr<Node> last;
                    for (int i = 0; i < 50; ++i) {
                        last = make_tracked<Node>(next_id.fetch_add(1), last, nullptr);
                    }
                    keep->slot[t] = last;          // survives the thread
                    if (t == 0) {
                        collector::force_collect();
                    }
                });
            }
            for (auto& w : workers) {
                w.join();
            }
            if (r % 5 == 4) {
                collector::force_collect(true);
                for (int t = 0; t < 8; ++t) {
                    verify(keep->slot[t], 60);
                }
            }
        }
    }
    EXPECT_EQ(collector::get_live_object_count(), before);
}
