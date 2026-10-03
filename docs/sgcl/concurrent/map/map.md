[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::map

```cpp
map()                                                                                      // (1)
    noexcept(std::is_nothrow_default_constructible_v<hasher> &&
             std::is_nothrow_default_constructible_v<key_equal> &&
             std::is_nothrow_copy_constructible_v<hasher> &&
             std::is_nothrow_copy_constructible_v<key_equal>);
explicit map(size_type buckets, const hasher& hash = hasher(),                             // (2)
             const key_equal& equal = key_equal())
    noexcept(std::is_nothrow_copy_constructible_v<hasher> &&
             std::is_nothrow_copy_constructible_v<key_equal>);
template<std::input_iterator InputIt>
map(InputIt first, InputIt last, size_type buckets = 16, const hasher& hash = hasher(),    // (3)
    const key_equal& equal = key_equal());
map(std::initializer_list<value_type> ilist, size_type buckets = 16,                       // (4)
    const hasher& hash = hasher(), const key_equal& equal = key_equal());
map(const map&) = delete;                                                                  // (5)
```

Constructs a map from one of the sources below.

1. An empty map with 16 buckets.
2. An empty map with `buckets` buckets, rounded up to a power of two (2 at least), and the given hash and
   equality.
3. The elements of the range `[first, last)`, of two with one key the first kept, as [insert](insert.md) keeps
   it.
4. The elements of `ilist`, as (3).
5. The map is not copyable, and not movable: a structure shared by threads has one place.

- (3–4) The list is built at once, nobody sharing it yet: the elements are taken into a buffer and sorted by their
  split keys, the array is given as many buckets as the growth would reach for them (and `buckets` at least), and
  the list is linked in one pass, the dummies of the buckets in use merged in at their keys, a plain store each
  and no search.

## Parameters

| Parameter | Description |
|---|---|
| `buckets` | the number of buckets to start with, rounded up to a power of two |
| `hash` | the hash function |
| `equal` | the equality of the keys |
| `first`, `last` | the range the elements are copied from |
| `ilist` | the list the elements are copied from |

## Complexity

- (1–2) Linear in the number of buckets: two allocations, the head node and the array.
- (3–4) Linear in the number of elements, plus the sort of their split keys: 90 to 120 ns per element for
  200,000 random keys, against 140 to 270 for an [insert](insert.md) per element into an empty map.

## Exceptions

- (1–2) What the construction of `Hash` and `KeyEqual` (their default constructors, the copy of `hash` and
  `equal`) throws; none when it is noexcept.
- (3–4) What the copy of an element, of `hash` or of `equal` throws.

If an exception is thrown, no map is constructed; the nodes made so far are left to the collector.

## Notes

The default of `buckets`, 16, is a constant of the class. The array grows by itself once the elements outnumber the
buckets; (2) with the number of elements expected spares the doublings on the way, as [reserve](reserve.md) does
later. A count of buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../../core/collector.md#the-memory-limit)): a count past the largest array an address
space holds is taken as that array, refused in the same way.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Registry {
    concurrent::map<int, string> names;  // a member of a managed object
};

int main() {
    concurrent::map<int, string> empty;  // on the stack
    concurrent::map<string, int> sized(1 << 16);
    concurrent::map<int, string> listed = {{1, "one"}, {2, "two"}, {1, "uno"}};

    vector<pair<int, int>> squares;
    for (int i : range(100)) {
        squares.push_back({i, i * i});
    }
    concurrent::map<int, int> from_range(squares.begin(), squares.end());
    tracked_ptr registry = make_tracked<Registry>();

    println("{} {} {}", empty.size(), empty.bucket_count(), registry->names.empty());
    println("{}", sized.bucket_count());
    println("{} {}", listed.size(), listed.value_or(1, ""));
    println("{} {}", from_range.size(), from_range.bucket_count());
    println("{}", std::is_copy_constructible_v<concurrent::map<int, int>>);
}
```

Output:

```text
0 16 true
65536
2 one
100 128
false
```

## See also

- [insert](insert.md), [try_emplace](try_emplace.md): insert elements into a map that threads share
- [reserve](reserve.md): the buckets for the elements to come
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
