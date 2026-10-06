[sgcl](../../README.md) › [io](../README.md) › [terminal_mode](README.md)

# sgcl::io::terminal_mode::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the guard holds a terminal to restore: `false` for one made by the default constructor, moved from,
or restored.

## Parameters

None.

## Return value

`true` when a terminal waits to be restored by it.

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
    io::terminal_mode raw = io::make_raw(term.terminal().fd()).value();
    println("{}", static_cast<bool>(raw));
    raw.restore();
    println("{}", static_cast<bool>(raw));
}
```

Output:

```text
true
false
```

## See also

- [fd](fd.md): the descriptor
- [sgcl::io::terminal_mode](README.md)
