# Sgcl::MultiDictionary

```cpp
#include "sgcl/Sgcl/Containers/Dictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class MultiDictionary;
}
```

The same class in the `sgcl` interface: [unordered_multimap](../../containers/unordered_multimap.md).

`MultiDictionary<Key, Value, Hash, Equal>` is `std::unordered_multimap` over managed nodes: the same hash table as [Dictionary](Dictionary.md), with equivalent keys allowed. The interface is that of the dictionary without `Set` and `operator[]` (a key has any number of values), with `Add` and `Emplace` that always add, `Values(key)` for the run of one key, `CountOf(key)` for its length, `Remove(key)` that removes the whole run; the lookups with transparent hash and equality, the free `begin`/`end`, the hash policy, `HashFunction`/`KeyEqual`, `Swap`, `==`, `RemoveAll`. The behaviour is `std::unordered_multimap`'s: elements with equivalent keys are adjacent in the iteration order and in their bucket, an element is destroyed the moment it is removed. Within a run of equal keys a new element goes in front of those already there.

What differs from `std` is where the memory lives. The container holds two `Ptr`s (the bucket array and a sentinel node), the counts, the hasher and the equality, so it lives where a `Ptr` may live; the elements are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel, so elements holding `Ptr`s are traced and a cycle through the container is collected like any other. Nothing is freed by hand: a removal destroys the element and unlinks the node, the collector reclaims the node later. The hash of each key is cached in its node. The bucket count is 0 or a power of two, and the table grows when the count reaches `BucketCount() * MaxLoadFactor()`, doubling at least, to eight buckets at the least. Iterators are one raw node pointer each, trivially copyable, storable anywhere, valid across rehashes and until their element is removed. Lookups and iteration pay no write barrier; insertions, removals and rehashes store tracked pointers and pay the barrier on each link they relink ([README: Containers](../../containers/README.md#containers)).

## Rules

The rules of [Dictionary](Dictionary.md#rules): the container lives where a `Ptr` may; the elements may hold tracked pointers and are traced; an element is destroyed the moment it is removed (or, in a sweep, when the sweep reaches its node); an iterator, a reference or a pointer to an element is valid while the element is in the container; a `Ptr` may point at an element; thread safety is that of `std::unordered_multimap`.

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using PairType = std::pair<const Key, Value>;
using InnerType = sgcl::unordered_multimap<Key, Value, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, one raw node pointer
using ConstIterator = InnerType::const_iterator;
```

### Constructors, destructor, operator=

```cpp
MultiDictionary();
explicit MultiDictionary(SizeType buckets);
template<std::input_iterator It> MultiDictionary(It first, It last);
MultiDictionary(std::initializer_list<PairType> il);
explicit MultiDictionary(InnerType c) noexcept;
MultiDictionary(const MultiDictionary& other);
MultiDictionary(MultiDictionary&& other) noexcept;
~MultiDictionary();
MultiDictionary& operator=(const MultiDictionary& other);
MultiDictionary& operator=(MultiDictionary&& other) noexcept;
```

As for [Dictionary](Dictionary.md#constructors), except that a range with a key seen twice keeps every element.

```cpp
MultiDictionary<String, int> scores = {{"ann", 31}, {"ann", 27}};
MultiDictionary<int, int> sized(100);                                     // 128 buckets, no elements
List<Pair<int, int>> src = {{2, 20}, {2, 21}};
MultiDictionary<int, int> fromRange(begin(src), end(src));              // two elements under 2
```

### begin, end

```cpp
Iterator begin(MultiDictionary&) noexcept;    ConstIterator begin(const MultiDictionary&) noexcept;   // free functions
Iterator end(MultiDictionary&) noexcept;      ConstIterator end(const MultiDictionary&) noexcept;
```

Forward iterators over one chain of nodes, the elements of one key adjacent; `end(m)` is a null iterator.

```cpp
MultiDictionary<String, int> m = {{"a", 1}, {"a", 2}, {"b", 3}};
int sum = 0;
for (auto& [key, value] : m) {
    sum += value;                          // 6
}
```

### IsEmpty, Count, Clear

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
void Clear() noexcept;
```

`Count()` is a stored count. `Clear` destroys every element at once and unlinks every node; the bucket array and the policy stay.

### Add, Emplace

```cpp
Iterator Add(const Key& key, const Value& value);
Iterator Add(const Key& key, Value&& value);
template<class... A> Iterator Emplace(A&&... a);
```

Always adds, and returns the new element; a key already present gets the new element in front of its equivalents. `Emplace` builds the pair from `a...` in the new node. The table grows before the node is linked when the count has reached the threshold. A hasher or equality that throws destroys the new element and leaves the container as it was.

```cpp
MultiDictionary<String, int> m;
m.Add("a", 1);
auto it = m.Add("a", 2);                              // in front of the first "a"
m.Emplace("z", 26);
m.Emplace(std::piecewise_construct, std::forward_as_tuple("k"), std::forward_as_tuple(3));
bool front = m.FindEntry("a") == it;                  // true
```

### Remove, RemoveAt, RemoveRange, RemoveAll

```cpp
template<class K = Key> SizeType Remove(const K& key);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
template<class Pred> SizeType RemoveAll(Pred pred);
```

`Remove(key)` destroys every element of the key's run at once and returns how many; the iterator forms remove one element or a range and return the iterator after it; `RemoveAll` every element the predicate accepts.

```cpp
MultiDictionary<int, String> m = {{1, "a"}, {1, "b"}, {2, "c"}};
auto n = m.Remove(1);                                  // 2: "a" and "b" are destroyed here
m.RemoveAt(begin(m));                                  // "c"
```

### Find, FindEntry, Values, CountOf, ContainsKey

```cpp
template<class K = Key> Value* Find(const K& key) noexcept;
template<class K = Key> const Value* Find(const K& key) const noexcept;
template<class K = Key> Iterator FindEntry(const K& key) noexcept;
template<class K = Key> ConstIterator FindEntry(const K& key) const noexcept;
template<class K = Key> Range<Iterator> Values(const K& key);
template<class K = Key> Range<ConstIterator> Values(const K& key) const;
template<class K = Key> SizeType CountOf(const K& key) const;
template<class K = Key> bool ContainsKey(const K& key) const;
```

`Find` is the first value of the key's run (as a pointer, null when the key is absent), `FindEntry` its first pair, `Values` the run as a [Range](../Core/Range.md) for a range-for, `CountOf` its length (O(1 + count) on average). A `K` other than the key type looks up without building a key when both `Hash` and `Equal` declare `is_transparent`.

```cpp
MultiDictionary<String, int> m = {{"a", 1}, {"a", 2}};   // std::hash and std::equal_to of a String are transparent
auto n = m.CountOf("a");           // 2, no String built for the literal
String text = "a b";
StringView key = text.View(0, 1);  // a view of another string, nothing built either
for (auto& [k, v] : m.Values(key)) {
    (void)v;                       // 2, then 1
}
```

### Hash policy, HashFunction, KeyEqual, Swap, Comparisons, Inner

As for [Dictionary](Dictionary.md#hash-policy): `BucketCount`, `LoadFactor`, `MaxLoadFactor`, `Rehash`, `Reserve`, `HashFunction`, `KeyEqual`, `Swap` and the free `swap`, `==` (equal counts and, for every run of `l`, a run of `r` with the same pairs in some order), `Inner()`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Listener {
    String name;
    Ptr<Listener> forwardTo;           // traced through the node that holds the Listener
};

int main() {
    // Several listeners per topic: a multi-dictionary of traced pointers on the stack
    MultiDictionary<String, Ptr<Listener>> topics;
    Ptr logger = Make<Listener>("logger");
    topics.Add("error", logger);
    topics.Add("error", Make<Listener>("pager", logger));
    topics.Add("info", logger);
    topics.Add("info", Make<Listener>("stats"));

    // The run of one key: every listener of "error"
    std::cout << "error ->";
    for (auto& [topic, listener] : topics.Values("error")) {
        std::cout << ' ' << listener->name;                    // pager logger (the newest first)
    }
    std::cout << '\n';

    // Removing a whole key destroys its Ptr elements at once; the pager is
    // collected, the logger lives on under "info"
    auto removed = topics.Remove("error");
    logger = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << removed << " removed, " << topics.CountOf("info") << " under info, "
              << Collector::LiveObjectCount() << " live objects\n";
    return removed == 2 && topics.Count() == 2 ? 0 : 1;
}
```

The output:

```
error -> pager logger
2 removed, 2 under info, 10 live objects
```

## See also

- [Dictionary](Dictionary.md) for unique keys, [HashMultiSet](HashMultiSet.md) for keys alone, [SortedMultiDictionary](SortedMultiDictionary.md) for an ordered tree
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md), [Range](../Core/Range.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
