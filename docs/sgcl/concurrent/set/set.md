[sgcl](../../README.md) › [concurrent](../README.md) › [set](README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::set

```cpp
set()                                                                                      // (1)
    noexcept(std::is_nothrow_default_constructible_v<hasher> &&
             std::is_nothrow_default_constructible_v<key_equal> &&
             std::is_nothrow_copy_constructible_v<hasher> &&
             std::is_nothrow_copy_constructible_v<key_equal>);
explicit set(size_type buckets, const hasher& hash = hasher(),                             // (2)
             const key_equal& equal = key_equal())
    noexcept(std::is_nothrow_copy_constructible_v<hasher> &&
             std::is_nothrow_copy_constructible_v<key_equal>);
template<std::input_iterator InputIt>
set(InputIt first, InputIt last, size_type buckets = 16, const hasher& hash = hasher(),    // (3)
    const key_equal& equal = key_equal());
set(std::initializer_list<value_type> ilist, size_type buckets = 16,                       // (4)
    const hasher& hash = hasher(), const key_equal& equal = key_equal());
set(const set&) = delete;                                                                  // (5)
```

Constructs a set from one of the sources below.

1. An empty set with 16 buckets.
2. An empty set with `buckets` buckets, rounded up to a power of two (2 at least), and the given hash and
   equality.
3. The keys of the range `[first, last)`, of two equal ones the first kept, as [insert](insert.md) keeps it.
4. The keys of `ilist`, as (3).
5. The set is not copyable, and not movable: a structure shared by threads has one place.

- (3–4) The list is built at once, nobody sharing it yet: the keys are taken into a buffer and sorted by their
  split keys, the array is given as many buckets as the growth would reach for them (and `buckets` at least), and
  the list is linked in one pass, the dummies of the buckets in use merged in at their keys, a plain store each
  and no search.

## Parameters

| Parameter | Description |
|---|---|
| `buckets` | the number of buckets to start with, rounded up to a power of two |
| `hash` | the hash function |
| `equal` | the equality of the keys |
| `first`, `last` | the range the keys are copied from |
| `ilist` | the list the keys are copied from |

## Complexity

- (1–2) Linear in the number of buckets: two allocations, the head node and the array.
- (3–4) Linear in the number of keys, plus the sort of their split keys: for the map's table, 90 to 120 ns per
  element for 200,000 random keys, against 140 to 270 for an insertion per element into an empty one.

## Exceptions

- (1–2) What the construction of `Hash` and `KeyEqual` (their default constructors, the copy of `hash` and
  `equal`) throws; none when it is noexcept.
- (3–4) What the copy of a key, of `hash` or of `equal` throws.

If an exception is thrown, no set is constructed; the nodes made so far are left to the collector.

## Notes

The default of `buckets`, 16, is a constant of the class. The array grows by itself once the keys outnumber the
buckets; (2) with the number of keys expected spares the doublings on the way, as [reserve](reserve.md) does later.
A count of buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../../core/collector/README.md#the-memory-limit)): a count past the largest array an address
space holds is taken as that array, refused in the same way.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Crawler {
    concurrent::set<string> visited;  // a member of a managed object
};

int main() {
    concurrent::set<int> empty;  // on the stack
    concurrent::set<string> sized(1 << 16);
    concurrent::set<string> listed = {"a", "b", "a"};

    vector<int> ids;
    for (int i : range(100)) {
        ids.push_back(i * 3);
    }
    concurrent::set<int> from_range(ids.begin(), ids.end());
    tracked_ptr crawler = make_tracked<Crawler>();

    println("{} {} {}", empty.size(), empty.bucket_count(), crawler->visited.empty());
    println("{}", sized.bucket_count());
    println("{} {}", listed.size(), listed.contains("b"));
    println("{} {}", from_range.size(), from_range.bucket_count());
    println("{}", std::is_copy_constructible_v<concurrent::set<int>>);
}
```

Output:

```text
0 16 true
65536
2 true
100 128
false
```

## See also

- [insert](insert.md): inserts keys into a set that threads share
- [reserve](reserve.md): the buckets for the keys to come
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](README.md)
