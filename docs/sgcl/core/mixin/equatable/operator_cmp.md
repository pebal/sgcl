[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [equatable](README.md)

# sgcl::mixin::operator==, operator!= (sgcl::mixin::equatable)

```cpp
constexpr friend bool operator==(const Derived& a, const Derived& b);
```

Compares two values of `Derived` by their elements: equal when they hold as many elements, equal in the same
order, what `==` is on every standard container. `a != b` has no declaration of its own: the compiler rewrites
it as `!(a == b)`.

The operator is a friend of the base, found through `Derived` by the arguments, and takes part only when the
elements are `req::equatable`: a `vector<point>` for a `point` without `==` has no `==` either, though it carries
the mixin.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values to compare |

## Return value

`true` when `a` and `b` have the same number of elements and each is equal to the one at its position in the
other, `false` otherwise; `a != b` the opposite.

## Complexity

Linear in the size: at most one comparison of elements per position, none when the sizes differ and the
container knows its size (a `vector`, a `deque`, a `list`; not a `forward_list`, which is walked).

## Exceptions

What the `==` of the elements throws.

## Notes

The operator is not noexcept, even for elements whose `==` is, as the `==` of the standard containers is not: a
value whose defaulted `==` compares a container of itself (a tree whose node holds a `vector` of nodes) would need
its own exception specification to compute this one's, and would not compile.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x, y;
};

int main() {
    vector a = {1, 2}, b = {1, 2}, c = {2, 1};
    println("{} {}", a == b, a != c);

    list<int> l = {1, 2}, m = {1, 2};
    println("{}", l == m);

    // point has no ==, so a vector of points has none either
    println("{} {}", req::equatable<point>, req::equatable<vector<point>>);
}
```

Output:

```text
true true
true
false false
```

## See also

- [operator\<=\> (mixin::comparable)](../comparable/operator_cmp.md): the order of two values by their elements
- [req::equatable](../../req/equatable.md): a value with `==`
- [sgcl::mixin::equatable\<Derived\>](README.md)
