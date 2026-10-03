[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::operator==, operator!= (sgcl::immutable::vector)

```cpp
friend bool operator==(const vector& a, const vector& b);    // (1)
friend bool operator!=(const vector& a, const vector& b);    // (2)
```

Compares two vectors by their elements.

1. `true` when `a` and `b` have the same elements in the same order, whatever the two share. A version and its
   copy, which hold the same trie and tail, are equal at once, without a look at an element.
2. `!(a == b)`.

Both take part only when `T` is [req::equatable](../../core/req/equatable.md): a vector of elements without `==`
has no `==` either. `<`, `<=`, `>`, `>=` and `<=>` compare the elements lexicographically, from
[mixin::comparable](../../core/mixin/comparable.md), when `T` is
[req::comparable](../../core/req/comparable.md).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the vectors to compare |

## Return value

- (1) `true` when the vectors are equal, `false` otherwise.
- (2) `true` when they are not equal, `false` otherwise.

## Complexity

Constant when the sizes differ or the two hold the same trie and tail; otherwise linear in the size, at most one
comparison of elements per position.

## Exceptions

What the comparison of the elements throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x, y;  // no ==
};

int main() {
    immutable::vector<int> a = {1, 2, 3};
    immutable::vector<int> b = a.set(2, 4).set(2, 3);  // another trie, the same elements
    println("{} {} {}", a == b, a != a.pop_back(), a < a.set(2, 4));
    println("{}", req::equatable<immutable::vector<int>>);
    println("{}", req::equatable<immutable::vector<point>>);
}
```

Output:

```text
true true true
true
false
```

## See also

- [mixin::comparable](../../core/mixin/comparable.md): `<=>` by the elements
- [sgcl::immutable::vector\<T\>](../vector.md)
