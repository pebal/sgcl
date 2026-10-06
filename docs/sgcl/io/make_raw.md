[sgcl](../README.md) › [io](README.md)

# sgcl::io::make_raw

```cpp
#include "sgcl/io/terminal.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<terminal_mode, error> make_raw(int fd = 0) noexcept;
}
```

Puts the terminal on the descriptor `fd`, the standard input by default, in raw mode: the bytes as they are typed,
one at a time — no echo, no line editing, no signal from `^C` or `^Z` (they come as the bytes 3 and 26), no
translation of the line ends either way. The flags of `cfmakeraw`, with `VMIN` 1 and `VTIME` 0: Go's
`term.MakeRaw`. What an editor, a game or a reader of keys needs. The [terminal_mode](terminal_mode/README.md) returned
gives the terminal its modes back, by [restore](terminal_mode/restore.md) or at the end of its scope.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | a descriptor of the terminal; 0, the standard input, by default |

## Return value

The guard ([terminal_mode](terminal_mode/README.md)), or the [error](error/README.md), its operation `make_raw`: `ENOTTY`
when the descriptor is no terminal (the standard input redirected from a file or a pipe), `EBADF` when it is not
open.

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
    io::terminal_mode raw = io::make_raw(keyboard.fd()).value();
    term.write("\x03");  // ^C: a byte now, no signal
    byte key;
    keyboard.read(slice<byte>(&key, 1)).value();
    println("byte {}", int(key));
    println("{}", io::make_raw(-1).error().message());
}
```

Output:

```text
byte 3
make_raw: Bad file descriptor
```

## See also

- [terminal_mode](terminal_mode/README.md): the guard
- [disable_echo](disable_echo.md): the echo off, by lines
- [crypto::read_password](../crypto/read_password.md): a password, read into secret bytes
- [is_terminal](is_terminal.md): whether a descriptor is a terminal
