[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [comparable](README.md)

# sgcl::mixin::operator\<=\>, operator\<, operator\<=, operator\>, operator\>= (sgcl::mixin::comparable)

```cpp
constexpr friend auto operator<=>(const Derived& a, const Derived& b);
```

Orders two values of `Derived` lexicographically by their elements, what `<=>` is on the standard sequences and
the ordered associative containers: the first pair of elements that are not equivalent decides, and when one
value runs out first, the shorter is less. `a < b`, `a <= b`, `a > b` and `a >= b` have no declarations of their
own: the compiler rewrites them through `<=>` (`a < b` as `(a <=> b) < 0`).

The elements are compared by their `<=>`, or, for elements that have only `<`, by a weak ordering built from it,
as the standard containers do (synth-three-way: `a < b` less, `b < a` greater, neither equivalent). The operator is
a friend of the base, found through `Derived` by the arguments, and takes part only when the elements are
`req::comparable`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values to compare |

## Return value

The ordering of `a` against `b`, of the type the elements' comparison gives: `std::strong_ordering` for `int`
elements, `std::partial_ordering` for `double`, `std::weak_ordering` for elements ordered by `<` alone.

## Complexity

Linear in the size of the shorter value: at most one comparison of elements per position.

## Exceptions

What the comparison of the elements throws, their `<=>` or their `<`.

## Notes

The operator is not noexcept, for the reason the `==` of [mixin::equatable](../equatable/operator_cmp.md) is not:
a value whose own comparison compares a container of itself would need its own exception specification to
compute this one's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <compare>
#include <type_traits>

using namespace sgcl;

struct less_only {
    int v;
    bool operator<(less_only o) const { return v < o.v; }
};

struct opaque {
    int v;
};

int main() {
    vector a = {1, 2}, b = {1, 3};
    println("{} {}", a < b, (a <=> b) < 0);

    // by < alone: the order is weak, as with std
    vector<less_only> x = {{1}}, y = {{2}};
    println("{} {}", x < y, std::is_same_v<decltype(x <=> y), std::weak_ordering>);

    println("{} {}", req::comparable<less_only>, req::comparable<std::pair<int, int>>);
    println("{}", req::comparable<opaque>);
}
```

Output:

```text
true true
true true
true true
false
```

## See also

- [operator==, operator!= (mixin::equatable)](../equatable/operator_cmp.md): the equality of two values by their
  elements
- [req::comparable](../../req/comparable.md): a value with an order
- [sgcl::mixin::comparable\<Derived\>](README.md)
