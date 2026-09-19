# Sgcl::OrderedSet

```cpp
#include "sgcl/Sgcl/Containers/OrderedSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class OrderedSet;
}
```

The same class in the `sgcl` interface: [ordered_set](../../containers/ordered_set.md).

`OrderedSet<T, Hash, Equal>` is a [HashSet](HashSet.md) iterated in the order its values were added: Java's `LinkedHashSet`. The same table, every value on one more list in the order of adding, as [OrderedDictionary](OrderedDictionary.md) is to `Dictionary`: a range-for walks that list, oldest first, `First()` is the oldest value and `Last()` the newest, a copy keeps the order, `Remove` takes a value out of it, `Add` of a present value leaves it where it is, `MoveToLast` and `MoveToFirst` move a value to the end or the start, `RemoveFirst` and `RemoveLast` drop the oldest or the newest. Two words more per value.

What it is for: a set that is also a sequence without duplicates in the order things arrived: the distinct values of a stream in first-seen order, names with no repeats, the nodes of a walk in the order of the visit.

## Rules

Those of [HashSet](HashSet.md#rules); an iterator stays valid across `MoveToLast` and `MoveToFirst` (a value that is last or first already is left where it is), and `std::prev(end(s))` is the newest value.

## Members

Every member of [HashSet](HashSet.md#members), with these additions; the iterators are bidirectional and yield `const T&`.

```cpp
using InnerType = sgcl::ordered_set<T, Hash, Equal>;

const ValueType& First() const noexcept;   const ValueType& Last() const noexcept;   // the oldest, the newest (the set not empty)
bool RemoveFirst();   bool RemoveLast();                                             // the oldest (the newest) removed: whether there was one
void MoveToLast(ConstIterator pos) noexcept;   void MoveToFirst(ConstIterator pos) noexcept;   // the value becomes the newest (the oldest); O(1)
template<class K = T> bool MoveToLast(const K& value) noexcept;   template<class K = T> bool MoveToFirst(const K& value) noexcept;   // by the value: whether it was there
```

`Add` and `Emplace` put a new value at the end of the order and leave a present one where it is. `Remove` takes the value out of the order; `RemoveAt` returns the next in the order; `RemoveRange` is a range of the order. A copy reproduces the order; `==` compares the contents and ignores it.

```cpp
OrderedSet<int> seen;
for (int v : {3, 1, 3, 2, 1}) {
    seen.Add(v);                              // 3 1 2: each value once, in first-seen order
}
seen.MoveToFirst(2);                          // 2 3 1
assert(seen.First() == 2 && seen.Last() == 1);
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    String name;
    List<Ptr<Node>> edges;
};

// The nodes reachable from `start`, each once, in the order the walk
// first reaches them: the set is the visited set and the report at once
OrderedSet<Ptr<Node>> Reach(const Ptr<Node>& start) {
    OrderedSet<Ptr<Node>> visited;
    List<Ptr<Node>> stack = {start};
    while (!stack.IsEmpty()) {
        Ptr node = stack.Last();
        stack.RemoveLast();
        if (visited.Add(node)) {                  // new: its edges next
            for (const auto& edge : node->edges) {
                stack.Add(edge);
            }
        }
    }
    return visited;
}

int main() {
    Ptr a = Make<Node>("a");
    Ptr b = Make<Node>("b");
    Ptr c = Make<Node>("c");
    a->edges = {b, c};
    b->edges = {a};                               // a cycle
    c->edges = {b};
    for (const auto& node : Reach(a)) {
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

- [OrderedDictionary](OrderedDictionary.md) for entries in the order of adding, [HashSet](HashSet.md) for the same set without the order, [SortedSet](SortedSet.md) for the order of the values
- [README: Containers](README.md#containers), [README: The rules](../Core/README.md#the-rules)
