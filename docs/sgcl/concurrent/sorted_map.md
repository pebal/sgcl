# sgcl::concurrent::sorted_map

```cpp
#include "sgcl/concurrent/sorted_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_map;
}
```

`sgcl::concurrent::sorted_map<Key, T, Compare>` is a lock-free sorted map shared by any number of threads: a skip list with the algorithm of Herlihy and Shavit (*The Art of Multiprocessor Programming*, the lock-free skip list), the structure Java's `ConcurrentSkipListMap` is. The bottom level is a sorted singly linked list holding every element; each node also stands in a random number of the levels above (a quarter of the nodes of one level in the next), so that a search descends from the top level in logarithmic time and finishes on the bottom list, which alone decides what the map holds. A logical deletion is a marker node linked after the deleted one at each of its levels, Java's way of marking a link without stealing a bit from the pointer (which the collector's pointer maps could not follow); the marked node is unlinked by the next search that passes it. No node is reused while a thread holds it, so there is no ABA and no reclamation scheme ([README: Lock-free containers](README.md#lock-free-containers)). The interface has the names of `std::map`, restricted to what a lock-free map can offer: `find`, `contains`, `count`, `lower_bound`, `upper_bound`, `insert`, `emplace`, `try_emplace`, `erase`, `clear`, `size`, `empty`, iteration in key order. There is no `operator[]`, no `at`, no `insert_or_assign` and no node handles: an element's mapped value is replaced through the reference the iterator gives, which is the program's own race to manage, as in Java.

A node is one managed object: the bottom link and the element, then the links of its upper levels, as many as its height, so that a search steps through one object per node at any level.

## Rules

- The container holds the head node (a sentinel of the maximum height, no element) by a `tracked_ptr` and the number of levels in use, so it lives where a `tracked_ptr` may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). Its iterators hold their node by a `tracked_ptr` and live where the map may.
- `insert`, `emplace`, `try_emplace` and `erase` are lock-free and linearizable: an insertion takes effect at the compare-exchange that links the node into the bottom list, an erasure at the one that marks it there. `find`, `contains`, `count`, `lower_bound` and `upper_bound` are wait-free and never write. A key given by rvalue is looked up first and moved only into a node of its own; when another thread inserts the same key between that look and the link, the key has been moved into a node that is dropped: the one case where the standard's promise of no move for a key already there does not hold.
- Iteration is weakly consistent, as Java's: an iterator is valid whatever the other threads do, it skips the elements erased since it passed them, and it may or may not see the ones inserted meanwhile. The element an iterator addresses stays alive for as long as the iterator does, erased or not: its key is immutable, its mapped value is what the threads make of it.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the erase, which other threads may be reading it across. This is the one container of the library whose elements outlive their erasure ([Containers](../core/README.md#containers)); an element that must be released promptly is held by a `tracked_ptr` whose object does its own cleanup, or watched by an [expiry_queue](../core/expiry_queue.md).
- `size()` counts the elements: linear, and a snapshot of no particular moment when other threads modify the map, as Java's `size` is. `empty()` is a step from the head.
- A `tracked_ptr` may not address an element or a node ([The rules](../core/README.md#the-rules), 4); an iterator holds the node, and a reference or a raw pointer to an element is valid while some iterator or the map holds it.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using value_type = pair<const Key, T>;   // pair, the alias of std::pair (sgcl/core/aliases.h)
using key_compare = Compare;
using size_type = size_t;
using difference_type = ptrdiff_t;
using iterator = /* forward iterator over value_type */;
using const_iterator = /* forward iterator over const value_type */;
```

An iterator is a forward iterator holding its node by a `tracked_ptr<...>` word; `*it` is a `value_type&` (a `const value_type&` for a `const_iterator`), `it->first` the key, `it->second` the mapped value. An `iterator` converts to a `const_iterator`.

### Constructors

```cpp
sorted_map();
explicit sorted_map(const Compare& comp);
template<std::input_iterator InputIt> concurrent::sorted_map(InputIt first, InputIt last, const Compare& comp = Compare());
concurrent::sorted_map(std::initializer_list<value_type> ilist, const Compare& comp = Compare());
sorted_map(const concurrent::sorted_map&) = delete;
```

An empty map (the head node on the managed heap), or one built at once from a range or a list: the elements sorted (the first of two with one key kept, as `insert` keeps it) and every node linked behind the last at each of its levels, a store each and no search, which is what the inserts would cost, a search each; 110 to 200 ns per element for 200,000 random keys against 380 to 600 by the inserts.

```cpp
concurrent::sorted_map<int, tracked_ptr<Session>> sessions;              // a global: 
concurrent::sorted_map<string, int, std::greater<string>> by_name_desc;
concurrent::sorted_map<int, int> m = {{1, 10}, {2, 20}};
```

### begin, end, cbegin, cend

```cpp
iterator begin() noexcept;
const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
iterator end() noexcept;
const_iterator end() const noexcept;
const_iterator cend() const noexcept;
```

The elements in key order, weakly consistent; `end()` holds no node.

```cpp
for (auto& [id, session] : sessions) {   // a walk while other threads insert and erase
    session->touch();
}
```

### empty, size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
```

### find, contains, count

```cpp
iterator find(const Key& key) noexcept;
const_iterator find(const Key& key) const noexcept;
bool contains(const Key& key) const noexcept;
size_type count(const Key& key) const noexcept;
template<class K> iterator find(const K& key) noexcept;        // when Compare::is_transparent: and const, contains, count
```

The element under `key`, or `end()`; wait-free. With a transparent comparator (`is_transparent`, as `std::less` of a [string](../core/string.md) is) the lookups take a key of another type and build none: a `string_view` or a literal finds a `string` key with no string made for the search.

```cpp
if (auto it = sessions.find(42); it != sessions.end()) {
    it->second->touch();                 // the node is held by `it` for as long as it exists
}
```

### lower_bound, upper_bound

```cpp
iterator lower_bound(const Key& key) noexcept;
const_iterator lower_bound(const Key& key) const noexcept;
iterator upper_bound(const Key& key) noexcept;
const_iterator upper_bound(const Key& key) const noexcept;
template<class K> iterator lower_bound(const K& key) noexcept;   // when Compare::is_transparent: and const, upper_bound
```

The first element whose key is not less than `key`, and the first whose key is greater; wait-free; with a transparent comparator, by a key of another type.

```cpp
for (auto it = m.lower_bound(10); it != m.end() && it->first < 20; ++it) {   // the keys in [10, 20)
    std::cout << it->first << "\n";
}
```

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

Inserts an element unless its key is taken: the element and `true`, or the one already there and `false`, as `std::map`. `emplace` builds the element from `a...` first, in a node of its own, and drops the node when the key turns out to be taken (the collector reclaims it); `try_emplace` searches once and builds nothing when the key is there: the element is built from the key and `a...` only when it is absent, and linked between the neighbours that search found (an insertion of a key that may be present costs one search, not a lookup and then one, which is what `emplace` cannot do, its key being inside the element it builds). A concurrent insertion of the same key wins or loses at the compare-exchange on the bottom list: exactly one of them returns `true`.

```cpp
auto [it, inserted] = sessions.try_emplace(42, make_tracked<Session>());
if (!inserted) {
    // another thread's session under 42: it->second
}
```

### erase

```cpp
size_type erase(const Key& key);
template<class K> size_type erase(const K& key);   // when Compare::is_transparent
iterator erase(const_iterator pos);
```

Erases the element under `key` (1 or 0 erased), or the element `pos` addresses if it is still there (the next element in key order is returned). The node is marked at each of its levels, the bottom one last, which is where one thread wins when several erase the same element, then unlinked by a search; the element itself lives on for as long as some iterator holds its node, and dies with the node at a sweep.

```cpp
sessions.erase(42);
for (auto it = sessions.begin(); it != sessions.end();) {
    it = it->second->expired() ? sessions.erase(it) : std::next(it);
}
```

### clear

```cpp
void clear();
```

Erases every element there is at the time of the walk.

### key_comp

```cpp
key_compare key_comp() const;
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A registry shared by writers and readers: writers insert and erase,
// readers look up and walk, nobody locks and nobody frees
struct Entry {
    int id;
    int hits = 0;
};

int main() {
    concurrent::sorted_map<int, tracked_ptr<Entry>> registry;   // could as well be a global
    atomic<long> found = 0;
    vector<thread> threads;
    for (int w : range(4)) {
        threads.emplace_back([&, w] {
            for (int i : range(250)) {
                int id = i * 4 + w;                              // 1000 keys between the four writers
                registry.try_emplace(id, make_tracked<Entry>(id));
            }
            for (int id = w; id < 1000; id += 8) {
                registry.erase(id);                              // half of this writer's keys taken out again
            }
        });
        threads.emplace_back([&] {
            for (int i : range(10000)) {
                if (auto it = registry.find(i % 1000); it != registry.end()) {
                    ++it->second->hits;                          // the entry lives while `it` does, erased or not
                    ++found;
                }
            }
            long walked = 0;
            for (auto& [id, entry] : registry) {                 // weakly consistent: sorted, live at the time
                walked += entry->id == id;
            }
            found += walked > 0;
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << registry.size() << " entries, " << found << " lookups hit\n";
    return registry.size() == 500 ? 0 : 1;
}
```

The output of one run (the hits depend on how the threads interleave):

```
500 entries, 16483 lookups hit
```

## See also

- [concurrent::queue](queue.md), [concurrent::stack](stack.md) for the lock-free sequences
- [sorted_map](../core/sorted_map.md), the sequential map with the full `std::map` interface
- [atomic](../core/atomic.md), what every link is; [expiry_queue](../core/expiry_queue.md) for a cleanup when an erased element's node dies
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
