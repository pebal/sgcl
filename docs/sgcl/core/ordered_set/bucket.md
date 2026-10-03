[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::bucket

```cpp
size_type bucket(const key_type& key) const noexcept;                                // (1)
template<class K> size_type bucket(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of the bucket `key` falls into, whether or not the key is there: its hash, xored with a
product of the hash's bits above `bucket_count() - 1`, masked by it. Keys below the bucket count keep
consecutive buckets, as an integer's hash of itself gives them, and keys a power of two apart (strides, aligned
pointers), which the mask alone would put into one bucket, are spread. A set with no bucket array gives 0.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both
   declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to place |

## Return value

The number of the bucket.

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) None when the call of `Hash` with a `K` is noexcept or it is a function object of `std`; otherwise what
  that call throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {1, 2, 3};
    println("{} {}", s.bucket_count(), s.bucket(2));

    size_t n = s.bucket(2);
    size_t in_bucket = 0;
    for (auto it = s.begin(n); it != s.end(n); ++it) {
        ++in_bucket;  // every element here falls into bucket n
    }
    println("{}", in_bucket == s.bucket_size(n));
}
```

Output:

```text
4 2
true
```

## See also

- [bucket_size](bucket_size.md): the number of elements in a bucket
- [begin, cbegin](begin.md): a local iterator to the first element of a bucket
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
