[sgcl](../../README.md) › [core](../README.md) › [variant](README.md)

# sgcl::swap (sgcl::variant)

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class... Ts>
    requires ((std::is_move_constructible_v<Ts> && std::is_swappable_v<Ts>) && ...)
    void swap(variant<Ts...>& l, variant<Ts...>& r) noexcept(noexcept(l.swap(r)));
}
```

Swaps the contents of `l` and `r`: `l.swap(r)` ([swap](swap.md)).

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the variants to swap |

## Return value

None.

## Complexity

Constant, plus the swap or the moves of the alternatives.

## Exceptions

What the move constructor or the swap of an alternative throws; none when every alternative's are noexcept.

## Notes

Found by the argument's type: `swap(a, b)` written without a namespace, and the `using std::swap; swap(a, b);` of
generic code, call it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    variant<int, double> a = 1;
    variant<int, double> b = 2.5;
    swap(a, b);
    println("{} {}", get<double>(a), get<int>(b));
}
```

Output:

```text
2.5 1
```

## See also

- [swap](swap.md): the member function
- [sgcl::variant\<Ts...\>](README.md)
