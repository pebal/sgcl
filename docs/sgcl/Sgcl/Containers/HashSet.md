# Sgcl::HashSet

```cpp
#include "sgcl/Sgcl/Containers/HashSet.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class HashSet;
}
```

The same class in the `sgcl` interface: [unordered_set](../../containers/unordered_set.md).

`HashSet<T, Hash, Equal>` is `std::unordered_set` over managed nodes: the same hash table as [Dictionary](Dictionary.md), holding values alone. The interface has the constructors, `Add` (unless the value is there: `bool`), `Emplace`, `Remove`, `RemoveAt`, `RemoveAll`, the lookups with transparent hash and equality (`Contains`, `FindEntry`), forward iterators through the free `begin`/`end`, the hash policy (`LoadFactor`/`MaxLoadFactor`/`Rehash`/`Reserve`, `BucketCount`), `HashFunction`/`KeyEqual`, `Swap`, `==`. The behaviour is `std::unordered_set`'s: unique values, an element destroyed the moment it is removed, iterators valid across a rehash and until their element is removed.

What differs from `std` is where the memory lives. The set holds two `Ptr`s (the bucket array and a sentinel node), the counts, the hasher and the equality, so it lives where a `Ptr` may live; the elements are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel, so a `HashSet<Ptr<T>>` is a set of traced pointers (`std::hash<Ptr<T>>` hashes the address: a set of objects by identity) and a cycle through a set is collected like any other. Nothing is freed by hand: a removal destroys the element and unlinks the node, the collector reclaims the node later. The hash of each value is cached in its node. The bucket count is 0 or a power of two, and the table grows when the count reaches `BucketCount() * MaxLoadFactor()`, doubling at least, to eight buckets at the least. Iterators are one raw node pointer each, trivially copyable, storable anywhere. As in `std`, the iterators yield `const T&`: an element is never modified in place (the cached hash and the bucket would no longer match); remove it and add it back. Lookups and iteration pay no write barrier; insertions, removals and rehashes store tracked pointers and pay the barrier on each link they relink ([README: Containers](../../containers/README.md#containers)).

## Rules

- A `HashSet` holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is removed, cleared, assigned over, or the set is destroyed, exactly as in `std`. The one exception is a set dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the set, across insertions, rehashes, removals of other elements, `Swap` and a move of the set. An iterator to a removed element is invalid as in `std`.
- An element cannot be modified through an iterator (they yield `const T&`); remove it and add the changed one.
- A `Ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_set`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../../core/README.md#the-rules), 6). A set shared between threads is a [ConcurrentHashSet](../Concurrent/ConcurrentHashSet.md).

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::unordered_set<T, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, one raw node pointer, yields const T&
using ConstIterator = InnerType::const_iterator;    // the same type
```

### Constructors, destructor, operator=

```cpp
HashSet();
explicit HashSet(SizeType buckets);
template<std::input_iterator It> HashSet(It first, It last);
HashSet(std::initializer_list<T> il);
explicit HashSet(InnerType c) noexcept;
HashSet(const HashSet& other);
HashSet(HashSet&& other) noexcept;
~HashSet();
HashSet& operator=(const HashSet& other);
HashSet& operator=(HashSet&& other) noexcept;
```

As for [Dictionary](Dictionary.md#constructors): the default constructor allocates nothing, a bucket count is rounded up to a power of two, a range sizes the table first and keeps the first of equal values, a copy has nodes of its own, a move takes the table over. The destructor destroys the elements unless the set dies in a sweep; copy assignment builds a copy and swaps it in, move assignment clears and takes the table over.

```cpp
HashSet<String> names = {"ann", "bob"};
HashSet<int> sized(100);                                     // 128 buckets, no elements
List src = {2, 1, 2};
HashSet<int> fromRange(begin(src), end(src));              // 1 and 2
HashSet<String> taken = std::move(names);               // names is empty now
```

### begin, end

```cpp
Iterator begin(HashSet&) noexcept;    ConstIterator begin(const HashSet&) noexcept;   // free functions
Iterator end(HashSet&) noexcept;      ConstIterator end(const HashSet&) noexcept;
```

Forward iterators over one chain of nodes, yielding `const T&`; `end(s)` is a null iterator. The order is the chain's, bucket by bucket, and changes with a rehash. An iterator is a raw node pointer: copying and advancing it costs a load, never a write barrier.

```cpp
HashSet s = {1, 2, 3};
int sum = 0;
for (int v : s) {
    sum += v;                              // 6, in whichever order
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
bool Add(const T& value);
bool Add(T&& value);
template<class... A> bool Emplace(A&&... a);
```

`Add` looks the value up and adds it when it is new, saying so; nothing is built on a duplicate. `Emplace` builds the value from `a...` in a new node before it is looked up, as in `std`; if it is already there the new element is destroyed and `false` returned. The table grows before the node is linked when the count has reached the threshold; an existing value never rehashes. A hasher or equality that throws destroys the new element and leaves the set as it was.

```cpp
HashSet<String> s;
bool added = s.Add("a");                              // true
added = s.Add("a");                                   // false
s.Emplace(3, 'x');                                    // "xxx"
bool fresh = s.Emplace("xxx");                        // false
```

### Remove, RemoveAt, RemoveRange, RemoveAll

```cpp
template<class K = T> bool Remove(const K& value);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
template<class Pred> SizeType RemoveAll(Pred pred);
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and, the iterator forms, returns the iterator after it. The value form says whether there was one; `RemoveAll` removes every element the predicate accepts and returns how many.

```cpp
HashSet s = {1, 2, 3, 4};
s.Remove(2);
for (auto it = begin(s); it != end(s);) {
    it = *it % 2 ? s.RemoveAt(it) : std::next(it);   // 1 and 3 go
}
auto n = s.RemoveAll([](int v) { return v > 3; });   // 1: the 4; s is empty
```

### Contains, FindEntry

```cpp
template<class K = T> bool Contains(const K& value) const;
template<class K = T> ConstIterator FindEntry(const K& value) const noexcept;
```

O(1) on average, reading raw pointers only; the cached hash is compared before the value. A `K` other than the value type looks up without building a value when both `Hash` and `Equal` declare `is_transparent`: a `std::string_view` finds a `std::string`.

```cpp
HashSet<String> s = {"apple"};     // std::hash and std::equal_to of a String are transparent
bool has = s.Contains("apple");    // no String is built for the literal
String line = "apple pie";
bool piece = s.Contains(line.View(0, 5));   // a view of another string, nothing built either
```

### Hash policy, HashFunction, KeyEqual, Swap, Comparisons, Inner

As for [Dictionary](Dictionary.md#hash-policy): `BucketCount`, `LoadFactor`, `MaxLoadFactor`, `Rehash`, `Reserve`, `HashFunction`, `KeyEqual`, `Swap` and the free `swap`, `==` (equal counts and every element of one in the other), `Inner()` for the node handles (`extract`, `merge`) and the bucket interface.

```cpp
HashSet<int> a = {1, 2}, b(1000);
b.Add(2);
b.Add(1);
bool same = a == b;                               // true, whatever the bucket counts
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    String name;
    HashSet<Ptr<Node>> peers;        // a set of traced pointers inside a managed object
};

int main() {
    // A graph whose edges are sets: each node is reachable from its peers
    Ptr a = Make<Node>("a");
    Ptr b = Make<Node>("b");
    Ptr c = Make<Node>("c");
    a->peers.Add(b);
    b->peers.Add(a);                            // a cycle
    b->peers.Add(c);
    bool again = b->peers.Add(c);               // false: a duplicate, nothing is added

    // A set of values on the stack, each name once
    HashSet<String> names;
    for (const auto& peer : b->peers) {         // pointers hash by address: any order
        names.Add(peer->name);
    }
    names.Add("b");
    std::cout << names.Count() << " names, " << names.BucketCount() << " buckets\n";

    // Dropping the stack roots: a and b keep each other alive only through
    // their sets, which the collector sees as a cycle
    a = b = c = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << Collector::LiveObjectCount() << " live objects\n";     // the nodes, buckets and sentinel of `names`
    return !again && names.Count() == 3 ? 0 : 1;
}
```

The output:

```
3 names, 8 buckets
8 live objects
```

## See also

- [HashMultiSet](HashMultiSet.md) for equal values, [Dictionary](Dictionary.md) and [MultiDictionary](MultiDictionary.md) for key-value pairs, [SortedSet](SortedSet.md) for an ordered tree
- [ConcurrentHashSet](../Concurrent/ConcurrentHashSet.md) for a set shared between threads
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
