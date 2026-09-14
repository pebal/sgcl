//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// One of everything the formatters of sgcl.py show; check.sh builds it,
// stops at the line marked, prints the variables and looks for what the
// formatters should have said.
#include "sgcl/sgcl.h"

#include <memory>
#include <string>

struct Node {
    int v = 7;
    gc::tracked_ptr<Node> next;
};

int main() {
    gc::tracked_ptr node = gc::make_tracked<Node>();
    node->next = gc::make_tracked<Node>();
    node->next->v = 8;
    auto kept = std::make_unique<gc::tracked_ptr<Node>>(node->next);   // a cell
    sgcl::tracked_ptr<Node> plain = node;
    sgcl::unique_ptr<Node> owned = sgcl::make_tracked<Node>();
    gc::weak_ptr weak = node;
    gc::vector<int> v = {1, 2, 3};
    gc::vector<gc::tracked_ptr<Node>> vp;
    vp.push_back(node);
    vp.push_back(nullptr);
    gc::array<int> a(3);
    a[0] = 5;
    gc::deque<int> d;
    d.push_back(10);
    d.push_front(9);
    d.push_back(11);
    gc::list<std::string> l = {"a", "bb"};
    gc::forward_list<int> f = {4, 5};
    gc::map<int, std::string> m = {{1, "one"}, {2, "two"}};
    gc::set<int> s = {3, 1, 2};
    gc::unordered_map<int, int> um = {{1, 10}, {2, 20}};
    gc::unordered_set<std::string> us = {"x"};
    gc::atomic<gc::tracked_ptr<Node>> at(node);
    int sum = node->v + (*kept)->v + plain->v + owned->v + v[0] + a[0] + d[0] + (int)l.size() + *f.begin()
            + (int)m.size() + (int)s.size() + um[1] + (int)us.size() + at.load()->v;   // BREAK
    return sum == 0;
}
