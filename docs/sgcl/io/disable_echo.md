[sgcl](../README.md) › [io](README.md)

# sgcl::io::disable_echo

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<terminal_mode, error> disable_echo(int fd = 0) noexcept;
}
```

Turns the echo of the terminal on the descriptor `fd` off, the standard input by default, and keeps it a terminal of
lines: what is typed is not shown, a line reaches a read at its Enter (`ICANON`), `^C` keeps its signal (`ISIG`) and a
`"\r"` ends a line as a `"\n"` does (`ICRNL`). What a password or a passphrase is read under;
[crypto::read_password](../crypto/read_password.md) reads one this way into secret bytes, where a string would leave
it in managed memory, never zeroed. The [terminal_mode](terminal_mode/README.md) returned gives the modes back, by
[restore](terminal_mode/restore.md) or at the end of its scope.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | a descriptor of the terminal; 0, the standard input, by default |

## Return value

The guard ([terminal_mode](terminal_mode/README.md)), or the [error](error/README.md), its operation `disable_echo`:
`ENOTTY` when the descriptor is no terminal, `EBADF` when it is not open.

## Complexity

Constant: two system calls.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    io::file keyboard = term.terminal();
    {
        io::terminal_mode quiet = io::disable_echo(keyboard.fd()).value();
        term.write("secret\n");  // typed: not shown
        byte line[32];
        size_t n = keyboard.read(line).value();
        println("read {} bytes", n);
    }
    term.write("shown\n");  // the echo back
    byte screen[64];
    size_t n = term.read(screen).value();
    print("{}", string(slice<const byte>(screen, n)).replace("\r\n", "\n"));
}
```

Output:

```text
read 7 bytes
shown
```

## See also

- [make_raw](make_raw.md): every key as it comes
- [crypto::read_password](../crypto/read_password.md): a password read under it
- [terminal_mode](terminal_mode/README.md)
