[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::bucket

```cpp
size_type bucket(const key_type& key) const noexcept;                                // (1)
template<class K> size_type bucket(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of the bucket `key` falls into, whether or not an element is under it: the hash, xored with a
product of its bits above `bucket_count() - 1` and masked by it. 0 while the map has no buckets.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to place |

## Return value

The number of the key's bucket, below `bucket_count()`, or 0 with no buckets.

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) None when the call of `Hash` with a `K` is noexcept or it is a function object of `std`; otherwise what
  that call throws. `KeyEqual` is not called.

## Notes

The product spreads keys that the mask alone would put into one bucket: keys a power of two apart (strides,
aligned pointers), which agree in their low bits. A key below the bucket count is its own bucket (the product of
0 is 0), so consecutive integers, which `std::hash` gives as themselves (in libc++ and libstdc++), keep
consecutive buckets and their nodes are walked in order.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, int> m;
    println("{}", m.bucket(5));

    m.rehash(64);
    println("{} {} {}", m.bucket(0), m.bucket(5), m.bucket(63));

    // multiples of 64 agree in the bits under the mask, yet spread
    set<size_t> buckets;
    for (int i : range(1, 9)) {
        buckets.insert(m.bucket(i * 64));
    }
    println("{}", buckets.size() > 1);
}
```

Output:

```text
0
0 5 63
true
```

## See also

- [bucket_size](bucket_size.md): the number of elements in a bucket
- [begin, cbegin](begin.md): the local iterators of a bucket
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
