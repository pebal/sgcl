[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](README.md)

# sgcl::mixin::enumerable\<Derived\>::exists

```cpp
template<class Pred> constexpr bool exists(Pred pred) const noexcept(/* see below */);
```

Checks whether `pred` accepts some element, calling it on the elements from the first on and stopping at the
first it accepts. Takes part only when `pred` is a predicate callable with a const element; it asks nothing of the
element, so it is there where `contains` is not: on a range of elements without `==`.

## Parameters

| Parameter | Description |
|---|---|
| `pred` | the predicate, called with a const element and returning what converts to `bool` |

## Return value

`true` when `pred` accepts an element, `false` otherwise, and on an empty range.

## Complexity

Linear in the size of the range: at most one call of `pred` per element.

## Exceptions

None when the copy of `pred` and its call on a const element are noexcept; otherwise what they throw.

## Notes

`exists` is `std::ranges::any_of`, as a member.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x, y;  // no ==
};

int main() {
    vector v = {5, 3, 9, 3};
    println("{} {}", v.exists([](int x) { return x == 9; }), v.exists([](int x) { return x < 0; }));

    vector<point> pts = {{1, 2}, {3, 0}};
    println("{}", pts.exists([](const point& p) { return p.y == 0; }));
    println("{}", vector<int>().exists([](int) { return true; }));
}
```

Output:

```text
true false
true
false
```

## See also

- [all](all.md): checks whether a predicate accepts every element
- [count_of](count_of.md): the number of elements a predicate accepts
- [contains](contains.md): checks whether an element is equal to a value
- [sgcl::mixin::enumerable\<Derived\>](README.md)
