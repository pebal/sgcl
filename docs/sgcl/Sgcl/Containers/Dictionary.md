# Sgcl::Dictionary

```cpp
#include "sgcl/Sgcl/Containers/Dictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class Dictionary;
}
```

The same class in the `sgcl` interface: [unordered_map](../../containers/unordered_map.md).

`Dictionary<Key, Value, Hash, Equal>` is `std::unordered_map` over managed nodes, with the names of a dictionary: `Add`, `Set`, `Find`, `ContainsKey`, `Remove`, `Count`. The interface has the constructors, `Add` (unless the key is there), `Emplace`, `Set` (add or replace), `operator[]`, `Remove`, `RemoveAt`, `RemoveAll`, the lookups with transparent hash and equality (`Find`, `FindEntry`, `ContainsKey`), forward iterators through the free `begin`/`end` (`std::ranges` algorithms work), the hash policy (`LoadFactor`/`MaxLoadFactor`/`Rehash`/`Reserve`, `BucketCount`), `HashFunction`/`KeyEqual`, `Swap`, `==`. The behaviour is the one of `std::unordered_map` too: unique keys, an element destroyed the moment it is removed, iterators that stay valid across a rehash and until their element is removed. `Find` hands the value back as a pointer, null when the key is absent: one hash, no exception.

