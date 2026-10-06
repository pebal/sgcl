[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the pseudo-terminal was closed ([close](close.md)), through this handle or a copy of it.

## Parameters

None.

## Return value

`true` after [close](close.md), `false` before.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    println("{}", term.is_closed());
    term.close();
    println("{}", term.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md): closes it
- [sgcl::io::pty](README.md)
