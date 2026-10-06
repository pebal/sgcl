[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::name

```cpp
const string& name() const noexcept;
```

The path of the terminal end: `/dev/ttys003` on macOS, `/dev/pts/4` on Linux. A program on the terminal sees it
as its `tty`; [start](start.md) opens it in the child by this name. The path of the errors of the
pseudo-terminal's operations.

## Parameters

None.

## Return value

The path, for as long as the pseudo-terminal is held.

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
    io::command tty("tty");
    term.start(tty).value();
    string seen = term.read_all_text().value().trim();
    tty.wait().value();
    println("{}", seen == term.name());
}
```

Output:

```text
true
```

## See also

- [terminal](terminal.md): the terminal end itself
- [sgcl::io::pty](README.md)
