[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::size

```cpp
expected<terminal_size, error> size() const noexcept;
```

The size of the terminal (`TIOCGWINSZ` on the master): the one [open_pty](../open_pty.md) or [resize](resize.md)
set, or the one a program on the terminal set itself (`stty rows 50`).

## Parameters

None.

## Return value

The size ([terminal_size](../terminal_size.md)), or the [error](../error/README.md), its operation `size`:
`errc::closed` when the pseudo-terminal was closed, the `errno` of `ioctl(2)` otherwise.

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty({.rows = 30, .columns = 100}).value();
    io::terminal_size size = term.size().value();
    println("{} rows, {} columns", size.rows, size.columns);
}
```

Output:

```text
30 rows, 100 columns
```

## See also

- [resize](resize.md): sets it
- [get_terminal_size](../get_terminal_size.md): the size of any terminal
- [sgcl::io::pty](README.md)
