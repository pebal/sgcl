[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::get (sgcl::variant)

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    /*(1)*/ template<size_t I, class... Ts>
            variant_alternative_t<I, variant<Ts...>>& get(variant<Ts...>& v);
    /*(2)*/ template<size_t I, class... Ts>
            const variant_alternative_t<I, variant<Ts...>>& get(const variant<Ts...>& v);
    /*(3)*/ template<size_t I, class... Ts>
            variant_alternative_t<I, variant<Ts...>>&& get(variant<Ts...>&& v);
    /*(4)*/ template<size_t I, class... Ts>
            const variant_alternative_t<I, variant<Ts...>>&& get(const variant<Ts...>&& v);
    /*(5)*/ template<class T, class... Ts> T& get(variant<Ts...>& v);
    /*(6)*/ template<class T, class... Ts> const T& get(const variant<Ts...>& v);
    /*(7)*/ template<class T, class... Ts> T&& get(variant<Ts...>&& v);
    /*(8)*/ template<class T, class... Ts> const T&& get(const variant<Ts...>&& v);
}
```

The alternative `v` holds, when it is the one asked for.

- (1–4) The alternative at index `I`. An `I` past the alternatives is ill-formed.
- (5–8) The alternative of type `T`. A `T` that is not exactly one of `Ts` is ill-formed.

When `v` holds another alternative or is valueless, `bad_variant_access`.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the variant |

## Return value

A reference to the alternative: an lvalue reference for (1–2) and (5–6), an rvalue reference for (3–4) and (7–8).

## Complexity

Constant.

## Exceptions

`bad_variant_access` when `v.index()` is not the index asked for.

## Notes

The reference is valid while the variant holds that alternative: an assignment of another alternative or an
`emplace` destroys it. [get_if](get_if.md) asks without the exception.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    variant<int, tracked_ptr<Node>> v = make_tracked<Node>(1);
    get<1>(v)->value = 2;
    println("{}", get<tracked_ptr<Node>>(v)->value);

    try {
        get<int>(v);
    } catch (const bad_variant_access&) {
        println("not an int");
    }
}
```

Output:

```text
2
not an int
```

## See also

- [get_if](get_if.md): a pointer to the alternative, null on another
- [visit](visit.md): a function called with whichever alternative is held
- [sgcl::variant\<Ts...\>](../variant.md)
