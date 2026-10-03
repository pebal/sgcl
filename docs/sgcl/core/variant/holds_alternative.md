[sgcl](../../README.md) › [core](../README.md) › [variant](../variant.md)

# sgcl::holds_alternative (sgcl::variant)

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class... Ts>
    bool holds_alternative(const variant<Ts...>& v) noexcept;
}
```

Checks whether `v` holds the alternative of type `T`. Takes part only when `T` is exactly one of `Ts`: a type that
is not an alternative, or is one twice, is ill-formed, as with `std`, and a requires-expression sees the call as
invalid.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the variant |

## Return value

`true` when `v.index()` is the index of `T`, `false` otherwise, a valueless `v` included.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    variant<int, tracked_ptr<Node>> v = 1;
    println("{} {}", holds_alternative<int>(v), holds_alternative<tracked_ptr<Node>>(v));
    v = make_tracked<Node>(2);
    println("{} {}", holds_alternative<int>(v), holds_alternative<tracked_ptr<Node>>(v));
}
```

Output:

```text
true false
false true
```

## See also

- [index](index.md): the index of the alternative held
- [get_if](get_if.md): a pointer to the alternative, null on another
- [sgcl::variant\<Ts...\>](../variant.md)
