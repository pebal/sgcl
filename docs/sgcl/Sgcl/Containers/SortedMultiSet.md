# Sgcl::SortedMultiSet

```cpp
#include "sgcl/Sgcl/Containers/SortedSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Compare = std::less<T>>
    class SortedMultiSet;
}
```

The same class in the `sgcl` interface: [multiset](../../containers/multiset.md).

`SortedMultiSet<T, Compare>` is `std::multiset` on a red-black tree whose nodes are managed objects: the same tree as [SortedSet](SortedSet.md), with equivalent values allowed. The interface is that of the sorted set with `Add` and `Emplace` that always add, `Values(value)` for the run of equivalent ones, `CountOf(value)` for its length, `Remove(value)` that removes the whole run; `First`/`Last`, `LowerBound`/`UpperBound`/`EqualRange`, the lookups with a transparent comparator, the free `begin`/`end`, `KeyCompare`, `Swap`, `==` and `<=>`, `RemoveAll`. The behaviour is `std::multiset`'s: equivalent values are adjacent and a new one goes after those already there, the iterators yield `const T&`, an element is destroyed the moment it is removed.

What differs from `std` is where the memory lives: as for [SortedSet](SortedSet.md).

## Rules

The rules of [SortedSet](SortedSet.md#rules).

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::multiset<T, Compare>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // bidirectional, one raw node pointer, yields const T&
using ConstIterator = InnerType::const_iterator;
```

### Constructors, destructor, operator=

As for [SortedSet](SortedSet.md#constructors-destructor-operator), except that a range keeps every element, equivalent ones included, in their order.

```cpp
SortedMultiSet dice = {6, 1, 6, 3};                        // 1 3 6 6
SortedMultiSet<int, std::greater<int>> desc(std::greater<int>{});
```

### begin, end, First, Last, IsEmpty, Count, Clear, KeyCompare

As for [SortedSet](SortedSet.md#begin-end-first-last).

### Add, Emplace

```cpp
Iterator Add(const T& value);
Iterator Add(T&& value);
template<class... A> Iterator Emplace(A&&... a);
```

Always adds, after the elements equivalent to it, and returns the new element. O(log n).

```cpp
SortedMultiSet<String> s;
s.Add("a");
auto it = s.Add("a");                                 // after the first "a"
bool second = std::next(s.FindEntry("a")) == it;      // true
```

### Remove, RemoveAt, RemoveRange, RemoveAll

```cpp
template<class K = T> SizeType Remove(const K& value);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
template<class Pred> SizeType RemoveAll(Pred pred);
```

`Remove(value)` destroys every element of the run at once and returns how many; the iterator forms remove one element or a range; `RemoveAll` every element the predicate accepts.

### Contains, FindEntry, Values, CountOf, EqualRange, LowerBound, UpperBound

```cpp
template<class K = T> bool Contains(const K& value) const;
template<class K = T> ConstIterator FindEntry(const K& value) const noexcept;   // the first of the run
template<class K = T> Range<ConstIterator> Values(const K& value) const;        // the run: EqualRange under the set's name
template<class K = T> SizeType CountOf(const K& value) const;                   // O(log n + count)
template<class K> Range<Iterator> EqualRange(const K& value) noexcept;
template<class K> Iterator LowerBound(const K& value) noexcept;
template<class K> Iterator UpperBound(const K& value) noexcept;
// and the const forms
```

A `K` other than the value type looks up without building a value for a transparent comparator.

```cpp
SortedMultiSet s = {3, 7, 3, 3};
auto threes = s.CountOf(3);        // 3
for (int v : s.Values(3)) {
    (void)v;                       // 3, three times
}
```

### Swap, Comparisons, Inner

As for [SortedSet](SortedSet.md#swap-comparisons-inner).

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Job {
    String name;
    int priority;
    Ptr<Job> blockedBy;
};

// Jobs ordered by priority, several per level: a multiset of pointers
// with a comparator that looks through them
struct ByPriority {
    bool operator()(const Ptr<Job>& l, const Ptr<Job>& r) const {
        return l->priority < r->priority;
    }
};

int main() {
    SortedMultiSet<Ptr<Job>, ByPriority> queue;
    Ptr build = Make<Job>("build", 1);
    queue.Add(build);
    queue.Add(Make<Job>("test", 2, build));
    queue.Add(Make<Job>("lint", 2));                       // after "test": equal keys keep their order
    queue.Add(Make<Job>("deploy", 3));

    std::cout << "order:";
    for (const auto& job : queue) {
        std::cout << ' ' << job->name;                              // build test lint deploy
    }
    std::cout << '\n';

    // Everything at priority 2 goes: the two Ptrs are destroyed now,
    // "test" and "lint" are collected, "build" stays through `build`
    Ptr probe = Make<Job>("", 2);                 // a key to look up with
    auto removed = queue.Remove(probe);
    build = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << removed << " removed, " << queue.Count() << " left, "
              << Collector::LiveObjectCount() << " live objects\n";
    return removed == 2 && queue.Count() == 2 ? 0 : 1;
}
```

The output:

```
order: build test lint deploy
2 removed, 2 left, 8 live objects
```

## See also

- [SortedSet](SortedSet.md) for unique values, [SortedMultiDictionary](SortedMultiDictionary.md) for key-value pairs, [HashMultiSet](HashMultiSet.md) for a hash table
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md), [Range](../Core/Range.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
