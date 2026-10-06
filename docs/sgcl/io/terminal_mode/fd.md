[sgcl](../../README.md) › [io](../README.md) › [terminal_mode](README.md)

# sgcl::io::terminal_mode::fd

```cpp
int fd() const noexcept;
```

The descriptor of the terminal the guard restores; `-1` when it holds none (made by the default constructor, moved
from, or restored).

## Parameters

None.

## Return value

The descriptor, or `-1`.

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
    println("{}", raw.fd() == term.terminal().fd());
    raw.restore();
    println("{}", raw.fd());
}
```

Output:

```text
true
-1
```

## See also

- [operator bool](operator_bool.md): whether it holds one
- [sgcl::io::terminal_mode](README.md)
