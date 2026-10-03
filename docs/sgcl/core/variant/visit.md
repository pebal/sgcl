[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::visit (sgcl::variant)

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    /*(1)*/ template<class F, class... Vs>
            decltype(auto) visit(F&& f, Vs&&... vs);
    /*(2)*/ template<class R, class F, class... Vs>
            R visit(F&& f, Vs&&... vs);
}
```

Calls `f` with the alternatives the variants `vs...` hold, one of each, through `std::invoke`: `f` may be a callable
or a pointer to a member. Each alternative is passed as the variant is: an lvalue reference from an lvalue variant,
an rvalue reference from an rvalue, `const` from a `const` one.

1. The result is what `f` returns. `f` must return the same type for every combination of the alternatives, as
   `std::visit` asks; a visitor that does not takes no part (a constraint, so that a requires-expression sees it).
2. Every result of `f` is converted to `R`; `R` may be `void`.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function called with the alternatives |
| `vs` | the variants whose alternatives are passed |

## Return value

- (1) What `f` returns.
- (2) What `f` returns, converted to `R`.

## Complexity

Constant for one variant: one indirect call through a table of the alternatives. For several, one such call per
variant.

## Exceptions

`bad_variant_access` when one of `vs` is valueless, and what `f` throws.

## Notes

Over several variants, `f` is called with the alternatives in the order of `vs`; the variants may be of different
types.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

struct Describe {
    void operator()(int n) const {
        println("an int, {}", n);
    }
    void operator()(const tracked_ptr<Node>& n) const {
        println("a node, {}", n->value);
    }
};

int main() {
    variant<int, tracked_ptr<Node>> a = 3;
    variant<int, tracked_ptr<Node>> b = make_tracked<Node>(1);
    visit(Describe(), a);
    visit(Describe(), b);

    variant<int, double> x = 2, y = 0.5;
    auto product = visit<double>([](auto l, auto r) { return l * r; }, x, y);  // int or double
    auto sum = visit([](auto l, auto r) -> double { return l + r; }, x, y);
    println("{} {}", product, sum);
}
```

Output:

```text
an int, 3
a node, 1
1 2.5
```

## See also

- [get](get.md), [get_if](get_if.md): one alternative, asked for by index or by type
- [sgcl::variant\<Ts...\>](../variant.md)
