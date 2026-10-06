[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::terminal

```cpp
io::file terminal() const noexcept;
```

The terminal end, the program's own copy of it, opened by [open_pty](../open_pty.md): a [file](../file/README.md)
that is a terminal ([is_terminal](../is_terminal.md)), for a program that runs something on it other than by
[start](start.md), or that reads and writes it as the program on the terminal would. [start](start.md) closes it
once the child holds the terminal; [close](close.md) closes it with the master.

## Parameters

None.

## Return value

The terminal end, a handle of the file the pseudo-terminal keeps (closed after [start](start.md)).

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
    io::file end = term.terminal();
    println("{}", io::is_terminal(end.fd()));
    end.write("from the terminal end\n");
    byte chunk[64];
    size_t n = term.read(chunk).value();
    print("{}", string(slice<const byte>(chunk, n)).replace("\r\n", "\n"));
}
```

Output:

```text
true
from the terminal end
```

## See also

- [name](name.md): its path
- [start](start.md): closes it
- [sgcl::io::pty](README.md)
