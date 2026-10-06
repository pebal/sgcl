[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::resize

```cpp
expected<void, error> resize(const terminal_size& size) const noexcept;
```

Sets the size of the terminal (`TIOCSWINSZ` on the master): the rows and the columns a program on the terminal
lays its screen out in, and its pixels. When the size changes, the foreground process group of the terminal's
session gets `SIGWINCH`, which a shell, an editor or a pager answers by reading the new size and drawing again —
what a terminal emulator does when its window is resized, and what an SSH server does on the client's
`window-change`.

## Parameters

| Parameter | Description |
|---|---|
| `size` | the new size ([terminal_size](../terminal_size.md)) |

## Return value

Nothing, or the [error](../error/README.md), its operation `resize`: `errc::closed` when the pseudo-terminal was
closed, the `errno` of `ioctl(2)` otherwise.

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty({.rows = 24, .columns = 80}).value();
    io::command sh("/bin/sh", "-c", "trap 'stty size; exit' WINCH; echo ready; while :; do sleep 0.05; done");
    term.start(sh).value();
    byte line[16];
    term.read(line).value();  // "ready": the trap is set
    term.resize({.rows = 40, .columns = 120}).value();
    string screen = term.read_all_text().value();
    print("{}", screen.replace("\r\n", "\n"));
    sh.wait().value();
}
```

Output:

```text
40 120
```

## See also

- [size](size.md): the size it has
- [set_terminal_size](../set_terminal_size.md): the same on any terminal's descriptor
- [sgcl::io::pty](README.md)
