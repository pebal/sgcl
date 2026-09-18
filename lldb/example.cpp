//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
    sgcl::tracked_ptr<Node> next;
};

int main() {
    sgcl::tracked_ptr node = sgcl::make_tracked<Node>();
    node->next = sgcl::make_tracked<Node>();
    node->next->v = 8;
    auto kept = std::make_unique<sgcl::root_ptr<Node>>(node->next);   // a root in unmanaged memory: a cell
    sgcl::tracked_ptr<Node> plain = node;
    sgcl::unique_ptr<Node> owned = sgcl::make_tracked<Node>();
    sgcl::weak_ptr weak = node;
    sgcl::vector<int> v = {1, 2, 3};
    sgcl::vector<sgcl::tracked_ptr<Node>> vp;
    vp.push_back(node);
    vp.push_back(nullptr);
    sgcl::array<int> a(3);
    a[0] = 5;
    sgcl::deque<int> d;
    d.push_back(10);
    d.push_front(9);
    d.push_back(11);
    sgcl::list<std::string> l = {"a", "bb"};
    sgcl::forward_list<int> f = {4, 5};
    sgcl::map<int, std::string> m = {{1, "one"}, {2, "two"}};
    sgcl::set<int> s = {3, 1, 2};
    sgcl::unordered_map<int, int> um = {{1, 10}, {2, 20}};
    sgcl::unordered_set<std::string> us = {"x"};
    sgcl::atomic<sgcl::tracked_ptr<Node>> at(node);
    int sum = node->v + (*kept)->v + plain->v + owned->v + v[0] + a[0] + d[0] + (int)l.size() + *f.begin()
            + (int)m.size() + (int)s.size() + um[1] + (int)us.size() + at.load()->v;   // BREAK
    return sum == 0;
}
