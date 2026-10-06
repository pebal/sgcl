[sgcl](../README.md) › [io](README.md)

# sgcl::io::set_terminal_size

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> set_terminal_size(int fd, const terminal_size& size) noexcept;
}
```

Sets the size of the terminal on the descriptor `fd` (`TIOCSWINSZ`); when it changes, the foreground process
group of the terminal's session gets `SIGWINCH`. What a terminal emulator does when its window is resized; for a
pseudo-terminal of the library, [pty::resize](pty/resize.md) says it on the pseudo-terminal itself.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | a descriptor of the terminal |
| `size` | the new size ([terminal_size](terminal_size.md)) |

## Return value

Nothing, or the [error](error/README.md), its operation `set_terminal_size`: `ENOTTY` when the descriptor is no
terminal, `EBADF` when it is not open.

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    io::set_terminal_size(term.terminal().fd(), {.rows = 60, .columns = 160}).value();
    println("{}", term.size().value().columns);
    println("{}", io::set_terminal_size(-1, {}).error().message());
}
```

Output:

```text
160
set_terminal_size: Bad file descriptor
```

## See also

- [get_terminal_size](get_terminal_size.md): reads it
- [pty::resize](pty/resize.md): of a pseudo-terminal
- [terminal_size](terminal_size.md)