What differs from `std` is where the memory lives and a few details of the layout. The dictionary object holds two `Ptr`s (the bucket array, a managed array of node pointers, and a sentinel node that precedes the first element), the counts, the hasher and the equality, so it lives where a `Ptr` may live. Every element is a node on the managed heap and the nodes form one chain linked by tracked pointers, as in libstdc++: a bucket points at the node before its first node, so the whole chain is traced from the sentinel, an iterator is one raw node pointer, and an iteration is a load per step. A `Dictionary<Key, Ptr<T>>`, or one inside a managed object, is traced like any other managed data, and a cycle through it is collected like any other cycle. Nothing is freed by hand: a removal destroys the element and unlinks the node, the collector reclaims the node later. The hash of each key is cached in its node, so a rehash hashes nothing and a lookup compares hashes before keys. The bucket count is always a power of two (a lookup masks the hash; `Rehash` and the constructors round up), the table starts with no bucket array at all, and it grows when the count reaches `BucketCount() * MaxLoadFactor()`, doubling at least, to eight buckets at the least. A lookup, an iteration and an iterator copy read raw pointers only and pay no write barrier; an insertion, a removal and a rehash store tracked pointers and pay the barrier on each link they relink ([README: Containers](../../containers/README.md#containers)).

## Rules

- A `Dictionary` holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The elements may hold tracked pointers (`Dictionary<int, Ptr<T>>`, or a `Ptr` key: `std::hash<Ptr<T>>` hashes the address; a `String` key hashes by `Hash()` and compares by `Equals()`): the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is removed, cleared, assigned over, or the dictionary is destroyed, exactly as in `std`. The one exception is a dictionary dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread ([README: Threads](../../async/README.md#threads)).
- An iterator, a reference or a pointer to an element (what `Find` returns) is valid while the element is in the dictionary, across insertions, rehashes, removals of other elements, `Swap` and a move of the dictionary (it follows the node). An iterator to a removed element is invalid as in `std`; it keeps the node's memory mapped but not the element. Iterators are trivially copyable and may live anywhere, a `std::vector` of them included: the dictionary roots every node it links.
- A `Ptr` may point at an element of the dictionary, or a member of one (a node is a managed object, [The rules](../../core/README.md#the-rules), 4); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../../core/README.md#the-rules), 6). A dictionary shared between threads is a [ConcurrentDictionary](../Concurrent/ConcurrentDictionary.md).

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using PairType = std::pair<const Key, Value>;      // what a walk gives: `for (auto& [key, value] : d)`
using InnerType = sgcl::unordered_map<Key, Value, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, one raw node pointer
using ConstIterator = InnerType::const_iterator;
```

`Iterator` converts to `ConstIterator`, not back. An iterator is one word.

### Constructors

```cpp
Dictionary();
explicit Dictionary(SizeType buckets);
template<std::input_iterator It> Dictionary(It first, It last);
Dictionary(std::initializer_list<PairType> il);
explicit Dictionary(InnerType c) noexcept;
Dictionary(const Dictionary& other);
Dictionary(Dictionary&& other) noexcept;
```

The default constructor allocates nothing (`BucketCount() == 0`). A bucket count is rounded up to a power of two; 0 means no array yet. The range constructor, given a forward range, sizes the table for the distance first; a key seen twice keeps its first value. A copy reproduces `other`'s bucket count, order and `MaxLoadFactor`; a move takes the table over and leaves `other` empty. An element constructor or hasher that throws while a constructor runs destroys the elements built so far.

```cpp
Dictionary<String, int> ages = {{"ann", 31}, {"bob", 27}};
Dictionary<int, int> sized(100);                                          // 128 buckets, no elements
List<Pair<int, int>> src = {{2, 20}, {1, 10}};
Dictionary<int, int> fromRange(begin(src), end(src));
Dictionary<String, int> taken = std::move(ages);                     // ages is empty now
```

### Destructor

```cpp
~Dictionary();
```

Destroys the elements when the dictionary dies on a stack or inside a managed object destroyed by hand. In a sweep (the dictionary inside a dying managed object) it does nothing: the nodes are garbage of the same sweep and destroy their elements when the sweep reaches them. The nodes, the bucket array and the sentinel are reclaimed by the collector in both cases.

### operator=

```cpp
Dictionary& operator=(const Dictionary& other);
Dictionary& operator=(Dictionary&& other) noexcept;
```

Copy assignment builds a copy of `other` and swaps it in (the old elements die when the temporary does); self-assignment is a no-op. Move assignment clears this dictionary, destroying its elements at once, and takes the table over.

```cpp
Dictionary<int, int> a = {{1, 1}}, b;
b = a;                       // copies
a = std::move(b);            // a holds 1, b is empty
```

### begin, end

```cpp
Iterator begin(Dictionary&) noexcept;               ConstIterator begin(const Dictionary&) noexcept;   // free functions
Iterator end(Dictionary&) noexcept;                 ConstIterator end(const Dictionary&) noexcept;
```

Forward iterators over one chain of nodes; `end(d)` is a null iterator. The order is the chain's, bucket by bucket, and changes with a rehash. An iterator is a raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be kept in unmanaged memory for as long as its element is in the dictionary.

```cpp
Dictionary<String, int> m = {{"a", 1}, {"b", 2}};
int sum = 0;
for (auto& [key, value] : m) {
    sum += value;                          // 3, in whichever order
}
std::vector<decltype(m)::Iterator> kept;   // iterators in unmanaged memory: fine
kept.push_back(m.FindEntry("a"));
m.Rehash(64);                              // kept[0] still points at "a"
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

`Count()` is a stored count, O(1).

### Clear

```cpp
void Clear() noexcept;
```

Destroys every element at once and unlinks every node; the bucket array, the hasher, the equality and `MaxLoadFactor` stay. The nodes are reclaimed by the collector.

```cpp
Dictionary<int, String> m = {{1, "a"}, {2, "b"}};
auto buckets = m.BucketCount();
m.Clear();                                  // both strings are destroyed here
bool same = m.BucketCount() == buckets;     // true
```

### Add, Emplace

```cpp
bool Add(const Key& key, const Value& value);
bool Add(const Key& key, Value&& value);
bool Add(Key&& key, Value&& value);
template<class... A> bool Emplace(const Key& key, A&&... a);
template<class... A> bool Emplace(Key&& key, A&&... a);
```

Looks the key up first and, only when the key is new, adds the pair (`Add`) or builds the value from `a...` in place (`Emplace`): the value type need not be movable or copyable, and `a...` are not touched on a duplicate. Returns whether the key was new.

```cpp
struct Pinned {
    int value;
    explicit Pinned(int v) : value(v) {}
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
};
Dictionary<int, Pinned> m;
m.Emplace(1, 10);                                           // Pinned(10) built inside the node
bool fresh = m.Emplace(1, 11);                              // false; no Pinned(11) was built
Dictionary<String, int> ages;
bool added = ages.Add("ann", 31);                           // true
added = ages.Add("ann", 32);                                // false: 31 stays
```

### Set

```cpp
void Set(const Key& key, const Value& value);
void Set(const Key& key, Value&& value);
```

Adds `{key, value}` when the key is new, otherwise assigns `value` to the value under the key in place.

```cpp
Dictionary<String, int> m;
m.Set("a", 1);                          // added
m.Set("a", 2);                          // replaced: *m.Find("a") is 2
```

### operator[]

```cpp
Value& operator[](const Key& key);
Value& operator[](Key&& key);
```

Inserts a value-initialized value for a new key (built in place) and returns a reference to it; for a `Ptr` value type that is a null pointer.

```cpp
Dictionary<String, int> counts;
++counts["x"];                       // inserted as 0, now 1
++counts["x"];                       // 2
```

### Remove, RemoveAt, RemoveRange

```cpp
template<class K = Key> bool Remove(const K& key);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and, the iterator forms, returns the iterator after it. The key form says whether there was one. Removing during an iteration is done as with `std`: `it = m.RemoveAt(it)`. The bucket count never shrinks on a removal.

```cpp
Dictionary<int, String> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
m.Remove(2);                                       // "b" is destroyed here
for (auto it = begin(m); it != end(m);) {
    it = it->first % 2 ? m.RemoveAt(it) : std::next(it);   // 1 and 3 go
}
auto rest = m.Count();                             // 1: {4, "d"}
```

### Take

```cpp
template<class K = Key> Optional<Value> Take(const K& key);
```

The value under the key, moved out, and the entry removed; `None` when the key is absent: what Java's `remove(key)` and C#'s `Remove(key, out value)` hand back. A `K` other than the key type looks up without building a key when the lookups are transparent (a `string_view` for a `String`).

```cpp
Dictionary<String, int> m = {{"a", 1}};
Optional<int> taken = m.Take("a");                 // 1, and m is empty
Optional<int> none = m.Take("a");                  // None
```

### RemoveAll

```cpp
template<class Pred> SizeType RemoveAll(Pred pred);
```

Removes every element for which `pred(pair)` is true and returns how many; each removed element is destroyed at once.

```cpp
Dictionary<int, int> m = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
auto n = m.RemoveAll([](const auto& p) { return p.first % 2 == 0; });   // 2; m holds 1 and 3
```

### Swap

```cpp
void Swap(Dictionary& other) noexcept;
void swap(Dictionary& l, Dictionary& r) noexcept;   // free function
```

Exchanges the tables, counts, load factors, hashers and equalities; no element is touched, and every iterator keeps pointing at its element, now in the other dictionary.

```cpp
Dictionary<int, int> a = {{1, 1}}, b = {{2, 2}};
auto it = begin(a);
swap(a, b);                            // it still points at {1, 1}, which is in b now
bool moved = it == b.FindEntry(1);     // true
```

### Find, FindEntry, ContainsKey

```cpp
template<class K = Key> Value* Find(const K& key) noexcept;
template<class K = Key> const Value* Find(const K& key) const noexcept;
template<class K = Key> Iterator FindEntry(const K& key) noexcept;
template<class K = Key> ConstIterator FindEntry(const K& key) const noexcept;
template<class K = Key> bool ContainsKey(const K& key) const;
```

O(1) on average, reading raw pointers only; the cached hash is compared before the key. `Find` is the value as a pointer, null when the key is absent; `FindEntry` the pair as an iterator, `end(d)` when absent. A `K` other than the key type looks up without building a key when both `Hash` and `Equal` declare `is_transparent`: a `std::string_view` finds a `String` key.

```cpp
Dictionary<String, int> m = {{"apple", 1}};   // std::hash and std::equal_to of a String are transparent
bool has = m.ContainsKey("apple"); // no String is built for the literal
String line = "apple pie";
StringView key = line.View(0, 5);  // a piece of another string, a view holding its object
int* value = m.Find(key);          // *value == 1, nothing built either
```

### Hash policy

```cpp
SizeType BucketCount() const noexcept;
float LoadFactor() const noexcept;
float MaxLoadFactor() const noexcept;
void MaxLoadFactor(float z);
void Rehash(SizeType buckets);
void Reserve(SizeType n);
```

`LoadFactor()` is `Count() / BucketCount()` (0 with no buckets); `MaxLoadFactor()` defaults to 1.0. `MaxLoadFactor(z)` sets it and takes effect on the next insertion (a value that is not positive, or not a number, is ignored). `Rehash(buckets)` makes the bucket count the smallest power of two not below `buckets` and not below `Count() / MaxLoadFactor()`: it may shrink the table; `Rehash(0)` on an empty table leaves it without buckets. `Reserve(n)` is `Rehash` for `n` elements. A rehash relinks the nodes in chain order, hashing nothing (the hash is cached) and invalidating no iterator.

```cpp
Dictionary<int, int> m;
m.Reserve(1000);                                  // 1024 buckets: no rehash while inserting 1000 elements
for (int i : Range(1000)) {
    m.Emplace(i, i);
}
bool fits = m.LoadFactor() <= m.MaxLoadFactor();  // true
m.MaxLoadFactor(0.5f);                            // the table grows on the next insertion if overloaded
m.Rehash(8);                                      // 2048 buckets: never below what the elements need
```

### HashFunction, KeyEqual

```cpp
Hash HashFunction() const;
Equal KeyEqual() const;
```

Copies of the hasher and the equality.

```cpp
Dictionary<int, int> m;
bool eq = m.KeyEqual()(1, 1);                     // true
size_t h = m.HashFunction()(1);
```

### Comparisons

```cpp
bool operator==(const Dictionary& l, const Dictionary& r);
```

Equal counts and, for every element of `l`, an element of `r` with an equal key and an equal value, whatever the bucket counts and orders. `!=` follows; there is no ordering.

```cpp
Dictionary<int, int> a = {{1, 1}, {2, 2}};
Dictionary<int, int> b(1000);
b.Add(2, 2);
b.Add(1, 1);
bool same = a == b;                               // true
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The table inside, as its own type: for what the dictionary's names leave out, the node handles (`extract`, `merge`) and the bucket interface.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    int id;
    Ptr<Node> next;          // a pointer inside an element: traced through the node
};

// The dictionary object lives inside a managed object: its nodes hang off
// it and die with it, elements included, when the Registry is collected.
struct Registry {
    Dictionary<int, Ptr<Node>> byId;
};

int main() {
    Ptr registry = Make<Registry>();
    for (int id : Range(100)) {
        // operator[] inserts a null Ptr; the object is made afterwards
        registry->byId[id] = Make<Node>(id);
    }
    registry->byId[0]->next = registry->byId[1];
    registry->byId[1]->next = registry->byId[0];               // a cycle: collected like any other

    // A dictionary on the stack, keyed by pointer: std::hash<Ptr> hashes the address
    Dictionary<Ptr<Node>, String> names;
    names.Add(registry->byId[0], "zero");
    names.Emplace(registry->byId[1], "one");
    auto one = names.FindEntry(registry->byId[1]);
    names.Rehash(256);                                         // `one` is still valid
    std::cout << one->second << " is node " << one->first->id << '\n';

    // Removing from the registry destroys the Ptr elements at once;
    // nodes 0 and 1 stay reachable through `names`, the rest is garbage
    registry->byId.RemoveAll([](const auto& p) { return p.first >= 2; });
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << registry->byId.Count() << " in the registry, "
              << Collector::LiveObjectCount() << " live objects\n";
    return registry->byId.Count() == 2 && names.Count() == 2 ? 0 : 1;
}
```

The output:

```
one is node 1
2 in the registry, 13 live objects
```

## See also

- [MultiDictionary](MultiDictionary.md) for equal keys, [HashSet](HashSet.md) and [HashMultiSet](HashMultiSet.md) for keys alone, [SortedDictionary](SortedDictionary.md) for an ordered tree
- [ConcurrentDictionary](../Concurrent/ConcurrentDictionary.md) for a dictionary shared between threads
- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md), [String](../Core/String.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Stack roots](../../../garbage_collector/overview.md#stack-roots)
