# sgcl::ordered_map

```cpp
#include "sgcl/containers/ordered_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class ordered_map;
}
```

`sgcl::ordered_map<Key, T, Hash, KeyEqual>` is a hash map iterated in the order its elements were inserted: what Java's `LinkedHashMap`, the `dict` of Python and .NET's `OrderedDictionary` are, and `std` has not. It is [unordered_map](unordered_map.md) with one thing added: every node is on a second list, in insertion order, threaded through the sentinel. `begin()` to `end()` walks that list, both ways (the iterators are bidirectional, `rbegin` exists), `front()` is the oldest element and `back()` the newest, a copy comes out in the same order, an erase takes the element out of the order, an insert of a key that is present leaves it where it was (`operator[]`, `insert`, `emplace`, `try_emplace`, `insert_or_assign` alike), and `to_back` and `to_front` move an element to the end or the start of the order. Everything else is `unordered_map`: the same table, so a lookup costs the same and the bucket interface, the hash policy, node handles, `merge`, the transparent lookups (a `string_view` for a [string](../core/string.md) key) are all there; a rehash relinks the chain and never touches the order. The price is two words more per node.

What it is for: what a hash map is for, where the order of the entries is part of the data or of the behaviour. An object read from JSON that is written back with its keys as they came; a configuration, a header list, a registry that reports in the order of registration; and a cache that evicts in an order: `to_back(it)` on a hit makes the element the newest, `erase(begin())` when full drops the oldest, which is an LRU cache in two lines (the example below). `unordered_map` is the right map when the order is nothing; [map](map.md) when it is the order of the keys.

## Rules

