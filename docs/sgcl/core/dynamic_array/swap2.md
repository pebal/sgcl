[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](README.md)

# sgcl::swap (sgcl::dynamic_array)

```cpp
friend void swap(dynamic_array& l, dynamic_array& r) noexcept;
```

Exchanges the contents of `l` and `r`: `l.swap(r)` ([swap](swap.md)), the buffers and the counts, no element
touched. A hidden friend, found by the arguments' type: `swap(a, b)` written without a namespace, and
`std::ranges::swap(a, b)`.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the arrays to exchange the contents of |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<string> a = {"x"};
    dynamic_array<string> b = {"y", "z"};
    swap(a, b);
    println("{} {}", a, b);
}
```

Output:

```text
["y", "z"] ["x"]
```

## See also

- [swap](swap.md): the member form
- [sgcl::dynamic_array\<T\>](README.md)
