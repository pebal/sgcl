# sgcl::ordered_set

```cpp
#include "sgcl/containers/ordered_set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class ordered_set;
}
```

The same class in the `Sgcl` interface: [OrderedSet](../Sgcl/Containers/OrderedSet.md).

`sgcl::ordered_set<Key, Hash, KeyEqual>` is a hash set iterated in the order its elements were inserted: Java's `LinkedHashSet`. It is [unordered_set](unordered_set.md) with every node on a second list in insertion order, as [ordered_map](ordered_map.md) is to `unordered_map`: `begin()` to `end()` walks that list both ways, `front()` is the oldest element and `back()` the newest, a copy keeps the order, an erase takes the element out of it, an insert of a present element leaves it where it is, `to_back` and `to_front` move an element to the end or the start. The table, the lookups, the bucket interface, node handles, `merge` and the transparent lookups are those of `unordered_set`; a rehash never touches the order. Two words more per node.

What it is for: a set that is also a sequence without duplicates, kept in the order things arrived: the distinct values of a stream in first-seen order, a list of names with no repeats, a set of visited nodes reported in the order of the visit; and a set with an eviction order, as the map has.

## Rules

Those of [unordered_set](unordered_set.md#rules) and, for the order, of [ordered_map](ordered_map.md#rules): an iterator stays valid across `to_back` and `to_front`, `end()` is the sentinel and `--end()` the newest element.

## Members

Every member of [unordered_set](unordered_set.md#members), with these differences and additions; the iterators are bidirectional and yield `const Key&`.

```cpp
using reverse_iterator = std::reverse_iterator<iterator>;  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

iterator begin() noexcept;  const_iterator begin() const noexcept;  const_iterator cbegin() const noexcept;    // the oldest element
iterator end() noexcept;    const_iterator end() const noexcept;    const_iterator cend() const noexcept;      // the sentinel: --end() is the newest
const_reverse_iterator rbegin() const noexcept;  const_reverse_iterator crbegin() const noexcept;
const_reverse_iterator rend() const noexcept;    const_reverse_iterator crend() const noexcept;
const value_type& front() const noexcept;   const value_type& back() const noexcept;   // the oldest, the newest (the set not empty)
void to_back(const_iterator pos) noexcept;  void to_front(const_iterator pos) noexcept;   // the element becomes the newest (the oldest); O(1), the iterator stays valid
```

`insert` and `emplace` put a new element at the end of the order and leave a present one where it is. `erase` takes the element out of the order; `erase(pos)` returns the next in the order; `erase(first, last)` is a range of the order. `extract` takes the node out of the order, and an inserted handle goes to the end. `merge` appends what it takes. A copy reproduces the order; `operator==` compares the contents and ignores it.

```cpp
sgcl::ordered_set<int> seen;
for (int v : {3, 1, 3, 2, 1}) {
    seen.insert(v);                           // 3 1 2: each value once, in first-seen order
}
seen.to_front(seen.find(2));                  // 2 3 1
assert(seen.front() == 2 && seen.back() == 1);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Node {
    sgcl::string name;
    sgcl::vector<sgcl::tracked_ptr<Node>> edges;
};

// The nodes reachable from `start`, each once, in the order the walk
// first reaches them: the set is the visited set and the report at once
sgcl::ordered_set<sgcl::tracked_ptr<Node>> reach(const sgcl::tracked_ptr<Node>& start) {
    sgcl::ordered_set<sgcl::tracked_ptr<Node>> visited;
    sgcl::vector<sgcl::tracked_ptr<Node>> stack = {start};
    while (!stack.empty()) {
        sgcl::tracked_ptr node = stack.back();
        stack.pop_back();
        if (visited.insert(node).second) {        // new: its edges next
            for (const auto& edge : node->edges) {
                stack.push_back(edge);
            }
        }
    }
    return visited;
}

int main() {
    sgcl::tracked_ptr a = sgcl::make_tracked<Node>("a");
    sgcl::tracked_ptr b = sgcl::make_tracked<Node>("b");
    sgcl::tracked_ptr c = sgcl::make_tracked<Node>("c");
    a->edges = {b, c};
    b->edges = {a};                               // a cycle
    c->edges = {b};
    for (const auto& node : reach(a)) {
        std::cout << node->name << " ";
    }
    std::cout << "\n";
    return 0;
}
```

The output:

```
a c b 
```

## See also

- [ordered_map](ordered_map.md) for key-value pairs in insertion order, [unordered_set](unordered_set.md) for the same set without the order, [set](set.md) for the order of the keys
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules)
