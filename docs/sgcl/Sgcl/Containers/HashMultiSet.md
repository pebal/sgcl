# Sgcl::HashMultiSet

```cpp
#include "sgcl/Sgcl/Containers/HashSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class HashMultiSet;
}
```

The same class in the `sgcl` interface: [unordered_multiset](../../containers/unordered_multiset.md).

`HashMultiSet<T, Hash, Equal>` is `std::unordered_multiset` over managed nodes: the same hash table as [HashSet](HashSet.md), with equal values allowed. The interface is that of the hash set with `Add` and `Emplace` that always add, `Values(value)` for the run of equal ones, `CountOf(value)` for its length, `Remove(value)` that removes the whole run; the lookups with transparent hash and equality, the free `begin`/`end`, the hash policy, `HashFunction`/`KeyEqual`, `Swap`, `==`, `RemoveAll`. The behaviour is `std::unordered_multiset`'s: equal elements are adjacent in the iteration order and in their bucket, an element is destroyed the moment it is removed. Within a run of equal elements a new one goes in front of those already there.

What differs from `std` is where the memory lives: as for [HashSet](HashSet.md), the set holds two `Ptr`s and lives where a `Ptr` may, the elements are nodes on the managed heap traced from the sentinel, nothing is freed by hand, the hash is cached in the node, iterators are one raw node pointer and yield `const T&`.

## Rules

The rules of [HashSet](HashSet.md#rules).

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::unordered_multiset<T, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, one raw node pointer, yields const T&
using ConstIterator = InnerType::const_iterator;
```

### Constructors, destructor, operator=

As for [HashSet](HashSet.md#constructors-destructor-operator), except that a range keeps every element, equal ones included.

```cpp
HashMultiSet readings = {3, 7, 3};                  // three elements
HashMultiSet<int> sized(100);                            // 128 buckets, no elements
```

### begin, end, IsEmpty, Count, Clear

As for [HashSet](HashSet.md#begin-end): the free `begin`/`end`, equal elements adjacent; the stored count; `Clear` that destroys every element and keeps the buckets.

### Add, Emplace

```cpp
Iterator Add(const T& value);
Iterator Add(T&& value);
template<class... A> Iterator Emplace(A&&... a);
```

Always adds, and returns the new element; a value already present gets the new element in front of its equals. `Emplace` builds the value from `a...` in the new node. The table grows before the node is linked when the count has reached the threshold.

```cpp
HashMultiSet<String> s;
s.Add("a");
auto it = s.Add("a");                                 // in front of the first "a"
s.Emplace(3, 'x');                                    // "xxx"
bool front = s.FindEntry("a") == it;                  // true
```

### Remove, RemoveAt, RemoveRange, RemoveAll

```cpp
template<class K = T> SizeType Remove(const K& value);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
template<class Pred> SizeType RemoveAll(Pred pred);
```

`Remove(value)` destroys every element of the run at once and returns how many; the iterator forms remove one element or a range and return the iterator after it; `RemoveAll` every element the predicate accepts.

```cpp
HashMultiSet s = {1, 1, 2};
auto n = s.Remove(1);                                  // 2
s.RemoveAt(begin(s));                                  // the 2
```

### Contains, FindEntry, Values, CountOf

```cpp
template<class K = T> bool Contains(const K& value) const;
template<class K = T> ConstIterator FindEntry(const K& value) const noexcept;   // the first of the run, end() when absent
template<class K = T> Range<ConstIterator> Values(const K& value) const;        // the run, for a range-for
template<class K = T> SizeType CountOf(const K& value) const;                   // its length: O(1 + count) on average
```

A `K` other than the value type looks up without building a value when both `Hash` and `Equal` declare `is_transparent`.

```cpp
HashMultiSet s = {3, 7, 3, 3};
auto threes = s.CountOf(3);        // 3
for (int v : s.Values(3)) {
    (void)v;                       // 3, three times
}
```

### Hash policy, HashFunction, KeyEqual, Swap, Comparisons, Inner

As for [Dictionary](Dictionary.md#hash-policy): `BucketCount`, `LoadFactor`, `MaxLoadFactor`, `Rehash`, `Reserve`, `HashFunction`, `KeyEqual`, `Swap` and the free `swap`, `==` (equal counts and, for every run of one, a run of the other of the same length), `Inner()`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Sample {
    String source;
    Ptr<Sample> previous;          // traced through the node that holds the Sample
};

int main() {
    // A bag of readings keyed by value: several samples may read the same
    HashMultiSet<int> readings;
    for (int r : {3, 7, 3, 3, 9, 7}) {
        readings.Add(r);
    }
    std::cout << "3 read " << readings.CountOf(3) << " times, 7 read " << readings.CountOf(7) << " times\n";

    // A bag of traced pointers inside a managed object: the samples live as
    // long as the bag's owner does
    struct Owner {
        HashMultiSet<Ptr<Sample>> bag;
    };
    Ptr owner = Make<Owner>();
    Ptr first = Make<Sample>("a");
    owner->bag.Add(first);
    owner->bag.Add(first);                                      // the same pointer twice: a multiset allows it
    owner->bag.Add(Make<Sample>("b", first));
    auto duplicates = owner->bag.CountOf(first);                // 2

    // Removing every copy of the pointer destroys those elements; the Sample
    // itself stays while `first` or "b" refers to it
    owner->bag.Remove(first);
    first = nullptr;
    owner = nullptr;                                            // the bag, "b" and then "a" are garbage
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << Collector::LiveObjectCount() << " live objects\n";     // the nodes, buckets and sentinel of `readings`
    return readings.CountOf(3) == 3 && duplicates == 2 ? 0 : 1;
}
```

The output:

```
3 read 3 times, 7 read 2 times
8 live objects
```

## See also

- [HashSet](HashSet.md) for unique values, [MultiDictionary](MultiDictionary.md) for key-value pairs, [SortedMultiSet](SortedMultiSet.md) for an ordered tree
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md), [Range](../Core/Range.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
