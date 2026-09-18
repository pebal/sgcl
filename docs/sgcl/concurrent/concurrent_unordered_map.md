# sgcl::concurrent_unordered_map

```cpp
#include "sgcl/concurrent/concurrent_unordered_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class concurrent_unordered_map;
}
```

The same class in the `Sgcl` interface: [ConcurrentDictionary](../Sgcl/Concurrent/ConcurrentDictionary.md).

`sgcl::concurrent_unordered_map<Key, T, Hash, KeyEqual>` is a lock-free hash map shared by any number of threads: the split-ordered list of Shalev and Shavit (*Split-Ordered Lists: Lock-Free Extensible Hash Tables*, 2006), the structure that makes a resizable lock-free hash table possible, what Java's `ConcurrentHashMap` is for. Every element sits in one sorted singly linked list (Harris and Michael's, with marker nodes for the deletions as in [concurrent_map](concurrent_map.md)), ordered by the bit reversal of its hash; an array of buckets points into that list at dummy nodes, one per bucket, made on the bucket's first use and inserted after the parent bucket's dummy (the bucket's number with its highest bit cleared). In split order the elements of bucket *i* of an array of *n* follow its dummy and precede the dummy of the bucket that *i* splits into at *2n*, so doubling the array moves no node: the new array gets the old slots copied and the rest made on use, the list stays what it was, and the old array is garbage once nothing walks it. In C++ without a collector that old array, and every node a walk may be standing on, is exactly what a reclamation scheme has to guard; here it is nothing ([README: Lock-free containers](README.md#lock-free-containers)). The interface has the names of `std::unordered_map`, restricted to what a lock-free map can offer: `find`, `contains`, `count`, `insert`, `emplace`, `try_emplace`, `erase`, `clear`, `size`, `empty`, `bucket_count`, `reserve`, iteration. There is no `operator[]`, no `at`, no `insert_or_assign`, no node handles and no iteration of one bucket.

## Rules

- The container holds its bucket array, its head node and its counters by `tracked_ptr`s, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). Its iterators hold their node by a `tracked_ptr`.
- `insert`, `emplace`, `try_emplace` and `erase` are lock-free and linearizable: an insertion takes effect at the compare-exchange that links the node into the list, an erasure at the one that marks it. `find`, `contains` and `count` are wait-free and never write. A key given by rvalue is looked up first and moved only into a node of its own; when another thread inserts the same key between that look and the link, the key has been moved into a node that is dropped: the one case where the standard's promise of no move for a key already there does not hold.
- The count of the elements is striped over cache lines (Java's `LongAdder`): `size()` is the sum, a snapshot of no particular moment under concurrent modification, exact once the threads are quiet. The array doubles once the elements outnumber the buckets (a load factor of one), by the insertion that notices; `reserve(n)` doubles it up front.
- Iteration is weakly consistent, as Java's: an iterator is valid whatever the other threads do, it skips the elements erased since it passed them and may or may not see the ones inserted meanwhile; the order is the list's, the bit reversal of the hashes, and it changes with nothing but the elements. The element an iterator addresses stays alive for as long as the iterator does, erased or not.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the erase, which other threads may be reading it across ([concurrent_map](concurrent_map.md) has the same rule).
- The dummies of the buckets used stay for the life of the container, one node each; a bucket initialized by two threads at once during a growth of the array gets two, which the list takes in its stride.
- The bucket is the low bits of the hash, and `std::hash` is the identity for integers and pointers, so keys with nothing in their low bits (multiples of a power of two, page-aligned addresses) crowd into a few buckets whatever the size of the array: give such keys a hash that mixes. Keys whose low bits vary (ids, counters, ordinary pointers) are best left as they are: neighbouring keys keep neighbouring nodes, which a mixing hash would scatter (a find of a sequential key 25 ns against 60 mixed, measured).
- Non-copyable, non-movable: a shared structure has one place.

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
using iterator = /* forward iterator over value_type */;
using const_iterator = /* forward iterator over const value_type */;
```

### Constructors

```cpp
concurrent_unordered_map();
explicit concurrent_unordered_map(size_type buckets, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt> concurrent_unordered_map(InputIt first, InputIt last, size_type buckets = 16, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
concurrent_unordered_map(std::initializer_list<value_type> ilist, size_type buckets = 16, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
concurrent_unordered_map(const concurrent_unordered_map&) = delete;
```

An empty map with 16 buckets (`buckets` rounded up to a power of two when given), or one built at once from a range or a list: as many buckets as the elements from the start, the elements sorted by their split keys (the first of two with one key kept, as `insert` keeps it) and the list linked in one pass with the dummies of the buckets in use merged in at their keys, a store each and no search; 90 to 120 ns per element for 200,000 random keys against 140 to 270 by the inserts.

```cpp
sgcl::concurrent_unordered_map<int, sgcl::tracked_ptr<Session>> sessions;   // a global: sgcl::
sgcl::concurrent_unordered_map<sgcl::string, int> counts(1 << 16);      // 65536 buckets from the start
```

### begin, end, cbegin, cend, empty, size, bucket_count, reserve, hash_function, key_eq

```cpp
iterator begin() noexcept;
const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
iterator end() noexcept;
const_iterator end() const noexcept;
const_iterator cend() const noexcept;
bool empty() const noexcept;
size_type size() const noexcept;
size_type bucket_count() const noexcept;
void reserve(size_type count);
hasher hash_function() const;
key_equal key_eq() const;
```

`empty` is a step from the head of the list; `size` the sum of the stripes; `reserve` grows the array to at least `count` buckets.

### find, contains, count

```cpp
iterator find(const Key& key) noexcept;
const_iterator find(const Key& key) const noexcept;
bool contains(const Key& key) const noexcept;
size_type count(const Key& key) const noexcept;
template<class K> iterator find(const K& key) noexcept;        // when Hash and KeyEqual are transparent: and const, contains, count
```

The element under `key`, or `end()`; wait-free once the bucket has its dummy node, which the first lookup or insertion into the bucket after the array grew makes (an allocation and a compare-exchange, lock-free, once per bucket for the array's life): from the bucket's dummy along the list to the element. (A lookup walked from the nearest initialized ancestor's dummy before, writing nothing, and with the array doubled late in a run of insertions and then only looked up, that walk was a large part of the list: a find of 6 ns took 7000, measured with 200,000 keys inserted by 32 threads.) With a transparent hash and equality (`is_transparent`, as `std::hash` and `std::equal_to` of a [string](../core/string.md) are) the lookups take a key of another type and build none: a `string_view` or a literal finds a `string` key with no string made for the search.

### insert, emplace, try_emplace

```cpp
pair<iterator, bool> insert(const value_type& value);
pair<iterator, bool> insert(value_type&& value);
template<class P> pair<iterator, bool> insert(P&& value);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<value_type> ilist);
template<class... A> pair<iterator, bool> emplace(A&&... a);
template<class... A> pair<iterator, bool> try_emplace(const Key& key, A&&... a);
template<class... A> pair<iterator, bool> try_emplace(Key&& key, A&&... a);
```

Inserts an element unless its key is taken: the element and `true`, or the one already there and `false`, as `std::unordered_map`. `emplace` builds the element first, in a node of its own, and drops the node when the key turns out to be taken; `try_emplace` searches once and builds nothing when the key is there: the element is built from the key and `a...` only when it is absent, and linked where that search found its place. A concurrent insertion of the same key wins or loses at the compare-exchange: exactly one returns `true`.

```cpp
auto [it, inserted] = sessions.try_emplace(42, sgcl::make_tracked<Session>());
```

### erase, clear

```cpp
size_type erase(const Key& key);
template<class K> size_type erase(const K& key);   // when Hash and KeyEqual are transparent
iterator erase(const_iterator pos);
void clear();
```

Erases the element under `key` (1 or 0 erased), or the one `pos` addresses if it is still there (some element after it in the list, or `end()`, is returned); `clear` erases every element there is at the time of the walk. The node is marked, which is where one thread wins when several erase the same element, then unlinked by a search; the element lives on while an iterator holds its node.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A word count over many threads: every thread inserts or increments,
// nobody locks, the table doubles under them as it fills
struct Count {
    sgcl::atomic<int> n = 0;
};

int main() {
    sgcl::concurrent_unordered_map<sgcl::string, sgcl::tracked_ptr<Count>> counts;
    sgcl::vector<sgcl::thread> threads;
    for (int t : sgcl::range(8)) {
        threads.emplace_back([&, t] {
            for (int i : sgcl::range(10000)) {
                sgcl::string word = "w" + sgcl::to_string((i * 8 + t) % 1000);
                auto [it, fresh] = counts.try_emplace(word, sgcl::make_tracked<Count>());   // one Count per word, whoever gets there first
                ++it->second->n;
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    int total = 0;
    for (auto& [word, count] : counts) {
        total += count->n;
    }
    std::cout << counts.size() << " words, " << total << " occurrences, " << counts.bucket_count() << " buckets\n";
    return counts.size() == 1000 && total == 80000 ? 0 : 1;
}
```

The output of one run (the buckets depend on how the threads interleave):

```
1000 words, 80000 occurrences, 256 buckets
```

## See also

- [concurrent_unordered_set](concurrent_unordered_set.md), the same table with the key as the element
- [concurrent_map](concurrent_map.md), the ordered map (a skip list), and the marker nodes both share
- [unordered_map](../containers/unordered_map.md), the sequential map with the full `std::unordered_map` interface
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
