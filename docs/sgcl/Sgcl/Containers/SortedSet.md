# Sgcl::SortedSet

```cpp
#include "sgcl/Sgcl/Containers/SortedSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Compare = std::less<T>>
    class SortedSet;
}
```

The same class in the `sgcl` interface: [set](../../containers/set.md).

`SortedSet<T, Compare>` is `std::set` on a red-black tree whose nodes are managed objects: the same tree as [SortedDictionary](SortedDictionary.md), holding values alone. The interface has the constructors, `Add` (unless the value is there: `bool`), `Emplace`, `Remove`, `RemoveAt`, `RemoveAll`, the lookups with a transparent comparator (`Contains`, `FindEntry`), `First`/`Last`, `LowerBound`/`UpperBound`/`EqualRange`, bidirectional iterators through the free `begin`/`end`, `KeyCompare`, `Swap`, `==` and `<=>`. The behaviour is `std::set`'s: unique values in `Compare` order, elements that cannot be modified through an iterator (the iterators yield `const T&`), an element destroyed the moment it is removed.

What differs from `std` is where the memory lives: as for [SortedDictionary](SortedDictionary.md), the set holds one `Ptr` (to a header node), a count and the comparator, so it lives where a `Ptr` may; the nodes are managed objects linked by tracked pointers and traced from the header, so a `SortedSet<Ptr<T>>` is a set of traced pointers (ordered by address, or by a comparator that looks through them) and a cycle through a set is collected like any other; nothing is freed by hand; iterators are one raw node pointer each, valid while their element is in the set; lookups and iteration pay no write barrier. The header is allocated on the first insertion: an empty set costs nothing.

## Rules

The rules of [SortedDictionary](SortedDictionary.md#rules), and: an element cannot be modified through an iterator; remove it and add the changed one.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::set<T, Compare>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // bidirectional, one raw node pointer, yields const T&
using ConstIterator = InnerType::const_iterator;    // the same type
```

### Constructors, destructor, operator=

```cpp
SortedSet();
explicit SortedSet(const Compare& cmp);
template<std::input_iterator It> SortedSet(It first, It last);
SortedSet(std::initializer_list<T> il);
explicit SortedSet(InnerType c) noexcept;
SortedSet(const SortedSet& other);
SortedSet(SortedSet&& other) noexcept;
~SortedSet();
SortedSet& operator=(const SortedSet& other);
SortedSet& operator=(SortedSet&& other) noexcept;
```

As for [SortedDictionary](SortedDictionary.md#constructors): the default constructor allocates nothing, a range inserts in order and keeps the first of equal values, a copy has nodes of its own, a move takes the tree over.

```cpp
SortedSet<String> names = {"bob", "ann"};                   // ann bob
SortedSet<int, std::greater<int>> desc = {1, 3, 2};              // 3 2 1
List src = {2, 1, 2};
SortedSet<int> fromRange(begin(src), end(src));                // 1 2
```

### begin, end, First, Last

```cpp
Iterator begin(SortedSet&) noexcept;    ConstIterator begin(const SortedSet&) noexcept;   // free functions
Iterator end(SortedSet&) noexcept;      ConstIterator end(const SortedSet&) noexcept;
const T& First() const noexcept;        // the least
const T& Last() const noexcept;         // the greatest
```

`begin(s)` is the least value, in O(1); `end(s)` the header. Before the first insertion both are null iterators, equal to each other. An iterator is one raw node pointer and may be kept in unmanaged memory for as long as its element is in the set.

```cpp
SortedSet s = {3, 1, 2};
for (int v : s) { /* 1 2 3 */ }
assert(s.First() == 1 && s.Last() == 3);
```

### IsEmpty, Count, Clear, KeyCompare

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
void Clear() noexcept;
Compare KeyCompare() const;
```

### Add, Emplace

```cpp
bool Add(const T& value);
bool Add(T&& value);
template<class... A> bool Emplace(A&&... a);
```

`Add` looks the value up and adds it when it is new, saying so; `Emplace` builds the value from `a...` in a new node first and destroys it again on a duplicate. O(log n).

```cpp
SortedSet<String> s;
bool added = s.Add("a");                              // true
added = s.Add("a");                                   // false
s.Emplace(3, 'x');                                    // "xxx"
```

### Remove, RemoveAt, RemoveRange, RemoveAll

```cpp
template<class K = T> bool Remove(const K& value);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
template<class Pred> SizeType RemoveAll(Pred pred);
```

Destroys the element at once, unlinks and rebalances (the collector reclaims the node later) and, the iterator forms, returns the iterator after it.

```cpp
SortedSet s = {1, 2, 3, 4};
s.Remove(2);
for (auto it = begin(s); it != end(s);) {
    it = *it % 2 ? s.RemoveAt(it) : std::next(it);   // 1 and 3 go
}
auto n = s.RemoveAll([](int v) { return v > 3; });   // 1; s is empty
```

### Contains, FindEntry, EqualRange, LowerBound, UpperBound

```cpp
template<class K = T> bool Contains(const K& value) const;
template<class K = T> ConstIterator FindEntry(const K& value) const noexcept;
template<class K> Iterator LowerBound(const K& value) noexcept;
template<class K> Iterator UpperBound(const K& value) noexcept;
template<class K> Range<Iterator> EqualRange(const K& value) noexcept;
// and the const forms
```

O(log n), reading raw pointers only. A `K` other than the value type looks up without building a value for a transparent comparator, which `std::less` of a [String](../Core/String.md) is.

```cpp
SortedSet<String> s = {"apple", "pear"};
bool has = s.Contains("pear");     // no String is built for the literal
String line = "pear tart";
bool piece = s.Contains(line.View(0, 4));   // a view of another string, nothing built either
auto from = s.LowerBound("b");     // "pear"
```

### Swap, Comparisons, Inner

As for [SortedDictionary](SortedDictionary.md#swap): `Swap` and the free `swap`, `==` and `<=>` (element-wise, in order), `Inner()` for the node handles and `value_comp`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    String name;
    SortedSet<Ptr<Node>> peers;      // a set of traced pointers inside a managed object
};

int main() {
    // A graph whose edges are sets: each node is reachable from its peers
    Ptr a = Make<Node>("a");
    Ptr b = Make<Node>("b");
    Ptr c = Make<Node>("c");
    a->peers.Add(b);
    b->peers.Add(a);                            // a cycle
    b->peers.Add(c);
    b->peers.Add(c);                            // a duplicate: nothing is added

    // A set of values on the stack: values in order, each exactly once
    SortedSet<String> names;
    for (const auto& peer : b->peers) {         // pointers compare by address: any order
        names.Add(peer->name);
    }
    names.Add("b");
    std::cout << "b's neighbourhood:";
    for (const auto& n : names) {
        std::cout << ' ' << n;                  // a b c
    }
    std::cout << '\n';

    // Dropping the stack roots: a and b keep each other alive only through
    // their sets, which the collector sees as a cycle
    std::size_t nodeCount = names.Count();
    a = b = c = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << Collector::LiveObjectCount() << " live objects\n";     // the header and three nodes of `names`
    return nodeCount == 3 ? 0 : 1;
}
```

The output:

```
b's neighbourhood: a b c
7 live objects
```

## See also

- [SortedMultiSet](SortedMultiSet.md) for equal values, [SortedDictionary](SortedDictionary.md) and [SortedMultiDictionary](SortedMultiDictionary.md) for key-value pairs, [HashSet](HashSet.md) for a hash table
- [ConcurrentSortedSet](../Concurrent/ConcurrentSortedSet.md) for a sorted set shared between threads
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
