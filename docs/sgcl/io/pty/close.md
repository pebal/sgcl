[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::close

```cpp
expected<void, error> close() const noexcept;
```

Closes the pseudo-terminal: the master, and the program's copy of the terminal end when [start](start.md) has
not closed it. A task or a thread waiting in [read](read.md) or [write](write.md) wakes to `errc::closed`. With
the master gone the terminal hangs up: the session on it, a child that [start](start.md) made, gets `SIGHUP`,
which ends a shell and what it runs, as closing the window of a terminal emulator does. A second `close` does
nothing and succeeds.

A pseudo-terminal that is not closed keeps its descriptors until the collector finds it dead — later than the
last use.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md) of `close(2)`.

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
    io::command sh("/bin/sh", "-c", "echo ready; sleep 30");
    term.start(sh).value();
    byte line[16];
    term.read(line).value();
    term.close().value();
    (void)sh.wait();
    println("{}", sh.state->to_string());
}
```

Output:

```text
signal: hangup
```

## See also

- [is_closed](is_closed.md): whether it was closed
- [sgcl::io::pty](README.md)
