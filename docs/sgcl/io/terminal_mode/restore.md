[sgcl](../../README.md) › [io](../README.md) › [terminal_mode](README.md)

# sgcl::io::terminal_mode::restore

```cpp
expected<void, error> restore() noexcept;
```

Gives the terminal the modes it had before [make_raw](../make_raw.md) (`tcsetattr` with `TCSAFLUSH`: what was typed
in raw mode and not read is dropped, as a shell drops it when a program ends), and lets go of it: a second call, or
one of a guard that holds none, does nothing and succeeds. The destructor calls it when nothing did; `restore()`
is how its error is seen.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md), its operation `restore`, with the `errno` of `tcsetattr` (`EBADF` when
the descriptor was closed meanwhile).

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
    io::terminal_mode raw = io::make_raw(term.terminal().fd()).value();
    println("{}", raw.restore().has_value());
    println("{} {}", static_cast<bool>(raw), raw.restore().has_value());
}
```

Output:

```text
true
false true
```

## See also

- [make_raw](../make_raw.md): the raw mode
- [sgcl::io::terminal_mode](README.md)
