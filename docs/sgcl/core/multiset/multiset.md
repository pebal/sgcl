[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::multiset

```cpp
multiset() noexcept(std::is_nothrow_default_constructible_v<hasher> &&                   // (1)
                    std::is_nothrow_default_constructible_v<key_equal>);
explicit multiset(size_type bucket_count, const hasher& hash = hasher(),                 // (2)
                  const key_equal& equal = key_equal())
    noexcept(std::is_nothrow_copy_constructible_v<hasher> &&
             std::is_nothrow_copy_constructible_v<key_equal>);
template<std::input_iterator InputIt>
multiset(InputIt first, InputIt last, size_type bucket_count = 0,                        // (3)
         const hasher& hash = hasher(), const key_equal& equal = key_equal());
multiset(std::initializer_list<value_type> ilist, size_type bucket_count = 0,            // (4)
         const hasher& hash = hasher(), const key_equal& equal = key_equal());
multiset(const multiset& other);                                                         // (5)
multiset(multiset&& other) noexcept(std::is_nothrow_move_constructible_v<hasher> &&      // (6)
                                    std::is_nothrow_move_constructible_v<key_equal>);
```

Constructs a multiset from one of the sources below.

1. An empty multiset. Nothing is allocated: `bucket_count()` is 0 until the first insertion.
2. An empty multiset with `bucket_count` buckets, rounded up to a power of two (none for 0), and the given hash
   and equality.
3. The elements of the range `[first, last)`, every one kept, equal ones included. A forward range is counted
   first and the table sized for that many elements at once.
4. The elements of `ilist`, as (3).
5. A copy of `other`: its elements, its bucket count, its order and its `max_load_factor`, with copies of its
   hasher and its equality.
6. Takes the table of `other` over, with its hasher, its equality and its `max_load_factor`; `other` is left
   empty, without buckets.

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
| `other` | the multiset the elements are copied or taken from |

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

A copy (5) reproduces the chain of `other` node by node, so it iterates in the same order, runs of equal
elements included, and looks up no key. A move (6) touches no element: the iterators to the elements of `other`
point into the new multiset from then on.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<int> empty;
    multiset<int> sized(100);
    multiset rolls = {4, 2, 4, 6, 2};
    vector<string> words = {"b", "a", "b"};
    multiset from_range(words.begin(), words.end());  // deduced: multiset<string>
    multiset copy = rolls;
    multiset taken = std::move(rolls);

    println("{} {}", empty.size(), empty.bucket_count());
    println("{} {}", sized.size(), sized.bucket_count());
    println("{} {}", from_range.size(), from_range.count("b"));
    println("{} {} {}", copy.size(), copy == taken, rolls.size());
}
```

Output:

```text
0 0
0 128
3 2
5 true 0
```

## See also

- [operator=](operator_assign.md): replaces the elements of a multiset
- [reserve](reserve.md): the buckets for the elements to come
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
