# Sgcl::SortedMultiDictionary

```cpp
#include "sgcl/Sgcl/Containers/SortedDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Compare = std::less<Key>>
    class SortedMultiDictionary;
}
```

The same class in the `sgcl` interface: [multimap](../../containers/multimap.md).

`SortedMultiDictionary<Key, Value, Compare>` is `std::multimap` on a red-black tree whose nodes are managed objects: the same tree as [SortedDictionary](SortedDictionary.md), with equivalent keys allowed. The interface is that of the sorted dictionary without `Set` and `operator[]`, with `Add` and `Emplace` that always add, `Values(key)` for the run of one key, `CountOf(key)` for its length, `Remove(key)` that removes the whole run, `First`/`Last`, `LowerBound`/`UpperBound`/`EqualRange`, the lookups with a transparent comparator, bidirectional iterators through the free `begin`/`end`, `KeyCompare`, `Swap`, `==` and `<=>`, `RemoveAll`. The behaviour is `std::multimap`'s: elements with equivalent keys are adjacent, a new one goes after the ones already there (insertion order within a key is kept), an element is destroyed the moment it is removed.

What differs from `std` is where the memory lives. The object holds one `Ptr` (to a header node), a count and the comparator, so it lives where a `Ptr` may live; the nodes are managed objects linked by tracked pointers, traced from the header, so elements holding `Ptr`s are traced and a cycle through the container is collected like any other. Nothing is freed by hand: a removal destroys the element and unlinks the node, the collector reclaims the node later. Iterators are one raw node pointer each, trivially copyable, storable anywhere, valid while their element is in the container. Lookups and iteration read raw pointers and pay no write barrier; insertions, removals and rebalancing store tracked pointers and pay the barrier on each link they relink ([README: Containers](../../containers/README.md#containers)). The header is allocated on the first insertion: an empty container costs nothing.

## Rules

The rules of [SortedDictionary](SortedDictionary.md#rules): the container lives where a `Ptr` may; the elements may hold tracked pointers and are traced; an element is destroyed the moment it is removed (or, in a sweep, when the sweep reaches its node); an iterator, a reference or a pointer to an element is valid while the element is in the container; a `Ptr` may point at an element; thread safety is that of `std::multimap`.

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using PairType = std::pair<const Key, Value>;
using InnerType = sgcl::multimap<Key, Value, Compare>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // bidirectional, one raw node pointer
using ConstIterator = InnerType::const_iterator;
```

### Constructors, destructor, operator=

```cpp
SortedMultiDictionary();
explicit SortedMultiDictionary(const Compare& cmp);
template<std::input_iterator It> SortedMultiDictionary(It first, It last);
SortedMultiDictionary(std::initializer_list<PairType> il);
explicit SortedMultiDictionary(InnerType c) noexcept;
SortedMultiDictionary(const SortedMultiDictionary& other);
SortedMultiDictionary(SortedMultiDictionary&& other) noexcept;
~SortedMultiDictionary();
SortedMultiDictionary& operator=(const SortedMultiDictionary& other);
SortedMultiDictionary& operator=(SortedMultiDictionary&& other) noexcept;
```

As for [SortedDictionary](SortedDictionary.md#constructors), except that a range with a key seen twice keeps every element, in its order.

```cpp
SortedMultiDictionary<String, int> scores = {{"ann", 31}, {"ann", 27}};   // both, 31 first
SortedMultiDictionary<int, int, std::greater<int>> desc(std::greater<int>{});
```

### begin, end, First, Last, IsEmpty, Count, Clear, KeyCompare

As for [SortedDictionary](SortedDictionary.md#begin-end): the free `begin`/`end`, the least and the greatest pair, the stored count, `Clear` that destroys every element and keeps the header, a copy of the comparator.

```cpp
SortedMultiDictionary<int, char> m = {{2, 'b'}, {1, 'a'}, {2, 'c'}};
for (auto& [key, value] : m) { /* 1 a, 2 b, 2 c */ }
assert(m.First().first == 1 && m.Last().second == 'c' && m.Count() == 3);
```

### Add, Emplace

```cpp
Iterator Add(const Key& key, const Value& value);
Iterator Add(const Key& key, Value&& value);
template<class... A> Iterator Emplace(A&&... a);
```

Always adds, after the elements with an equivalent key, and returns the new element. `Emplace` builds the pair from `a...` in the new node. O(log n).

```cpp
SortedMultiDictionary<String, int> m;
m.Add("a", 1);
auto it = m.Add("a", 2);                              // after the first "a"
m.Emplace("z", 26);
bool second = std::next(m.FindEntry("a")) == it;      // true
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
SortedMultiDictionary<int, String> m = {{1, "a"}, {1, "b"}, {2, "c"}};
auto n = m.Remove(1);                                  // 2: "a" and "b" are destroyed here
m.RemoveAt(begin(m));                                  // "c"
```

### Find, FindEntry, Values, CountOf, ContainsKey, EqualRange, LowerBound, UpperBound

```cpp
template<class K = Key> Value* Find(const K& key) noexcept;                  // the first value of the run, null when absent
template<class K = Key> Iterator FindEntry(const K& key) noexcept;           // its first pair, end() when absent
template<class K = Key> Range<Iterator> Values(const K& key);                // the run: EqualRange under the dictionary's name
template<class K = Key> SizeType CountOf(const K& key) const;
template<class K = Key> bool ContainsKey(const K& key) const;
template<class K> Range<Iterator> EqualRange(const K& key) noexcept;
template<class K> Iterator LowerBound(const K& key) noexcept;
template<class K> Iterator UpperBound(const K& key) noexcept;
// and the const forms
```

O(log n), reading raw pointers only; `CountOf` O(log n + count). A `K` other than the key type looks up without building a key for a transparent comparator.

```cpp
SortedMultiDictionary<String, int> m = {{"a", 1}, {"a", 2}};   // std::less of a String is transparent
auto n = m.CountOf("a");           // 2, no String built for the literal
String text = "a b";
StringView key = text.View(0, 1);  // a view of another string, nothing built either
for (auto& [k, v] : m.Values(key)) {
    (void)v;                       // 1, then 2
}
```

### Swap, Comparisons, Inner

As for [SortedDictionary](SortedDictionary.md#swap): `Swap` and the free `swap`, `==` and `<=>` (element-wise, in order), `Inner()`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Entry {
    String what;
    Ptr<Entry> cause;          // traced through the node that holds the Entry
};

int main() {
    // A sorted multi-dictionary on the stack: several events per day, in the order they came
    SortedMultiDictionary<int, Ptr<Entry>> log;
    Ptr first = Make<Entry>("boot");
    log.Add(1, first);
    log.Add(1, Make<Entry>("login", first));
    log.Add(2, Make<Entry>("logout"));
    log.Add(2, Make<Entry>("shutdown"));
    log.Add(1, Make<Entry>("late entry"));           // goes after the other day-1 events

    // The run of one key, in insertion order
    std::cout << "day 1:";
    for (auto& [day, event] : log.Values(1)) {
        std::cout << ' ' << event->what;                       // boot login late entry
    }
    std::cout << '\n';

    // Removing a whole key destroys its elements (the Ptrs) at once; the
    // Events they pointed at are collected, except "boot", still held by `first`
    auto removed = log.Remove(1);
    first = nullptr;                                           // now "boot" is unreachable too
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << removed << " removed, " << log.Count() << " left, "
              << Collector::LiveObjectCount() << " live objects\n";
    return removed == 3 && log.CountOf(2) == 2 ? 0 : 1;
}
```

The output:

```
day 1: boot login late entry
3 removed, 2 left, 7 live objects
```

## See also

- [SortedDictionary](SortedDictionary.md) for unique keys, [SortedMultiSet](SortedMultiSet.md) for keys alone, [MultiDictionary](MultiDictionary.md) for a hash table
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md), [Range](../Core/Range.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
