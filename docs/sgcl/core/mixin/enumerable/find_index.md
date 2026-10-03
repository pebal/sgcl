[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](README.md)

# sgcl::mixin::enumerable\<Derived\>::find_index

```cpp
template<class Pred> constexpr size_t find_index(Pred pred) const noexcept(/* see below */);
```

Finds the position of the first element `pred` accepts, calling it on the elements from the first on. Takes part
only when `pred` is a predicate callable with a const element; it asks nothing of the element, which needs no
`==`.

## Parameters

| Parameter | Description |
|---|---|
| `pred` | the predicate, called with a const element and returning what converts to `bool` |

## Return value

The position of the first element `pred` accepts, `npos` when it accepts none.

## Complexity

Linear in the size of the range: at most one call of `pred` per element.

## Exceptions

None when the copy of `pred` and its call on a const element are noexcept; otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    println("{}", v.find_index([](int x) { return x > 4; }));
    println("{}", v.find_index([](int x) { return x > 6; }));
    println("{}", v.find_index([](int x) { return x > 9; }) == npos);
}
```

Output:

```text
0
2
true
```

## See also

- [find_if](find_if.md): a pointer to the first element a predicate accepts
- [index_of](index_of.md): the position of the first element equal to a value
- [sgcl::mixin::enumerable\<Derived\>](README.md)
