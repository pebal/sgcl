# Sgcl::PersistentDictionary

```cpp
#include "sgcl/Sgcl/Concurrent/PersistentDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class PersistentDictionary;
}
```

The same class in the `sgcl` interface: [persistent_map](../../concurrent/persistent_map.md).

`PersistentDictionary<Key, Value, Hash, Equal>` is the persistent hash map of Clojure and Scala: a dictionary every `Set` and `Remove` of which returns a new dictionary and leaves the old one exactly as it was, the two sharing everything but the path that changed. It is a hash array mapped trie, Bagwell's (*Ideal Hash Trees*, 2001) as Clojure's `PersistentHashMap` has it: a node has 32 slots, one per value of the five bits of the hash its level consumes, and stores only the slots in use, packed behind a bitmap; an entry holds an element or the subtrie of the elements whose hashes agree with it so far, and two elements with the same hash to the last bit hang off one entry as a chain. A lookup walks log32(*n*) nodes, four for a million elements; a `Set` or a `Remove` copies those nodes, a few hundred bytes, and shares the rest, and a subtrie left with one element is folded into its parent so that a dictionary emptied holds no node at all. Nothing is ever modified: a dictionary held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [CopyOnWrite](CopyOnWrite.md) or an [Atomic](Atomic.md) ([README: Persistent structures](../../concurrent/README.md#persistent-structures)).

The dictionary is two words and its function objects: the count and a `Ptr` to the root; a copy of it is a copy of those words. The nodes are managed objects that no version owns, made in six sizes (room for 1, 2, 4, 8, 16 or 32 entries) so that each is one object with a destructor for its elements: a node reached by ten versions is one node, and the collector destroys its elements and frees it once the last version that reaches it is dropped. `Find` hands back a pointer to the value, null when the key is absent, as [Dictionary](../Containers/Dictionary.md) does: the value is in a node some version holds, valid for as long as one does.

## Rules

- `PersistentDictionary` holds its root by a `Ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). Its iterators, and the pointers `Find` hands back, hold nothing alive: valid while the dictionary object they came from exists.
- Every member is `const`. `Set`, `Emplace` and `Remove` return the new dictionary; the one they were called on is unchanged, and stays so for as long as it is held. The elements are `const` through the dictionary.
- A change copies the elements of the nodes on the path, up to 32 per node: the copy constructors of `Key` and `Value` are what a change costs, plus the nodes.
- A key or a value holding a `Ptr` is traced where it lives, in a node; one with a destructor is destroyed when the collector frees its node, once no version reaches it.
- With a transparent hash and equality (as those of a [String](../Core/String.md) are) the lookups take a key of another type and build none: a `string_view` or a literal finds a `String` key with no string made.
- Sharing between threads: any number of threads read any version. A `PersistentDictionary` variable that one thread replaces while others read it needs synchronization, and `CopyOnWrite<PersistentDictionary<Key, Value>>` is the shape for it: `Load()` is one atomic load for a snapshot, `Update(f)` copies two words, applies `f` (a `Set`, a `Remove`) and swings the pointer, so an update costs O(log *n*) where a `CopyOnWrite<Dictionary<Key, Value>>` costs a copy of everything.
- The order of iteration is the trie's, the bits of the hashes.

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using InnerType = sgcl::persistent_map<Key, Value, Hash, Equal>;
using PairType = Pair<const Key, Value>;
using SizeType = size_t;
using Iterator = /* forward iterator over const PairType */;
using ConstIterator = Iterator;
```

### Constructors

```cpp
PersistentDictionary();
template<std::input_iterator It> PersistentDictionary(It first, It last);
PersistentDictionary(std::initializer_list<PairType> il);
explicit PersistentDictionary(InnerType m) noexcept;
PersistentDictionary(const PersistentDictionary&) noexcept;
PersistentDictionary& operator=(const PersistentDictionary&) noexcept;
```

```cpp
PersistentDictionary<String, int> ports = {{"http", 80}, {"https", 443}};
```

### Count, IsEmpty, HashFunction, KeyEqual

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
Hash HashFunction() const;
Equal KeyEqual() const;
```

### Find, TryGet, At, ContainsKey

```cpp
template<class K = KeyType> const ValueType* Find(const K& key) const;   // the value, null when absent
template<class K = KeyType> Optional<ValueType> TryGet(const K& key) const;   // a copy of the value, or None
template<class K = KeyType> const ValueType& At(const K& key) const;     // std::out_of_range when absent
template<class K = KeyType> bool ContainsKey(const K& key) const;
```

The value under the key: log32(*n*) nodes walked, the key compared once; 13 ns for a random key of a hundred thousand `int`s. A `K` other than the key type looks up without building a key when the hash and the equality are transparent.

```cpp
PersistentDictionary<String, int> ports = {{"http", 80}, {"https", 443}};
if (auto p = ports.Find("https")) {          // a literal: transparent, no String made
    std::cout << *p << '\n';
}
String url = "ftp://host";
StringView name = url.View(0, 3);            // a view of another string, nothing built
bool known = ports.ContainsKey(name);        // false
Optional<int> port = ports.TryGet("http");   // 80
```

### Set, Emplace

```cpp
PersistentDictionary Set(const KeyType& key, const ValueType& value) const;
PersistentDictionary Set(const KeyType& key, ValueType&& value) const;
PersistentDictionary Set(KeyType&& key, ValueType&& value) const;
template<class... A> PersistentDictionary Emplace(const KeyType& key, A&&... a) const;
```

The dictionary with `value` under `key`, added, or in place of the value there (the count grows only when the key was absent): the nodes on the path copied, log32(*n*) of them, the rest shared. `Emplace` builds the value from the arguments. 590 ns per `Set` while building a hundred thousand `int`s.

```cpp
PersistentDictionary<String, int> ports = {{"http", 80}};
auto more = ports.Set("https", 443);         // ports has one element, more has two
auto changed = more.Set("http", 8080);       // more still says 80
auto built = ports.Emplace("ssh", 22);
```

### Remove

```cpp
template<class K = KeyType> PersistentDictionary Remove(const K& key) const;
```

The dictionary without the element under the key: the path copied, a node emptied dropped, a subtrie left with one element folded into its parent; the same dictionary, sharing everything, when the key is absent.

```cpp
PersistentDictionary<String, int> ports = {{"http", 80}, {"https", 443}};
auto fewer = ports.Remove("http");           // ports still has both
```

### begin, end

```cpp
template<class K, class V, class H, class E> auto begin(const PersistentDictionary<K, V, H, E>& d) noexcept;
template<class K, class V, class H, class E> auto end(const PersistentDictionary<K, V, H, E>& d) noexcept;
```

Free functions, for a range-for over the pairs: 8.5 ns per element over a hundred thousand.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The `sgcl::persistent_map` inside.

### Comparison

```cpp
friend bool operator==(const PersistentDictionary& a, const PersistentDictionary& b);
friend bool operator!=(const PersistentDictionary& a, const PersistentDictionary& b);
```

The same keys with equal values, whatever the two share.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A configuration read on every request by many threads and changed
// once in a while by one: the readers take a snapshot, the writer
// publishes a new version that shares all but one path with the old
// one, and nobody copies a dictionary or takes a lock
int main() {
    CopyOnWrite<PersistentDictionary<String, int>> limits(PersistentDictionary<String, int>{{"connections", 100}, {"requests", 1000}});
    Atomic stop = false;
    Atomic<long> reads = 0, inconsistent = 0;
    List<Thread> readers;
    for (int r : Range(4)) {
        readers.Emplace([&] {
            while (!stop) {
                auto snapshot = limits.Load();                      // one load: this version, for as long as snapshot lives
                auto c = snapshot->Find("connections");
                auto q = snapshot->Find("requests");
                inconsistent += !c || !q || *q != 10 * *c;          // the writer keeps the ratio: a snapshot is whole
                ++reads;
            }
        });
    }
    for (int i : Range(1, 101)) {
        limits.Update([i](auto& d) {                                // two paths copied, the rest of the dictionary shared
            d = d.Set("connections", 100 * i).Set("requests", 1000 * i);
        });
    }
    stop = true;
    for (auto& r : readers) {
        r.Join();
    }
    auto final = limits.Load();
    std::cout << reads << " reads, " << inconsistent << " inconsistent, connections " << final->At("connections") << ", " << final->Count() << " keys\n";
    return inconsistent == 0 && final->At("requests") == 100000 ? 0 : 1;
}
```

The output of one run (the reads depend on how the threads interleave):

```
703 reads, 0 inconsistent, connections 10000, 2 keys
```

## See also

- [PersistentSet](PersistentSet.md), the same trie with the value as the element; [PersistentList](PersistentList.md)
- [CopyOnWrite](CopyOnWrite.md), how a version is published to other threads; [Atomic](Atomic.md)
- [Dictionary](../Containers/Dictionary.md), the mutable one; [ConcurrentDictionary](ConcurrentDictionary.md), the one many threads change in place
- [persistent_map: Measured](../../concurrent/persistent_map.md#measured), the numbers
- [README: Persistent structures](../../concurrent/README.md#persistent-structures), [README: The rules](../../core/README.md#the-rules)
