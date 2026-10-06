[sgcl](../README.md) › [io](README.md)

# sgcl::io::open_pty

```cpp
#include "sgcl/io/pty.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<pty, error> open_pty(const terminal_size& size = {}) noexcept;
}
```

A new pseudo-terminal of the size given, 24 rows of 80 columns by default: the master from `posix_openpt`,
`grantpt` and `unlockpt`, non-blocking and served by the [reactor](../async/readable.md); the terminal end opened
by its name (`ptsname_r`), never as this process's controlling terminal (`O_NOCTTY`), and kept for
[start](pty/start.md). Both ends are closed in the children the program starts (`O_CLOEXEC`), but for the child
[start](pty/start.md) puts on the terminal. Go's `pty.Open` of `creack/pty`.

## Parameters

| Parameter | Description |
|---|---|
| `size` | the size of the terminal ([terminal_size](terminal_size.md)) |

## Return value

The pseudo-terminal ([pty](pty/README.md)), or the [error](error/README.md), its operation `open_pty`, `grantpt`,
`unlockpt`, `ptsname`, `open` or `resize`, with the `errno` of the call that failed (`EAGAIN` when the system has
no pseudo-terminal left, `EMFILE` when the process has no descriptor left).

## Complexity

Constant: a few system calls.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty({.rows = 50, .columns = 132}).value();
    io::command sh("/bin/sh", "-c", "stty size");
    term.start(sh).value();
    print("{}", term.read_all_text().value().replace("\r\n", "\n"));
    sh.wait().value();
}
```

Output:

```text
50 132
```

## See also

- [pty](pty/README.md): the pseudo-terminal
- [terminal_size](terminal_size.md): its size