- An `ordered_map` holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1). The same holds for a node handle.
- The keys and the mapped values may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the map is destroyed, exactly as in `std`; a map dying in a sweep, inside a managed object nobody refers to any more, has its elements destroyed by the same sweep, on a collector thread ([unordered_map](unordered_map.md#rules)).
- An iterator, a reference or a pointer to an element is valid while the element is in the map, across insertions, rehashes, erasures of other elements, `to_back` and `to_front` of any element, `swap`, `merge` and a move of the map. An iterator to an erased element is invalid as in `std`. `end()` is the sentinel, so `--end()` is the last element; an `end()` taken from a map that has never had an element is null and is not decremented.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../core/README.md#the-rules), 6).

## Members

Every member of [unordered_map](unordered_map.md#members), with these differences and additions.

### Types

```cpp
using iterator = ...;  using const_iterator = ...;                  // bidirectional; a raw node pointer, as for unordered_map
using reverse_iterator = std::reverse_iterator<iterator>;  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

### Iterators

```cpp
iterator begin() noexcept;    const_iterator begin() const noexcept;    const_iterator cbegin() const noexcept;    // the oldest element
iterator end() noexcept;      const_iterator end() const noexcept;      const_iterator cend() const noexcept;      // the sentinel: --end() is the newest
reverse_iterator rbegin() noexcept;  const_reverse_iterator rbegin() const noexcept;  const_reverse_iterator crbegin() const noexcept;
reverse_iterator rend() noexcept;    const_reverse_iterator rend() const noexcept;    const_reverse_iterator crend() const noexcept;
```

The order of insertion, oldest first, unchanged by a rehash. The bucket interface (`begin(n)`, `end(n)`) walks the chain as in `unordered_map`.

```cpp
sgcl::ordered_map<sgcl::string, int> m;
m["c"] = 1;
m["a"] = 2;
m["b"] = 3;
m["a"] = 4;                                   // present: the value changes, the place does not
sgcl::string keys;
for (const auto& [key, value] : m) {
    keys = keys + key;                        // "cab", always
}
for (auto it = m.rbegin(); it != m.rend(); ++it) {
    keys = keys + it->first;                  // then "bac"
}
```

### front, back

```cpp
value_type& front() noexcept;        const value_type& front() const noexcept;   // the oldest element
value_type& back() noexcept;         const value_type& back() const noexcept;    // the newest
```

The map must not be empty.

### to_back, to_front

```cpp
void to_back(const_iterator pos) noexcept;    // the element becomes the newest
void to_front(const_iterator pos) noexcept;   // the oldest
```

Moves the element at `pos` to the end (the start) of the order, as if it had been inserted last (first) from now on; nothing else changes, the iterator stays valid, O(1). The element that is last (first) already is left alone: a load, no relink, so a cache whose hits go mostly to its newest element pays nothing for them.

```cpp
sgcl::ordered_map<sgcl::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
m.to_back(m.find("a"));                       // b c a
m.to_front(m.find("c"));                      // c b a
assert(m.front().first == "c" && m.back().first == "a");
```

### insert, emplace, try_emplace, insert_or_assign, operator[]

As in [unordered_map](unordered_map.md#insert): a new key goes to the end of the order; a present key stays where it is, whatever happens to its value.

### erase, take, extract, clear, erase_if

As in [unordered_map](unordered_map.md#erase): the element leaves the order too (`take` hands its value back); `erase(pos)` returns the iterator after `pos` in the order (`end()` for the newest); `erase(first, last)` is a range of the order. An extracted node is out of the order and goes to the end of it when inserted again, here or in another map.

### merge

As in [unordered_map](unordered_map.md#merge), from another `ordered_map` with the same `Key` and `T` (an `unordered_map`'s nodes are of another shape, without the order's words: the call does not compile): the nodes taken from the source are appended to this map's order, in the source's chain order; those left in the source keep their places there.

### Copy, swap, comparison

A copy (the constructor, `operator=`) reproduces the order. `swap` and a move carry it along. `operator==` compares the contents and ignores the order, as `LinkedHashMap.equals` does: two maps with the same pairs are equal whatever the order they came in.

### The mixins

`ordered_map` carries [m_enumerable](../core/mixin/m_enumerable.md) (over the pairs, in insertion order; `contains` its own) and [m_lookup](../core/mixin/m_lookup.md) ([the mixins](../core/mixin/README.md)).

```cpp
sgcl::ordered_map<sgcl::string, int> m = {{"b", 2}, {"a", 1}};
assert(m.value_or("c", 0) == 0 && m.index_of(std::pair<const sgcl::string, int>{"a", 1}) == 1);   // the position in insertion order
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A cache of `capacity` entries that drops the least recently used one:
// an ordered_map, the oldest first, touched entries moved to the back
struct Cache {
    size_t capacity;
    sgcl::ordered_map<sgcl::string, sgcl::string> entries;

    const sgcl::string* get(std::string_view key) {
        auto it = entries.find(key);              // a string_view: no string made for the lookup
        if (it == entries.end()) {
            return nullptr;
        }
        entries.to_back(it);                      // the newest now
        return &it->second;
    }

    void put(sgcl::string key, sgcl::string value) {
        auto [it, inserted] = entries.insert_or_assign(std::move(key), std::move(value));
        if (!inserted) {
            entries.to_back(it);
        } else if (entries.size() > capacity) {
            entries.erase(entries.begin());       // the oldest goes
        }
    }
};

int main() {
    Cache cache{2};
    cache.put("a", "1");
    cache.put("b", "2");
    cache.get("a");                               // a is newer than b now
    cache.put("c", "3");                          // full: b, the oldest, is dropped
    for (const auto& [key, value] : cache.entries) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << "\n" << (cache.get("b") ? "b kept" : "b evicted") << "\n";
    return 0;
}
```

The output:

```
a=1 c=3 
b evicted
```

## See also

- [ordered_set](ordered_set.md) for keys alone in insertion order, [unordered_map](unordered_map.md) for the same map without the order, [map](map.md) for the order of the keys
- [string](../core/string.md): a key looked up by a `string_view` or a literal
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules)
