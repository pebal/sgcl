[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::fd

```cpp
int fd() const noexcept;
```

The descriptor of the master, for a call of the system the library does not make. The pseudo-terminal keeps
owning it: the descriptor is not closed by the caller, and not used after [close](close.md).

## Parameters

None.

## Return value

The descriptor, or `-1` once the pseudo-terminal is closed.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty({.rows = 12, .columns = 34}).value();
    println("{}", io::get_terminal_size(term.fd()).value().columns);
    term.close();
    println("{}", term.fd());
}
```

Output:

```text
34
-1
```

## See also

- [terminal](terminal.md): the other end
- [sgcl::io::pty](README.md)
