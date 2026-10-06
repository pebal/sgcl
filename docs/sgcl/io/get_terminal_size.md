[sgcl](../README.md) › [io](README.md)

# sgcl::io::get_terminal_size

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<terminal_size, error> get_terminal_size(int fd = 1) noexcept;
}
```

The size of the terminal on the descriptor `fd`, the standard output by default (`TIOCGWINSZ`): of the program's
own terminal through a standard stream (`io::get_terminal_size()`), of a pseudo-terminal through either of its ends. Go's `term.GetSize` of
`golang.org/x/term`.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | a descriptor of the terminal; 1, the standard output, by default |

## Return value

The size ([terminal_size](terminal_size.md)), or the [error](error/README.md), its operation
`get_terminal_size`: `ENOTTY` when the descriptor is no terminal (a standard stream redirected to a file or a
pipe), `EBADF` when it is not open.

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    if (auto size = io::get_terminal_size()) {
        println("{} columns", size->columns);
    } else {
        println("{}", size.error().message());
    }
}
```

## See also

- [set_terminal_size](set_terminal_size.md): sets it
- [size_changes](size_changes.md): told of every change
- [is_terminal](is_terminal.md): whether a descriptor is a terminal
- [terminal_size](terminal_size.md)
