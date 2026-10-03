[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::operator==, operator!= (sgcl::immutable::list)

```cpp
friend bool operator==(const list& a, const list& b);    // (1)
friend bool operator!=(const list& a, const list& b);    // (2)
```

Compares two lists by their elements.

1. `true` when `a` and `b` have the same elements in the same order, whatever the two share. Two lists that hold
   the same first cell (a version and its copy) are equal at once, without a look at an element.
2. `!(a == b)`.

Both take part only when `T` is [req::equatable](../../core/req/equatable.md). `<`, `<=`, `>`, `>=` and `<=>`
compare the elements lexicographically, from [mixin::comparable](../../core/mixin/comparable.md), when `T` is
[req::comparable](../../core/req/comparable.md).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the lists to compare |

## Return value

- (1) `true` when the lists are equal, `false` otherwise.
- (2) `true` when they are not equal, `false` otherwise.

## Complexity

Constant when the two hold the same first cell or their sizes differ; otherwise linear in the size, at most one
comparison of elements per position.

## Exceptions

What the comparison of the elements throws.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<int> a = {1, 2, 3};
    immutable::list<int> b = {1, 2, 3};  // other cells, the same elements
    println("{} {} {}", a == b, a.push_front(0).pop_front() == a, a != a.pop_front());
    println("{}", a < immutable::list<int>{1, 3});
}
```

Output:

```text
true true true
true
```

## See also

- [mixin::comparable](../../core/mixin/comparable.md): `<=>` by the elements
- [sgcl::immutable::list\<T\>](../list.md)
