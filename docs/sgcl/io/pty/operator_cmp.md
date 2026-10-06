[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::operator==, operator!= (sgcl::io::pty)

```cpp
friend bool operator==(const pty& a, const pty& b) noexcept;
```

Checks whether `a` and `b` are handles of the same pseudo-terminal: copies of one handle, not two opened by two
calls. Two handles that hold none are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same pseudo-terminal, or both none; `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty a = io::open_pty().value();
    io::pty b = a;
    io::pty c = io::open_pty().value();
    println("{} {}", a == b, a == c);
}
```

Output:

```text
true false
```

## See also

- [(constructor)](pty.md): a copy is the same pseudo-terminal
- [sgcl::io::pty](README.md)
