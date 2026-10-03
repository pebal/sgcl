[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::operator==, operator!= (sgcl::multiset)

```cpp
friend bool operator==(const multiset& lhs, const multiset& rhs);
```

Compares two multisets: equal when they have the same size and, for every run of equal elements in `lhs`, `rhs`
has a run of the same key and the same length holding the same elements by `==`, in some order; whatever the
bucket counts and the orders of iteration. `!=` is the compiler's rewrite of `==`. A hidden friend, found by the
arguments' type. There is no ordering: the order of iteration of a hash table is not a value.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the multisets to compare |

## Return value

`true` when the multisets hold equal elements as often each, `false` otherwise.

## Complexity

Linear in the size on average, a lookup per run, plus the square of the length of each run; quadratic in the
size when every key falls into one bucket.

## Exceptions

What the `==` of `Key` throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<int> a = {1, 1, 2};
    multiset<int> b(1000);
    b.insert({2, 1, 1});
    println("{}", a == b);

    b.erase(b.find(1));
    b.insert(2);
    println("{} {}", a != b, a.size() == b.size());
}
```

Output:

```text
true
true true
```

## See also

- [count](count.md): the number of elements with a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
