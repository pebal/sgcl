[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::map

```cpp
map() noexcept(std::is_nothrow_default_constructible_v<hasher> &&                              // (1)
               std::is_nothrow_default_constructible_v<key_equal>);
explicit map(size_type bucket_count, const hasher& hash = hasher(),                            // (2)
             const key_equal& equal = key_equal())
    noexcept(std::is_nothrow_copy_constructible_v<hasher> &&
             std::is_nothrow_copy_constructible_v<key_equal>);
template<std::input_iterator InputIt>
map(InputIt first, InputIt last, size_type bucket_count = 0, const hasher& hash = hasher(),    // (3)
    const key_equal& equal = key_equal());
map(std::initializer_list<value_type> ilist, size_type bucket_count = 0,                       // (4)
    const hasher& hash = hasher(), const key_equal& equal = key_equal());
map(const map& other);                                                                         // (5)
map(map&& other) noexcept(std::is_nothrow_move_constructible_v<hasher> &&                      // (6)
                          std::is_nothrow_move_constructible_v<key_equal>);
```

Constructs a map from one of the sources below.

1. An empty map. Nothing is allocated: `bucket_count()` is 0 until the first insertion.
2. An empty map with `bucket_count` buckets, rounded up to a power of two (none for 0), and the given hash and
   equality.
3. The elements of the range `[first, last)`; of several elements with one key the first is kept, as
   [insert](insert.md) keeps it. A forward range is counted first and the table sized for that many elements at
   once.
4. The elements of `ilist`, as (3).
5. A copy of `other`: its elements, its bucket count, its order and its `max_load_factor`, with copies of its
   hasher and its equality.
6. Takes the table of `other` over, with its hasher, its equality and its `max_load_factor`; `other` is left
   empty, without buckets.

- (3–4) An element of the range that is a `std::pair<Key, U>` is hashed and looked up where it is and copied
  once, into its node, and a duplicate copies nothing; an element of any other type is converted to a
  `value_type` first, once.

- (2–4) A `bucket_count` the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
  allocation does ([collector](../collector/README.md#the-memory-limit)): a count past the largest array an address space
  holds is taken as that array, refused in the same way.

## Parameters

| Parameter | Description |
|---|---|
| `bucket_count` | the number of buckets to start with, rounded up to a power of two |
| `hash` | the hash function |
| `equal` | the equality of the keys |
| `first`, `last` | the range the elements are made from |
| `ilist` | the list the elements are copied from |
| `other` | the map the elements are copied or taken from |

## Complexity

- (1) Constant.
- (2) Linear in `bucket_count`.
- (3) Linear in the distance between `first` and `last` on average.
- (4) Linear in `ilist.size()` on average.
- (5) Linear in `other.size()` and `other.bucket_count()`.
- (6) Constant.

## Exceptions

- (1–2) What the construction of `Hash` and `KeyEqual` (their default constructors, the copy of `hash` and
  `equal`) throws; none when it is noexcept.
- (3–4) What the construction of an element from `*first` (the copy of an element of `ilist`), or the copy of
  `hash` or `equal`, throws.
- (5) What the copy of an element, of `Hash` or of `KeyEqual` throws.
- (6) What the move of `Hash` or of `KeyEqual` throws; none when it is noexcept.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates.

## Notes

A copy (5) reproduces the chain of `other` node by node, so it iterates in the same order and looks up no key. A
move (6) touches no element: the iterators to the elements of `other` point into the new map from then on.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> empty;
    map<int, int> sized(100);
    map<string, int> ages = {{"ann", 31}, {"bob", 27}, {"ann", 40}};
    vector<pair<int, int>> src = {{2, 20}, {1, 10}};
    map from_range(src.begin(), src.end());  // deduced: map<int, int>
    map from_list = {pair{1, 2.5}};  // deduced: map<int, double>
    map copy = ages;
    map taken = std::move(ages);

    println("{} {}", empty.size(), empty.bucket_count());
    println("{} {}", sized.size(), sized.bucket_count());
    println("{} {} {}", from_range.size(), from_range.at(1), from_list.at(1));
    println("{} {}", copy.size(), copy.at("ann"));
    println("{} {} {}", taken.size(), copy.bucket_count() == taken.bucket_count(), ages.size());
}
```

Output:

```text
0 0
0 128
2 10 2.5
2 31
2 true 0
```

## See also

- [operator=](operator_assign.md): replaces the elements of a map
- [reserve](reserve.md): the buckets for the elements to come
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
