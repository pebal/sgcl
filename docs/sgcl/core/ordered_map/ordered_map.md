[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::ordered_map

```cpp
/*(1)*/ ordered_map()
            noexcept(std::is_nothrow_default_constructible_v<hasher> &&
                     std::is_nothrow_default_constructible_v<key_equal>);
/*(2)*/ explicit ordered_map(size_type bucket_count, const hasher& hash = hasher(),
                             const key_equal& equal = key_equal())
            noexcept(std::is_nothrow_copy_constructible_v<hasher> &&
                     std::is_nothrow_copy_constructible_v<key_equal>);
/*(3)*/ template<std::input_iterator InputIt>
        ordered_map(InputIt first, InputIt last, size_type bucket_count = 0,
                    const hasher& hash = hasher(), const key_equal& equal = key_equal());
/*(4)*/ ordered_map(std::initializer_list<value_type> ilist, size_type bucket_count = 0,
                    const hasher& hash = hasher(), const key_equal& equal = key_equal());
/*(5)*/ ordered_map(const ordered_map& other);
/*(6)*/ ordered_map(ordered_map&& other)
            noexcept(std::is_nothrow_move_constructible_v<hasher> &&
                     std::is_nothrow_move_constructible_v<key_equal>);
```

Constructs a map from one of the sources below.

1. An empty map. Nothing is allocated: `bucket_count()` is 0.
2. An empty map with `bucket_count` buckets, rounded up to a power of two (0: no bucket array yet), and the
   given hash and equality.
3. The elements of the range `[first, last)`, in the order of the range: of two elements with one key the first
   is kept, with its value and its place. Given a forward range, the table is sized for the distance first.
4. The elements of `ilist`, as (3).
5. A copy of `other`: its elements in its order, its bucket count and its `max_load_factor`.
6. Takes the table of `other` over, the order with it; `other` is empty after, with no bucket array.

- (3–4) An element that is a `std::pair` with a first of the type `Key` is hashed and looked up where it is and
  copied once, into its node; a duplicate copies nothing. An element of any other type is converted to a
  `value_type` first, once.

- (2–4) A `bucket_count` the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
  allocation does ([collector](../collector.md#the-memory-limit)): a count past the largest array an address space
  holds is taken as that array, refused in the same way.

## Parameters

| Parameter | Description |
|---|---|
| `bucket_count` | the number of buckets to start with, rounded up to a power of two |
| `hash` | the hash function |
| `equal` | the equality of the keys |
| `first`, `last` | the range the elements are copied from |
| `ilist` | the list the elements are copied from |
| `other` | the map the elements are copied or taken from |

## Complexity

- (1) Constant.
- (2) Linear in `bucket_count`: the bucket array.
- (3) Linear in the distance between `first` and `last` on average.
- (4) Linear in the size of `ilist` on average.
- (5) Linear in `other.size()`: no lookup, every node is linked as it is copied.
- (6) Constant.

## Exceptions

- (1), (2), (6) What the construction of `Hash` and `KeyEqual` throws (their default constructors, their copies,
  their moves); none when it is noexcept.
- (3–5) What the construction of an element, or the copy of `Hash` or `KeyEqual`, throws.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates:
no map is constructed.

## Notes

A map made by (1), or by (2) with 0, has no bucket array and no sentinel until its first insertion (or a
[reserve](reserve.md), a [rehash](rehash.md)): its [end()](end.md) is null until then, and is not the `end()`
of the map once it holds an element.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> empty;
    ordered_map<string, int> sized(100);
    ordered_map<string, int> ages = {{"Grace", 85}, {"Ada", 36}, {"Grace", 0}};

    vector<pair<string, int>> letters = {{"z", 26}, {"a", 1}};
    ordered_map from_range(letters.begin(), letters.end());  // deduced: ordered_map<string, int>
    ordered_map copy = ages;
    ordered_map taken = std::move(ages);

    println("{} {} {}", empty.bucket_count(), sized.bucket_count(), from_range);
    println("{} {} {}", copy, taken, ages.size());
}
```

Output:

```text
0 128 {"z": 26, "a": 1}
{"Grace": 85, "Ada": 36} {"Grace": 85, "Ada": 36} 0
```

## See also

- [operator=](operator_assign.md): replaces the contents of a map
- [reserve](reserve.md): the buckets for the elements to come
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
