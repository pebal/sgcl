[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::operator==, operator!= (sgcl::set)

```cpp
friend bool operator==(const set& lhs, const set& rhs);
```

Compares two sets: equal when they have the same size and every element of `lhs` is found in `rhs` by its key
and compares equal to it with `==`, whatever the bucket counts and the orders of iteration. `!=` is the
compiler's rewrite of `==`. A hidden friend, found by the arguments' type. There is no ordering: the order of
iteration of a hash table is not a value.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the sets to compare |

## Return value

`true` when the sets hold equal elements, `false` otherwise.

## Complexity

Linear in the size on average, a lookup per element; quadratic when every key falls into one bucket.

## Exceptions

What the `==` of `Key` throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> a = {1, 2, 3};
    set<int> b(1000);
    b.insert({3, 2, 1});
    println("{} {}", a == b, a.bucket_count() == b.bucket_count());

    b.erase(3);
    println("{}", a != b);
}
```

Output:

```text
true false
true
```

## See also

- [count](count.md): the number of elements with a key
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
