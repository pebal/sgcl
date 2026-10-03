[sgcl](../README.md) › [core](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>

```cpp
#include "sgcl/core/ordered_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class ordered_map;
}
```

`sgcl::ordered_map<Key, T, Hash, KeyEqual>` is a hash map iterated in the order its elements were inserted: what
Java's `LinkedHashMap`, the `dict` of Python and .NET's `OrderedDictionary` are, and `std` has not. It is
[map](map.md) with one thing added: every node is on a second list, in the order of insertion, closed through
the sentinel. `begin()` to `end()` walks that list, both ways (the iterators are bidirectional, `rbegin` exists),
`front()` is the oldest element and `back()` the newest, a copy comes out in the same order, an erasure takes the
element out of the order, an insertion of a key that is there leaves it where it was (`operator[]`, `insert`,
`emplace`, `try_emplace` and `insert_or_assign` alike), and `to_back` and `to_front` move an element to the end
or the start of the order. Everything else is `map`: the same table, so a lookup costs the same, and the bucket
interface, the hash policy, the node handles, `merge` and the lookups by a key of another type (a `string_view`
for a [string](string.md) key) are all there; a rehash relinks the chain and never touches the order. The price
is two words more per node.

What it is for: what a hash map is for, where the order of the entries is part of the data or of the behaviour.
An object read from JSON that is written back with its keys as they came; a configuration, a header list, a
registry that reports in the order of registration; and a cache that evicts in an order: `to_back(it)` on a hit
makes the element the newest, `erase(begin())` when full drops the oldest, which is a cache of the least recently
used entries in two lines (the [example](#example) below). `map` is the right map when the order is nothing;
[sorted_map](sorted_map.md) when it is the order of the keys.

Where the memory lives is `map`'s layout. The map object holds two `tracked_ptr`s, the bucket array (a managed
array of node pointers) and the sentinel node, which is both the head of the chain and the end of the order, and
beside them the counts, the hash and the equality. Every element is a node on the managed heap; the nodes form
one chain linked by tracked pointers, as in libstdc++, a bucket pointing at the node before its first node, and
every node also links the one before and the one after it in the order. The whole structure is traced from the
sentinel, so an `ordered_map<Key, tracked_ptr<T>>`, or one inside a managed object, is traced like any other
managed data, and a cycle through it is collected like any other cycle. The hash of each key is cached in its
node: a rehash hashes nothing and a lookup compares hashes before keys. The bucket count is 0 or a power of two;
the table starts with no bucket array at all and grows when the size reaches `bucket_count() *
max_load_factor()`. A lookup, an iteration and a copy of an iterator read raw pointers only and pay no write
barrier; an insertion, an erasure, a rehash, `to_back` and `to_front` store tracked pointers and pay the barrier
on each link they change ([README: Containers](README.md#containers)).

## Rules

- An `ordered_map` holds tracked pointers, so it lives on a stack or inside a managed object: never in
  `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](README.md#the-rules), 1). The same holds for a [node handle](ordered_map-node_type.md).
- The keys and the values may be, or hold, tracked pointers (a `tracked_ptr` key is hashed by its address): the
  nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the map is destroyed, exactly as in
  `std`. The one exception is a map dying in a sweep, inside a managed object nobody refers to any more: its
  nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector
  thread ([README: Threads](../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the map, across
  insertions, rehashes, erasures of other elements, `to_back` and `to_front` of any element, `swap`, `merge` and a
  move of the map: it follows the node. An iterator to an erased element is invalid, as in `std`. Iterators are
  trivially copyable, one raw node pointer each, and may live anywhere, a `std::vector` included: the map roots
  every node it links.
- `end()` is the sentinel, so `--end()` is the newest element. A map that has never had a bucket array (made
  empty, nothing inserted yet) has no sentinel: its `end()` is null, is not decremented, and is not the `end()`
  of the map after its first insertion ([Iterator invalidation](#iterator-invalidation)).
- A `tracked_ptr` may point at an element of the map, or at a member of one (a node is a managed object,
  [The rules](README.md#the-rules), 4); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys: any object type that `Hash` hashes and `KeyEqual` compares. |
| `T` | The type of the values: any object type; `operator[]` asks for a default constructor, a copy of the map for a copy constructor. |
| `Hash` | A function object returning the `size_t` hash of a key. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `KeyEqual`, the lookups, `at`, `erase`, `extract`, `take` and `bucket` take a key of another type. |
| `KeyEqual` | A function object comparing two keys for equality, consistent with `Hash`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `value_type` | `std::pair<const Key, T>` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `reference` | `value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `value_type*` |
| `const_pointer` | `const value_type*` |
| `iterator` | a bidirectional iterator over `value_type` in the order of insertion, one raw node pointer, `std::bidirectional_iterator`; converts to `const_iterator` |
| `const_iterator` | the same over `const value_type` |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| `local_iterator` | a forward iterator over the elements of one bucket, in the order of the chain, `std::forward_iterator`; converts to `const_local_iterator` |
| `const_local_iterator` | the same over `const value_type` |
| [node_type](ordered_map-node_type.md) | the node handle |
| `insert_return_type` | a struct `{ iterator position; bool inserted; node_type node; }`, the result of `insert(node_type&&)` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ordered_map/ordered_map.md) | constructs the map |
| `(destructor)` | destroys the elements, in a sweep leaves them to the sweep; the nodes, the bucket array and the sentinel are left to the collector |
| [operator=](ordered_map/operator_assign.md) | assigns values to the map |

#### Element access

| Function | Description |
|---|---|
| [front](ordered_map/front.md) | the oldest element |
| [back](ordered_map/back.md) | the newest element |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](ordered_map/begin.md) | an iterator to the oldest element |
| [end, cend](ordered_map/end.md) | the iterator past the newest element |
| [rbegin, crbegin](ordered_map/rbegin.md) | a reverse iterator to the newest element |
| [rend, crend](ordered_map/rend.md) | the reverse iterator past the oldest element |

#### Capacity

| Function | Description |
|---|---|
| [empty](ordered_map/empty.md) | checks whether the map is empty |
| [size](ordered_map/size.md) | the number of elements |
| [max_size](ordered_map/max_size.md) | the largest number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [clear](ordered_map/clear.md) | erases every element |
| [insert](ordered_map/insert.md) | inserts elements or nodes |
| [insert_or_assign](ordered_map/insert_or_assign.md) | inserts an element, or assigns to the value under its key |
| [emplace](ordered_map/emplace.md) | constructs an element in place |
| [emplace_hint](ordered_map/emplace_hint.md) | constructs an element in place, with a hint |
| [try_emplace](ordered_map/try_emplace.md) | inserts an element built in place when the key is absent |
| [erase](ordered_map/erase.md) | erases elements |
| [take](ordered_map/take.md) | erases the element under a key and hands its value back |
| [swap](ordered_map/swap.md) | swaps the contents |
| [extract](ordered_map/extract.md) | takes a node out of the map |
| [merge](ordered_map/merge.md) | relinks the nodes of another map |
| [to_back](ordered_map/to_back.md) | moves an element to the end of the order |
| [to_front](ordered_map/to_front.md) | moves an element to the start of the order |

#### Lookup

| Function | Description |
|---|---|
| [at](ordered_map/at.md) | the value under a key, with a check |
| [operator[]](ordered_map/operator_at.md) | the value under a key, inserted when absent |
| [count](ordered_map/count.md) | the number of elements under a key |
| [find](ordered_map/find.md) | an iterator to the element under a key |
| [contains](ordered_map/contains.md) | checks whether a key is there |
| [equal_range](ordered_map/equal_range.md) | the range of the elements under a key |

#### Bucket interface

| Function | Description |
|---|---|
| [begin(size_type), cbegin(size_type)](ordered_map/begin.md) | a local iterator to the first element of a bucket |
| [end(size_type), cend(size_type)](ordered_map/end.md) | the local iterator past the last element of a bucket |
| [bucket_count](ordered_map/bucket_count.md) | the number of buckets |
| [max_bucket_count](ordered_map/max_bucket_count.md) | the largest number of buckets |
| [bucket_size](ordered_map/bucket_size.md) | the number of elements in a bucket |
| [bucket](ordered_map/bucket.md) | the bucket of a key |

#### Hash policy

| Function | Description |
|---|---|
| [load_factor](ordered_map/load_factor.md) | the average number of elements per bucket |
| [max_load_factor](ordered_map/max_load_factor.md) | the load factor at which the table grows, read or set |
| [rehash](ordered_map/rehash.md) | sets the number of buckets |
| [reserve](ordered_map/reserve.md) | makes room for a number of elements |

#### Observers

| Function | Description |
|---|---|
| [hash_function](ordered_map/hash_function.md) | a copy of the hash function |
| [key_eq](ordered_map/key_eq.md) | a copy of the equality of the keys |

#### From mixin::enumerable

The questions asked of the elements, the pairs, in the order of insertion
([mixin::enumerable](mixin/enumerable.md)); `contains` is the map's own, by the key.

| Function | Description |
|---|---|
| `index_of` | the position in the order of the first element equal to a pair |
| `last_index_of` | the position in the order of the last element equal to a pair |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::lookup

The reads by a key ([mixin::lookup](mixin/lookup.md)).

| Function | Description |
|---|---|
| `get` | a copy of the value under a key, `nullopt` when absent |
| `try_get` | a pointer to the value under a key, null when absent |
| `value_or` | a copy of the value under a key, or a fallback |
| `contains_key` | checks whether a key is there |
| `keys` | the keys, as a range in the order of insertion |
| `values` | the values, as a range in the order of insertion |
| `values_of` | the values under a key, as a range |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](ordered_map/operator_cmp.md) | compares the contents of two maps, whatever their orders |
| [swap](ordered_map/swap.md) | swaps the contents of two maps |
| [erase_if](ordered_map/erase_if.md) | erases the elements a predicate accepts |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<
             std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
         class KeyEqual = std::equal_to<
             std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
ordered_map(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> ordered_map<
           std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
           typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
ordered_map(std::initializer_list<pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> ordered_map<Key, T, Hash, KeyEqual>;
```

From an iterator pair over pairs, or from an initializer list of `pair`s spelled out: nested braces deduce
nothing, as with `std`.

## Complexity

- `find`, `contains`, `count`, `equal_range`, `at`, `operator[]`, `insert`, `emplace`, `try_emplace`,
  `insert_or_assign`, `erase` and `take` by a key, `extract`: constant on average, the walk of one bucket;
  linear in the size when every key falls into one bucket. An insertion is amortized over the growths of the
  table.
- `begin`, `end`, `front`, `back`, `to_back`, `to_front`, `size`, `empty`, a step of an iterator, `swap`:
  constant.
- `clear`, `rehash`, `reserve`, `erase_if`, a copy, `==`: linear in the size.

A node is allocated per element: the element, the link of the chain, the cached hash and the two links of the
order, two words more than a node of [map](map.md).

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `to_back`, `to_front`, `max_load_factor` | never |
| `rehash`, `reserve` | no iterator but an `end()` taken while the map had no bucket array, and the local ones (the last row) |
| `swap` | never: the iterators, `end()` included, follow their elements and the sentinel into the other map |
| `insert`, `emplace`, `emplace_hint`, `try_emplace`, `insert_or_assign`, `operator[]`, `merge` | no iterator but an `end()` taken while the map had no bucket array, and the local ones when the table grows (the last row) |
| `erase`, `take`, `extract`, `erase_if` | the erased elements only |
| `clear` | every element; not `end()` |
| `operator=` | always |
| a rehash, by `rehash`, `reserve` or an insertion that grows the table | the local iterators |

A move constructor takes the elements and the sentinel to the new map: the iterators into the old one, `end()`
included, are iterators into the new one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A cache of `capacity` entries that drops the least recently used one:
// an ordered_map, the oldest first, touched entries moved to the back
struct Cache {
    size_t capacity;
    ordered_map<string, string> entries;

    const string* get(std::string_view key) {
        auto it = entries.find(key);  // a string_view: no string made for the lookup
        if (it == entries.end()) {
            return nullptr;
        }
        entries.to_back(it);  // the newest now
        return &it->second;
    }

    void put(string key, string value) {
        auto [it, inserted] = entries.insert_or_assign(std::move(key), std::move(value));
        if (!inserted) {
            entries.to_back(it);
        } else if (entries.size() > capacity) {
            entries.erase(entries.begin());  // the oldest goes
        }
    }
};

int main() {
    Cache cache{2};
    cache.put("a", "1");
    cache.put("b", "2");
    cache.get("a");  // a is newer than b now
    cache.put("c", "3");  // full: b, the oldest, is dropped
    for (const auto& [key, value] : cache.entries) {
        print("{}={} ", key, value);
    }
    println("\n{}", (cache.get("b") ? "b kept" : "b evicted"));
    return 0;
}
```

Output:

```text
a=1 c=3 
b evicted
```

## See also

- [ordered_set](ordered_set.md): keys alone in the order of insertion
- [map](map.md): the same map without the order
- [sorted_map](sorted_map.md): the map in the order of the keys
- [concurrent::cache](../concurrent/cache.md): a cache shared by threads
- [string](string.md): a key looked up by a `string_view` or a literal
- [README: Containers](README.md#containers), [README: The rules](README.md#the-rules)
