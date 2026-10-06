[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::write, async_write

```cpp
expected<size_t, error> write(const slice<const byte>& data) const;                                // (1)
async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const noexcept;    // (2)
```

Writes the whole of `data` to the master: what the program on the terminal reads, as if typed. The terminal's
line discipline works on it as on a keyboard's input: it echoes it (when the program has not turned the echo off),
a line reaches a program that reads lines only at its `"\n"`, and the control characters do what they do on a
terminal — `"\x03"` (`^C`) is a `SIGINT` to the foreground, `"\x04"` (`^D`) at the start of a line the end of the
program's input.

1. On the calling thread, which waits on the [reactor](../../async/readable.md) while the terminal's input is full.
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |

## Return value

The size of `data`. Or the [error](../error/README.md), its operation `write` and its path the terminal's
[name](name.md): `errc::closed` when the pseudo-terminal was closed, the `errno` of `write(2)` otherwise.

## Complexity

Linear in the size of `data`: a system call per piece the terminal takes.

## Exceptions

- (1) `std::system_error` when the write has to wait and the thread of the reactor cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    io::command head("head", "-n", "1");
    term.start(head).value();
    term.write("typed\n");
    string screen = term.read_all_text().value();
    print("{}", screen.replace("\r\n", "\n"));  // the terminal's echo, then what head wrote
    head.wait().value();
}
```

Output:

```text
typed
typed
```

## See also

- [read, async_read](read.md): the other direction
- [start](start.md): the program the bytes go to
- [sgcl::io::pty](README.md)
