[sgcl](../../README.md) › [core](../README.md) › [any](../any.md)

# sgcl::any_cast

```cpp
#include "sgcl/core/any.h"   // or "sgcl/core.h"

namespace sgcl {
    /*(1)*/ template<class T, class U = std::remove_cvref_t<T>>
            requires std::is_constructible_v<T, const U&>
            T any_cast(const any& a);
    /*(2)*/ template<class T, class U = std::remove_cvref_t<T>>
            requires std::is_constructible_v<T, U&>
            T any_cast(any& a);
    /*(3)*/ template<class T, class U = std::remove_cvref_t<T>>
            requires std::is_constructible_v<T, U>
            T any_cast(any&& a);
    /*(4)*/ template<class T> const T* any_cast(const any* a) noexcept;
    /*(5)*/ template<class T> T* any_cast(any* a) noexcept;
}
```

The value of an `any` as the type it holds, compared by `typeid` as `std::any_cast` compares it.

- (1–3) `U` is `T` without its reference and `const`: when `a` holds a `U`, the value as a `T` — a copy for a `T`
  that is not a reference, a reference into the `any` (or into its node) for `U&` or `const U&`, the value moved
  out for `U&&` and for a `U` from (3). Otherwise `bad_any_cast`.
- (4–5) A pointer to the value when `a` is not null and holds a `T`, null otherwise. `T` may not be `void`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the `any`, or a pointer to it |

## Return value

- (1–3) The value as a `T`.
- (4–5) A pointer to the value, or `nullptr`.

## Complexity

Constant: one comparison of `std::type_info`, and the copy or the move of the value for a `T` that is not a
reference.

## Exceptions

- (1–3) `bad_any_cast` when `a` holds no `U`, and what the copy or the move constructor of `U` throws.
- (4–5) None.

## Notes

A reference from (1–2) and a pointer from (4–5) into a value in a node are valid while the `any` holds that value:
an assignment, an `emplace` or a `reset` destroys it, as `std::any` destroys its own.

`bad_any_cast` is `std::bad_any_cast`: `catch (const std::bad_any_cast&)` catches it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    tracked_ptr node = make_tracked<Node>(1);
    any a = node;
    any b = 3;

    any_cast<tracked_ptr<Node>&>(a)->value = 2;  // a reference into a
    println("{} {}", node->value, any_cast<int>(b));

    int* p = any_cast<int>(&b);
    double* d = any_cast<double>(&b);
    println("{} {}", *p, d == nullptr);

    try {
        any_cast<string>(b);
    } catch (const bad_any_cast&) {
        println("not a string");
    }
}
```

Output:

```text
2 3
3 true
not a string
```

## See also

- [type](type.md): the type of the value held
- [has_value](has_value.md): checks whether the `any` holds a value
- [sgcl::any](../any.md)
