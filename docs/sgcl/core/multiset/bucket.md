[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::bucket

```cpp
size_type bucket(const key_type& key) const noexcept;                                // (1)
template<class K> size_type bucket(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of the bucket the elements with the key `key` are in, or would be in: the hash of the key,
xored with a product of its bits above `bucket_count() - 1`, masked by `bucket_count() - 1`; 0 while the
multiset has no buckets. Equal elements are all in one bucket, adjacent.

- (2) The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../string/README.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key whose bucket to compute |

## Return value

The number of the bucket, below `bucket_count()`, or 0 when there are no buckets.

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) None when the call of `Hash` with a `K` is noexcept or it is a function object of `std`; otherwise what
  that call throws. `KeyEqual` is not called.

## Notes

A hash below the bucket count is its own bucket, since the product of no bits is 0: consecutive keys under
`std::hash` of an integer, which is the integer itself in the common libraries, stay in consecutive buckets and
are walked in order. Keys that differ only in the bits above the mask (multiples of a power of two, aligned
addresses), which the mask alone would put into one bucket, are spread by the product.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<string> s = {"a", "b", "b"};
    size_t n = s.bucket("b");
    size_t found = 0;
    for (auto it = s.begin(n); it != s.end(n); ++it) {
        found += *it == "b";
    }
    println("{} {}", n < s.bucket_count(), found);

    multiset<string> empty;
    println("{}", empty.bucket("b"));
}
```

Output:

```text
true 2
0
```

## See also

- [bucket_size](bucket_size.md): the number of elements in a bucket
- [begin, cbegin](begin.md): a local iterator to the first element of a bucket
- [hash_function](hash_function.md): the hash function
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
