[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::get_if (sgcl::variant)

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    template<size_t I, class... Ts>
    std::add_pointer_t<variant_alternative_t<I, variant<Ts...>>>
    get_if(variant<Ts...>* v) noexcept;                                      // (1)
    template<size_t I, class... Ts>
    std::add_pointer_t<const variant_alternative_t<I, variant<Ts...>>>
    get_if(const variant<Ts...>* v) noexcept;                                // (2)
    template<class T, class... Ts>
    std::add_pointer_t<T> get_if(variant<Ts...>* v) noexcept;                // (3)
    template<class T, class... Ts>
    std::add_pointer_t<const T> get_if(const variant<Ts...>* v) noexcept;    // (4)
}
```

A pointer to the alternative `*v` holds, when it is the one asked for; null when it is another, when the variant is
valueless, or when `v` is null.

- (1–2) The alternative at index `I`. An `I` past the alternatives is ill-formed.
- (3–4) The alternative of type `T`. A `T` that is not exactly one of `Ts` is ill-formed.

## Parameters

| Parameter | Description |
|---|---|
| `v` | a pointer to the variant |

## Return value

A pointer to the alternative, or `nullptr`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The pointer is valid while the variant holds that alternative. It is a raw pointer into the variant: it keeps
nothing alive, and to the collector it is what any pointer into an object is.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

void describe(const variant<int, string>& v) {
    if (const int* n = get_if<int>(&v)) {
        println("an int, {}", *n);
    } else if (const string* s = get_if<1>(&v)) {
        println("a string, {}", *s);
    }
}

int main() {
    describe(42);
    describe("text");
}
```

Output:

```text
an int, 42
a string, text
```

## See also

- [get](get.md): a reference, `bad_variant_access` on another
- [holds_alternative](holds_alternative.md): checks for an alternative by its type
- [sgcl::variant\<Ts...\>](../variant.md)
