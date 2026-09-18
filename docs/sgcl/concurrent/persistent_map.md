# sgcl::persistent_map

```cpp
#include "sgcl/concurrent/persistent_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class persistent_map;
}
```

The same class in the `Sgcl` interface: [PersistentDictionary](../Sgcl/Concurrent/PersistentDictionary.md).

`sgcl::persistent_map<Key, T, Hash, KeyEqual>` is the persistent hash map of Clojure and Scala: a map every `insert` and `erase` of which returns a new map and leaves the old one exactly as it was, the two sharing everything but the path that changed. It is a hash array mapped trie, Bagwell's (*Ideal Hash Trees*, 2001) as Clojure's `PersistentHashMap` has it: a node has 32 slots, one per value of the five bits of the hash its level consumes, and stores only the slots in use, packed in slot order behind a bitmap, so that the entry of a slot is at the population count of the bits below it; an entry holds an element or the subtrie of the elements whose hashes agree with it so far, and two elements with the same hash to the last bit hang off one entry as a chain. A lookup walks log32(*n*) nodes, four for a million elements, and compares the key once; an `insert` or an `erase` copies those nodes, a few hundred bytes, and shares the rest, and a subtrie left with one element is folded into its parent so that a map erased down to nothing holds no node at all. Nothing is ever modified: a map held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [copy_on_write](copy_on_write.md) or an [atomic](atomic.md) ([README: Persistent structures](README.md#persistent-structures)).

The map is two words and its function objects: the size and a `tracked_ptr` to the root. A copy of it is a copy of those words. The nodes are managed objects that no version owns, made in six sizes (room for 1, 2, 4, 8, 16 or 32 entries) so that each is one object with a destructor for its elements: a node reached by ten versions is one node, and the collector destroys its elements and frees it once the last version that reaches it is dropped, the accounting a persistent map without a collector does with a reference count per node. `find` hands back a pointer to the value, null when the key is absent, the way the [Sgcl dictionaries](../Sgcl/Containers/Dictionary.md) do: the value is in a node some version holds, and it is valid for as long as one does.

## Rules

- `sgcl::persistent_map` holds its root by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). Its iterators, and the pointers `find` hands back, hold nothing alive: valid while the map object they came from exists, as `std`'s are.
- Every member is `const`. `insert`, `emplace` and `erase` return the new map; the one they were called on is unchanged, and stays so for as long as it is held. The elements are `const` through the map.
- A change copies the elements of the nodes on the path, up to 32 per node: the copy constructors of `Key` and `T` are what a change costs, plus the nodes.
- A key or a value holding tracked pointers is traced where it lives, in a node; one with a destructor is destroyed when the collector frees its node, once no version reaches it. Nothing is destroyed by an `erase`: the old version still holds the element.
- With a transparent hash and equality (`is_transparent`, as `std::hash` and `std::equal_to` of a [string](../core/string.md) are) the lookups take a key of another type and build none: a `string_view` or a literal finds a `string` key with no string made for the search.
- Sharing between threads: any number of threads read any version. A `persistent_map` variable that one thread replaces while others read it is the one thing that needs synchronization, and `copy_on_write<persistent_map<Key, T>>` is the shape for it: `load()` is one atomic load for a snapshot, `update(f)` copies two words, applies `f` (an `insert`, an `erase`) and swings the pointer, so an update costs O(log *n*) where a `copy_on_write<unordered_map<Key, T>>` costs a copy of everything. An `atomic<tracked_ptr<persistent_map<Key, T>>>` does the same with the version in a managed object of its own.
- The order of iteration is the trie's, the bits of the hashes; it changes with nothing but the elements.

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using value_type = pair<const Key, T>;   // sgcl::pair, the alias of std::pair (sgcl/core/aliases.h)
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using difference_type = ptrdiff_t;
using const_iterator = /* forward iterator over const value_type */;
using iterator = const_iterator;
```

### Constructors

```cpp
persistent_map();
explicit persistent_map(const Hash& hash, const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt> persistent_map(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
persistent_map(std::initializer_list<value_type> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
persistent_map(const persistent_map&) noexcept;
persistent_map& operator=(const persistent_map&) noexcept;
```

An empty map holds no node at all. A range or a list is built at once, not by an insert per element: the elements are taken into a buffer, sorted by their hashes in the trie's order (the low chunk first, as the trie consumes it), and the trie is made from the root down, every node allocated once at its size and every entry constructed once; a key that occurs twice keeps its last occurrence, as an insert would replace the earlier. 90 ns per element for 200,000 random `long` keys, against 700 for the inserts that would build the same map (the benchmarks page).

```cpp
sgcl::persistent_map<sgcl::string, int> ports = {{"http", 80}, {"https", 443}};
std::map<int, int> squares = {{1, 1}, {2, 4}, {3, 9}};
sgcl::persistent_map from_range(squares.begin(), squares.end());   // deduced: persistent_map<int, int>
```

### begin, end, cbegin, cend, size, empty, hash_function, key_eq

```cpp
const_iterator begin() const noexcept;
const_iterator end() const noexcept;
size_type size() const noexcept;
bool empty() const noexcept;
hasher hash_function() const;
key_equal key_eq() const;
```

A forward iterator over `pair<const Key, T>`, in the order of the trie: 8.5 ns per element over a hundred thousand.

### find, contains, count, at

```cpp
const T* find(const Key& key) const;
bool contains(const Key& key) const;
size_type count(const Key& key) const;
const T& at(const Key& key) const;              // std::out_of_range when absent
template<class K> const T* find(const K& key) const;   // when Hash and KeyEqual are transparent: and contains, count, at, erase
```

The value under the key, or null: log32(*n*) nodes walked, the key compared once (once per element of a chain when hashes collide); 13 ns for a random key of a hundred thousand `int`s.

```cpp
sgcl::persistent_map<sgcl::string, int> ports = {{"http", 80}, {"https", 443}};
if (auto p = ports.find("https")) {          // a literal: transparent, no string made
    std::cout << *p << '\n';
}
sgcl::string url = "ftp://host";
bool known = ports.contains(url.view(0, 3)); // false: a view of another string, nothing built
```

### insert, emplace

```cpp
persistent_map insert(const Key& key, const T& value) const;
persistent_map insert(const Key& key, T&& value) const;
persistent_map insert(Key&& key, const T& value) const;
persistent_map insert(Key&& key, T&& value) const;
persistent_map insert(const value_type& value) const;
template<class... A> persistent_map emplace(const Key& key, A&&... a) const;
```

The map with `value` under `key`, added, or in place of the value there (the size grows only when the key was absent): the nodes on the path copied, log32(*n*) of them, the rest shared. `emplace` builds the value from the arguments. 590 ns per insert while building a hundred thousand `int`s, 750 ns replacing values in the built map.

```cpp
sgcl::persistent_map<sgcl::string, int> ports = {{"http", 80}};
auto more = ports.insert("https", 443);      // ports has one element, more has two
auto changed = more.insert("http", 8080);    // more still says 80
auto built = ports.emplace("ssh", 22);
```

### erase

```cpp
persistent_map erase(const Key& key) const;
template<class K> persistent_map erase(const K& key) const;   // when Hash and KeyEqual are transparent
```

The map without the element under the key: the path copied, a node emptied dropped, a subtrie left with one element folded into its parent; the same map, sharing everything, when the key is absent. 670 ns per erase over a hundred thousand `int`s.

```cpp
sgcl::persistent_map<sgcl::string, int> ports = {{"http", 80}, {"https", 443}};
auto fewer = ports.erase("http");            // ports still has both
```

### Comparison

```cpp
friend bool operator==(const persistent_map& a, const persistent_map& b);
friend bool operator!=(const persistent_map& a, const persistent_map& b);
```

The same keys with equal values, whatever the two share.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A configuration read on every request by many threads and changed
// once in a while by one: the readers take a snapshot, the writer
// publishes a new version that shares all but one path with the old
// one, and nobody copies a map or takes a lock
int main() {
    sgcl::copy_on_write<sgcl::persistent_map<sgcl::string, int>> limits(sgcl::persistent_map<sgcl::string, int>{{"connections", 100}, {"requests", 1000}});
    sgcl::atomic stop = false;
    sgcl::atomic<long> reads = 0, inconsistent = 0;
    sgcl::vector<sgcl::thread> readers;
    for (int r : sgcl::range(4)) {
        readers.emplace_back([&] {
            while (!stop) {
                auto snapshot = limits.load();                      // one load: this version, for as long as snapshot lives
                auto c = snapshot->find("connections");
                auto q = snapshot->find("requests");
                inconsistent += !c || !q || *q != 10 * *c;          // the writer keeps the ratio: a snapshot is whole
                ++reads;
            }
        });
    }
    for (int i : sgcl::range(1, 101)) {
        limits.update([i](auto& m) {                                // two paths copied, the rest of the map shared
            m = m.insert("connections", 100 * i).insert("requests", 1000 * i);
        });
    }
    stop = true;
    for (auto& r : readers) {
        r.join();
    }
    auto final = limits.load();
    std::cout << reads << " reads, " << inconsistent << " inconsistent, connections " << final->at("connections") << ", " << final->size() << " keys\n";
    return inconsistent == 0 && final->at("requests") == 100000 ? 0 : 1;
}
```

The output of one run (the reads depend on how the threads interleave):

```
878 reads, 0 inconsistent, connections 10000, 2 keys
```

## Measured

On an Apple M-series core, `-O2`, `persistent_map<int, int>` of a hundred thousand elements: `find` 13 ns (`std::unordered_map`: 10), a miss 12 ns, `insert` 590 ns while building and 750 ns replacing a value (`std::unordered_map`: 52), the constructor from a range 90 ns per element, `erase` 670 ns, iteration 8.5 ns per element. An insert or an erase copies four nodes, three of them full: a hundred pointers stored, each with the collector's write barrier, which is where the time goes. One version of a hundred thousand `int` pairs is 2.99 MB, 30 bytes per element; a second version differing in one value adds 1.6 KB, a third with one more element 1.7 KB.

## See also

- [persistent_set](persistent_set.md), the same trie with the key as the element; [persistent_vector](persistent_vector.md)
- [copy_on_write](copy_on_write.md), how a version is published to other threads; [atomic](atomic.md)
- [unordered_map](../containers/unordered_map.md), the mutable one; [concurrent_unordered_map](concurrent_unordered_map.md), the one many threads change in place
- [Benchmarks](benchmarks.md#the-single-producer-queue-the-cache-and-the-persistent-map): insert, find and the range constructor against `std::map`
- [README: Persistent structures](README.md#persistent-structures), [README: The rules](../core/README.md#the-rules)
