# sgcl::immutable::map

```cpp
#include "sgcl/immutable/map.h"   // or "sgcl/immutable/immutable.h", "sgcl/sgcl.h"

namespace sgcl::immutable {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map;
}
```

`sgcl::immutable::map<Key, T, Hash, KeyEqual>` is the immutable hash map (the persistent map of Clojure and Scala): a map every `insert` and `erase` of which returns a new map and leaves the old one exactly as it was, the two sharing everything but the path that changed. It is a hash array mapped trie, Bagwell's (*Ideal Hash Trees*, 2001) as Clojure's `PersistentHashMap` has it: a node has 32 slots, one per value of the five bits of the hash its level consumes, and stores only the slots in use, packed in slot order behind a bitmap, so that the entry of a slot is at the population count of the bits below it; an entry holds an element or the subtrie of the elements whose hashes agree with it so far, and two elements with the same hash to the last bit hang off one entry as a chain. A lookup walks log32(*n*) nodes, four for a million elements, and compares the key once; an `insert` or an `erase` copies those nodes, a few hundred bytes, and shares the rest, and a subtrie left with one element is folded into its parent so that a map erased down to nothing holds no node at all. Nothing is ever modified: a map held by any number of threads is read by all of them without a lock, and a version is published, and replaced by the next, through a [copy_on_write](../concurrent/copy_on_write.md) or an [atomic](../core/atomic.md) ([README: The structures](README.md#the-structures)).

The map is two words and its function objects: the size and a `tracked_ptr` to the root. A copy of it is a copy of those words. The nodes are managed objects that no version owns, made in six sizes (room for 1, 2, 4, 8, 16 or 32 entries) so that each is one object with a destructor for its elements: a node reached by ten versions is one node, and the collector destroys its elements and frees it once the last version that reaches it is dropped, the accounting a persistent map without a collector does with a reference count per node. `find` hands back a pointer to the value, null when the key is absent: the value is in a node some version holds, and it is valid for as long as one does.

## Rules

- `sgcl::immutable::map` holds its root by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). Its iterators, and the pointers `find` hands back, hold nothing alive: valid while the map object they came from exists, as `std`'s are.
- Every member is `const`. `insert`, `emplace` and `erase` return the new map; the one they were called on is unchanged, and stays so for as long as it is held. The elements are `const` through the map.
- A change copies the elements of the nodes on the path, up to 32 per node: the copy constructors of `Key` and `T` are what a change costs, plus the nodes.
- A key or a value holding tracked pointers is traced where it lives, in a node; one with a destructor is destroyed when the collector frees its node, once no version reaches it. Nothing is destroyed by an `erase`: the old version still holds the element.
- With a transparent hash and equality (`is_transparent`, as `std::hash` and `std::equal_to` of a [string](../core/string.md) are) the lookups take a key of another type and build none: a `string_view` or a literal finds a `string` key with no string made for the search.
- Sharing between threads: any number of threads read any version. A `map` variable that one thread replaces while others read it is the one thing that needs synchronization, and `concurrent::copy_on_write<map<Key, T>>` is the shape for it: `load()` is one atomic load for a snapshot, `update(f)` copies two words, applies `f` (an `insert`, an `erase`) and swings the pointer, so an update costs O(log *n*) where a `concurrent::copy_on_write<sgcl::map<Key, T>>`, over the mutable map, costs a copy of everything. An `atomic<tracked_ptr<map<Key, T>>>` does the same with the version in a managed object of its own.
- The order of iteration is the trie's, the bits of the hashes; it changes with nothing but the elements.

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using value_type = pair<const Key, T>;   // pair, the alias of std::pair (sgcl/core/aliases.h)
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using difference_type = ptrdiff_t;
using const_iterator = /* forward iterator over const value_type */;
using iterator = const_iterator;
```

### Constructors

```cpp
map();
explicit map(const Hash& hash, const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt> map(InputIt first, InputIt last, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
map(std::initializer_list<value_type> ilist, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
map(const map&) noexcept;
map& operator=(const map&) noexcept;
```

An empty map holds no node at all. A range or a list is built at once, not by an insert per element: the elements are taken into a buffer, sorted by their hashes in the trie's order (the low chunk first, as the trie consumes it), and the trie is made from the root down, every node allocated once at its size and every entry constructed once; a key that occurs twice keeps its first occurrence, as an insert keeps what it finds (and as a `std::unordered_map` made of the range does). 90 ns per element for 200,000 random `long` keys, against 700 for the inserts that would build the same map (the benchmarks page).

```cpp
immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
std::map<int, int> squares = {{1, 1}, {2, 4}, {3, 9}};
immutable::map from_range(squares.begin(), squares.end());   // deduced: map<int, int>
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

### find, try_get, contains, count, at

```cpp
const_iterator find(const Key& key) const;        // end() when absent; ++ goes on in begin()'s order
const T* try_get(const Key& key) const;             // the value, null when absent (mixin::lookup), cheaper than an iterator
bool contains(const Key& key) const;
size_type count(const Key& key) const;
const T& at(const Key& key) const;              // out_of_range when absent
template<class K> const_iterator find(const K& key) const;   // when Hash and KeyEqual are transparent: and try_get, contains, count, at, erase
```

`find` is the element as an iterator, as every `find` of the library; `try_get` the value under the key, or null: log32(*n*) nodes walked, the key compared once (once per element of a chain when hashes collide); 13 ns for a random key of a hundred thousand `int`s.

```cpp
immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
if (auto p = ports.try_get("https")) {       // a literal: transparent, no string made
    std::cout << *p << '\n';
}
string url = "ftp://host";
bool known = ports.contains(url.as_slice(0, 3)); // false: a view of another string, nothing built
```

### insert, set, emplace

```cpp
map insert(const Key& key, const T& value) const;
map insert(const Key& key, T&& value) const;
map insert(Key&& key, const T& value) const;
map insert(Key&& key, T&& value) const;
map insert(const value_type& value) const;
map set(const Key& key, const T& value) const;   // and the other three forms of insert
template<class... A> map emplace(const Key& key, A&&... a) const;
```

`insert` is the map with `value` under `key` when the key is absent, and the same map when it is there, as every insert of the library keeps what it finds; `set` puts `value` in place of the value there, or adds it (vector's `set`, the mutable maps' `insert_or_assign`). Either copies the nodes on the path, log32(*n*) of them, and shares the rest. `emplace` builds the value from the arguments, as `insert` when the key is there. 590 ns per insert while building a hundred thousand `int`s, 750 ns replacing values in the built map.

```cpp
immutable::map<string, int> ports = {{"http", 80}};
auto more = ports.insert("https", 443);      // ports has one element, more has two
auto kept = more.insert("http", 8080);       // the same map: http is there
auto changed = more.set("http", 8080);       // more still says 80
auto built = ports.emplace("ssh", 22);
```

### erase

```cpp
map erase(const Key& key) const;
template<class K> map erase(const K& key) const;   // when Hash and KeyEqual are transparent
```

The map without the element under the key: the path copied, a node emptied dropped, a subtrie left with one element folded into its parent; the same map, sharing everything, when the key is absent. 670 ns per erase over a hundred thousand `int`s.

```cpp
immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
auto fewer = ports.erase("http");            // ports still has both
```

### thaw, builder, freeze

```cpp
builder thaw() const;   // a builder over this map: nothing copied until it changes

class builder {         // map::builder: moves, never copies
public:
    builder();
    explicit builder(const Hash& hash, const KeyEqual& equal = KeyEqual());
    size_type size() const noexcept;
    bool empty() const noexcept;
    const T* try_get(const Key& key) const;       // valid until the builder's next change
    bool contains(const Key& key) const;          // try_get, contains, erase: a K as well when transparent
    bool insert(const Key& key, const T& value);  // and the other three forms of map's insert: when absent; true when added
    bool set(const Key& key, const T& value);     // and the other three forms: added or in place; true when added
    bool insert(const value_type& value);
    template<class... A> bool emplace(const Key& key, A&&... a);
    bool erase(const Key& key);                   // false when the key was absent
    map freeze();                                 // the map of what it holds; the builder goes on
};
```

A map changed in place, one element at a time, and frozen into a map when it is done: Clojure's transient, immer's. `thaw()` is the builder over the map's trie, which the two share, and copies nothing. An `insert` or an `erase` changes in place the nodes the builder made, and a node it shares with a map it copies once, the first time a change goes through it: where the map's own `insert` copies the whole path every time and leaves the old one to the collector, the builder copies a node once and changes it after. `freeze()` hands out the map of what the builder holds, and the builder goes on; its next change copies again whatever the map now shares, so the map is a value like any other and nothing the builder does afterwards reaches it. Over 200,000 random `long` keys: built through a builder, 88 ns per element, against 455 for an insert a version each and 144 for immer's transient; an edit of a tenth of the keys set and a tenth erased, 137 ns per change against 559 made a version each ([benchmarks](benchmarks.md)).

- A builder is one thread's, and it moves but does not copy: two builders would change one node. It lives where a `tracked_ptr` may. A builder moved from is empty.
- `find` hands back a pointer valid until the builder's next change, which may move the element within its node.
- An element added or taken out moves the elements after it in its node, which needs a move of `pair<const Key, T>` that cannot throw. A key whose copy can throw (a `std::string`: the key is `const` in the pair, so it is copied, not moved) makes the builder copy that one node instead of moving its elements, and still never the path above it.
- Nothing needs a builder: a map built at once from a range is the constructor's, 92 ns per element. The builder is for a map made or changed one element at a time, with conditions between, or for many changes to a large map at once.

```cpp
immutable::map<string, int> stock = {{"apples", 3}, {"pears", 0}};
auto b = stock.thaw();
for (auto& [item, count] : stock) {
    if (count == 0) {
        b.erase(item);
    }
}
b.set("plums", 12);
auto restocked = b.freeze();                  // stock still has its two
```

### Comparison

```cpp
friend bool operator==(const map& a, const map& b);
friend bool operator!=(const map& a, const map& b);
```

The same keys with equal values, whatever the two share.

### The mixins

`immutable::map` carries [mixin::immutable](../core/mixin/immutable.md), [mixin::enumerable](../core/mixin/enumerable.md) over its pairs (`contains` and `find` its own, by the key) and [mixin::lookup](../core/mixin/lookup.md), the reads by the key over its `find` — `get`, `try_get`, `value_or`, `contains_key`, `keys`, `values` ([the mixins](../core/mixin/README.md)).

```cpp
immutable::map<int, int> m = immutable::map<int, int>().insert(1, 10);
assert(m.contains(1) && m.exists([](const auto& kv) { return kv.second == 10; }));
assert(*m.get(1) == 10 && !m.get(2) && m.value_or(2, 0) == 0 && m.contains_key(1));
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A configuration read on every request by many threads and changed
// once in a while by one: the readers take a snapshot, the writer
// publishes a new version that shares all but one path with the old
// one, and nobody copies a map or takes a lock
int main() {
    concurrent::copy_on_write<immutable::map<string, int>> limits(immutable::map<string, int>{{"connections", 100}, {"requests", 1000}});
    atomic stop = false;
    atomic<long> reads = 0, inconsistent = 0;
    vector<thread> readers;
    for (int r : range(4)) {
        readers.emplace_back([&] {
            while (!stop) {
                auto snapshot = limits.load();                      // one load: this version, for as long as snapshot lives
                auto c = snapshot->try_get("connections");
                auto q = snapshot->try_get("requests");
                inconsistent += !c || !q || *q != 10 * *c;          // the writer keeps the ratio: a snapshot is whole
                ++reads;
            }
        });
    }
    for (int i : range(1, 101)) {
        limits.update([i](auto& m) {                                // two paths copied, the rest of the map shared
            m = m.set("connections", 100 * i).set("requests", 1000 * i);
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

On an Apple M-series core, `-O2`, `map<long, long>` of 200,000 random keys (`bench_immutable`): `insert` 480 ns a version each (`std::unordered_map`: 52), `find` 24 ns (`std::unordered_map`: 10), the constructor from a range 91 ns per element; against [immer](https://github.com/arximboldi/immer)'s map, the same trie over atomic reference counts, 452, 16 and 154 ([Benchmarks](benchmarks.md)). An insert or an erase copies four nodes, three of them full: the links copied without the barrier and each source node shaded once ([tracked_ptr: shade](../core/tracked_ptr.md#shade-storep-barrieroff)), the elements as any copy, which is where the time goes; the find's gap to immer is the entry holding its link and its element apart, a load more per level. One version of a hundred thousand `int` pairs is 2.99 MB, 30 bytes per element; a second version differing in one value adds 1.6 KB, a third with one more element 1.7 KB.

## See also

- [immutable::set](set.md), the same trie with the key as the element; [immutable::vector](vector.md)
- [copy_on_write](../concurrent/copy_on_write.md), how a version is published to other threads; [atomic](../core/atomic.md)
- [map](../core/map.md), the mutable one; [concurrent::map](../concurrent/map.md), the one many threads change in place
- [Benchmarks](benchmarks.md): insert, find and the range constructor against `std::map`
- [README: The structures](README.md#the-structures), [README: The rules](../core/README.md#the-rules)
