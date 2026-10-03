[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::variant\<Ts...\>::swap

```cpp
void swap(variant& o)
    noexcept(((std::is_nothrow_move_constructible_v<Ts> && std::is_nothrow_swappable_v<Ts>) && ...))
    requires ((std::is_move_constructible_v<Ts> && std::is_swappable_v<Ts>) && ...);
```

Swaps the contents of `*this` and `o`. When both hold the same alternative, the two are swapped with `swap`, found
as `using std::swap; swap(a, b)` finds it; when both are valueless, nothing happens; otherwise the two alternatives
change places by their move constructors, through a temporary variant.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the variant to swap with |

## Return value

None.

## Complexity

Constant, plus the swap or the moves of the alternatives.

## Exceptions

What the move constructor or the swap of an alternative throws; none when every alternative's are noexcept.

A move that throws while the alternatives change places may leave either variant valueless.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    variant<int, string> a = 1;
    variant<int, string> b = "two";
    a.swap(b);
    println("{} {}", get<string>(a), get<int>(b));
}
```

Output:

```text
two 1
```

## See also

- [swap](swap2.md): the same as a free function
- [sgcl::variant\<Ts...\>](../variant.md)
